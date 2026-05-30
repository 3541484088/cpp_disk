/**
 * @file xregister_client.cpp
 * @brief 注册中心客户端实现
 * 
 * 实现与注册中心的通信，包括服务注册、服务发现、心跳管理等功能
 */
#include "xregister_client.h"
#include "xlog_client.h"
#include "xmsg_com.pb.h"
#include "xtools.h"
#include <thread>
#include <fstream>

using namespace xmsg;
using namespace std;

// 注册服务列表的缓存（全局变量）
static XServiceMap *service_map = nullptr;
static XServiceMap *client_map = nullptr;

// 多线程访问服务列表的互斥锁
static mutex service_map_mutex;

/**
 * @brief 发送获取微服务列表请求
 * @param service_name 服务名称（NULL表示获取全部）
 * 
 * 根据参数决定获取指定服务或全部服务列表
 */
void XRegisterClient::GetServiceReq(const char *service_name)
{
    LOGDEBUG("GetServiceReq");
    
    XGetServiceReq req;
    if (service_name)
    {
        // 获取指定服务
        req.set_type(XServiceType::ONE);
        req.set_name(service_name);
    }
    else
    {
        // 获取全部服务
        req.set_type(XServiceType::ALL);
    }

    SendMsg(MSG_GET_SERVICE_REQ, &req);
}

/**
 * @brief 从本地缓存文件加载服务列表
 * @return 加载成功返回true，失败返回false
 * 
 * 从本地.cache文件读取之前缓存的服务列表数据
 */
bool XRegisterClient::LoadLocalFile()
{
    // 初始化服务列表
    if (!service_map)
    {
        service_map = new XServiceMap();
    }
    
    LOGDEBUG("Load local register data");
    
    // 构建缓存文件名
    stringstream ss;
    ss << "register_" << service_name_ << service_ip_ << service_port_ << ".cache";
    
    // 打开缓存文件
    ifstream ifs;
    ifs.open(ss.str(), ios::binary);
    if (!ifs.is_open())
    {
        stringstream log;
        log << "Load local register data failed! ";
        log << ss.str();
        LOGDEBUG(log.str().c_str());
        return false;
    }
    
    // 解析protobuf数据
    service_map->ParseFromIstream(&ifs);
    ifs.close();
    return true;
}

/**
 * @brief 获取指定服务名称的微服务列表（阻塞函数）
 * @param service_name 服务名称
 * @param timeout_sec 超时时间（秒）
 * @return 服务列表
 * 
 * 执行流程：
 * 1. 等待连接成功
 * 2. 发送获取微服务的消息
 * 3. 等待微服务列表消息反馈（有可能拿到上一次的配置）
 */
xmsg::XServiceList XRegisterClient::GetServcies(const char *service_name, int timeout_sec)
{
    xmsg::XServiceList re;
    
    // 计算总循环次数（每10毫秒检查一次）
    int total_count = timeout_sec * 100;
    int count = 0;

    // 步骤1：等待连接成功
    while (count < total_count)
    {
        if (is_connected())
            break;
        this_thread::sleep_for(chrono::milliseconds(10));
        count++;
    }

    // 连接超时，尝试读取本地缓存
    if (!is_connected())
    {
        LOGDEBUG("连接等待超时");
        XMutex mutex(&service_map_mutex);
        if (!service_map)
        {
            LoadLocalFile();
        }
        return re;
    }

    // 步骤2：发送获取微服务的消息
    GetServiceReq(service_name);
    
    // 步骤3：等待微服务列表消息反馈
    while (count < total_count)
    {
        XMutex mutex(&service_map_mutex);
        
        if (!service_map)
        {
            this_thread::sleep_for(chrono::milliseconds(10));
            count++;
            continue;
        }

        auto m = service_map->mutable_service_map();
        if (!m)
        {
            // 服务映射为空，重新请求
            GetServiceReq(service_name);
            this_thread::sleep_for(chrono::milliseconds(100));
            count += 10;
            continue;
        }

        auto s = m->find(service_name);
        if (s == m->end())
        {
            // 未找到指定服务，重新请求
            GetServiceReq(service_name);
            this_thread::sleep_for(chrono::milliseconds(100));
            count += 10;
            continue;
        }

        // 找到服务，复制并返回
        re.CopyFrom(s->second);
        return re;
    }

    return re;
}

/**
 * @brief 获取所有服务列表
 * @return 服务列表指针
 * 
 * 返回缓存中所有注册的服务列表（线程安全）
 */
xmsg::XServiceMap *XRegisterClient::GetAllService()
{
    XMutex mutex(&service_map_mutex);
    
    if (!service_map)
    {
        return nullptr;
    }
    
    if (!client_map)
    {
        client_map = new XServiceMap();
    }
    
    client_map->CopyFrom(*service_map);
    return client_map;
}

/**
 * @brief 处理获取服务列表响应
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析服务列表响应，更新内存缓存和磁盘缓存
 */
void XRegisterClient::GetServiceRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("GetServiceRes");

    XMutex mutex(&service_map_mutex);
    
    // 是否替换全部缓存
    bool is_all = false;
    XServiceMap *cache_map;
    XServiceMap tmp;
    cache_map = &tmp;

    // 初始化服务列表（如果尚未初始化）
    if (!service_map)
    {
        service_map = new XServiceMap();
        cache_map = service_map;
        is_all = true;
    }

    // 解析响应消息
    if (!cache_map->ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("service_map.ParseFromArray failed!");
        return;
    }
    
    // 判断是否为全部服务列表
    if (cache_map->type() == XServiceType::ALL)
    {
        is_all = true;
    }

    // 刷新内存缓存
    if (cache_map != service_map)
    {
        if (is_all)
        {
            // 替换全部缓存
            service_map->CopyFrom(*cache_map);
        }
        else
        {
            // 只更新指定服务
            auto cmap = cache_map->mutable_service_map();
            if (!cmap || cmap->empty()) return;
            
            auto one = cmap->begin();
            auto smap = service_map->mutable_service_map();
            (*smap)[one->first] = one->second;
        }
    }

    // 刷新磁盘缓存
    stringstream ss;
    ss << "register_" << service_name_ << service_ip_ << service_port_ << ".cache";
    LOGDEBUG("Save local file!");
    
    if (!service_map) return;

    ofstream ofs;
    ofs.open(ss.str(), ios::binary);
    if (!ofs.is_open())
    {
        LOGDEBUG("save local file failed!");
        return;
    }
    
    // 序列化到文件（缓存需设定有效期）
    service_map->SerializePartialToOstream(&ofs);
    ofs.close();
}

/**
 * @brief 处理服务注册响应
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析注册响应，记录注册结果日志
 */
void XRegisterClient::RegisterRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("RegisterRes");
    
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("XRegisterClient::RegisterRes failed! res.ParseFromArray failed!");
        return;
    }
    
    if (res.return_() == XMessageRes::OK)
    {
        LOGINFO("RegisterRes success");
        return;
    }
    
    stringstream ss;
    ss << "RegisterRes failed! " << res.msg();
    LOGINFO(ss.str().c_str());
}

/**
 * @brief 连接成功回调函数
 * 
 * 连接到注册中心成功后，发送服务注册消息
 */
void XRegisterClient::ConnectedCB()
{
    LOGDEBUG("connected start send MSG_REGISTER_REQ");
    
    XServiceInfo req;
    req.set_name(service_name_);
    req.set_ip(service_ip_);
    req.set_port(service_port_);
    req.set_is_find(is_find_);
    SendMsg(MSG_REGISTER_REQ, &req);
}

/**
 * @brief 定时器回调函数（心跳发送）
 * 
 * 定时发送心跳消息到注册中心，保持连接活跃
 */
void XRegisterClient::TimerCB()
{
    static long long count = 0;
    count++;
    
    XMsgHeart req;
    req.set_count(count);
    SendMsg(MSG_HEART_REQ, &req);
}

/**
 * @brief 向注册中心注册服务
 * @param service_name 微服务名称
 * @param port 微服务端口
 * @param ip 微服务IP（NULL表示采用客户端连接地址）
 * @param is_find 是否可被发现
 * 
 * 初始化客户端，连接注册中心并注册服务
 */
void XRegisterClient::RegisterServer(const char *service_name, int port, const char *ip, bool is_find)
{
    is_find_ = is_find;
    
    // 注册消息回调函数
    RegMsgCallback();

    // 设置服务信息
    if (service_name)
        strcpy(service_name_, service_name);
    if (ip)
        strcpy(service_ip_, ip);
    service_port_ = port;

    // 设置自动重连
    set_auto_connect(true);

    // 设置心跳定时器（3秒）
    set_timer_ms(3000);

    // 设置默认注册中心地址
    if (server_ip()[0] == '\0')
    {
        set_server_ip("127.0.0.1");
    }
    if (server_port() <= 0)
    {
        set_server_port(REGISTER_PORT);
    }

    // 启动连接
    StartConnect();

    // 加载本地缓存
    LoadLocalFile();
}

/**
 * @brief XRegisterClient构造函数
 */
XRegisterClient::XRegisterClient()
{
}

/**
 * @brief XRegisterClient析构函数
 */
XRegisterClient::~XRegisterClient()
{
}
