/**
 * @file xregister_handle.cpp
 * @brief 注册中心消息处理实现
 * 
 * 处理服务注册、服务发现请求，维护服务列表缓存
 */
#include "xregister_handle.h"
#include "xtools.h"
#include "xmsg_com.pb.h"
#include "xlog_client.h"

using namespace xmsg;
using namespace std;

// 注册服务列表的缓存（全局变量）
static XServiceMap *service_map = nullptr;

// 多线程访问服务列表的互斥锁
static mutex service_map_mutex;

/**
 * @brief 处理服务发现请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 根据请求类型返回全部服务列表或指定服务的列表
 */
void XRegisterHandle::GetServiceReq(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("接收服务的发现请求");
    
    // 请求对象
    XGetServiceReq req;
    
    // 响应对象（默认设置为错误）
    xmsg::XServiceMap res;
    res.mutable_res()->set_return_(XMessageRes_XReturn_ERROR);

    // 解析请求消息
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        stringstream ss;
        ss << "req.ParseFromArray failed!";
        LOGINFO(ss.str().c_str());
        res.mutable_res()->set_msg(ss.str().c_str());
        SendMsg(MSG_GET_SERVICE_RES, &res);
        return;
    }

    string service_name = req.name();
    stringstream ss;
    ss << "GetServiceReq : service name " << service_name;
    LOGDEBUG(ss.str().c_str());

    XServiceMap *send_map = &res;

    // 获取服务列表（加锁保护）
    service_map_mutex.lock();

    // 初始化服务列表（如果尚未初始化）
    if (!service_map)
        service_map = new XServiceMap();

    // 根据请求类型返回服务列表
    if (req.type() == XServiceType::ALL)
    {
        // 返回全部服务
        send_map = service_map;
        send_map->set_type(XServiceType::ALL);
    }
    else
    {
        // 返回指定服务
        auto smap = service_map->mutable_service_map();
        if (smap && smap->find(service_name) != smap->end())
        {
            (*send_map->mutable_service_map())[service_name] = (*smap)[service_name];
        }
    }
    service_map_mutex.unlock();

    LOGDEBUG(send_map->DebugString());

    // 设置响应类型和状态
    send_map->set_type(req.type());
    send_map->mutable_res()->set_return_(XMessageRes_XReturn_OK);
    SendMsg(MSG_GET_SERVICE_RES, send_map);
}

/**
 * @brief 处理服务注册请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析服务注册信息，验证后添加到服务列表缓存
 */
void XRegisterHandle::RegisterReq(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("服务端接收到用户的注册请求");

    // 响应消息对象
    XMessageRes res;
    
    // 解析请求消息
    XServiceInfo req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGERROR("XRegisterReq ParseFromArray failed!");
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("XRegisterReq ParseFromArray failed!");
        SendMsg(MSG_REGISTER_RES, &res);
        return;
    }
    
    // 验证服务名称
    string service_name = req.name();
    if (service_name.empty())
    {
        string error = "service_name is empty!";
        LOGERROR(error.c_str());
        res.set_return_(XMessageRes::ERROR);
        res.set_msg(error);
        SendMsg(MSG_REGISTER_RES, &res);
        return;
    }

    // 获取服务IP（如果为空则使用客户端IP）
    string service_ip = req.ip();
    if (service_ip.empty())
    {
        LOGERROR("service_ip is empty, using client ip");
        service_ip = this->client_ip();
    }

    // 验证服务端口
    int service_port = req.port();
    if (service_port <= 0 || service_port > 65535)
    {
        stringstream ss;
        ss << "service_port is error! " << service_port;
        LOGERROR(ss.str().c_str());
        res.set_return_(XMessageRes::ERROR);
        res.set_msg(ss.str());
        SendMsg(MSG_REGISTER_RES, &res);
        return;
    }
    
    // 注册信息验证通过
    stringstream ss;
    ss << "接收到用户注册信息: " << service_name << "|" << service_ip << ":" << service_port;
    LOGINFO(ss.str().c_str());

    // 存储服务注册信息（加锁保护）
    {
        XMutex mutex(&service_map_mutex);
        
        // 初始化服务列表（如果尚未初始化）
        if (!service_map)
            service_map = new XServiceMap();
        
        // 获取服务映射表
        auto smap = service_map->mutable_service_map();

        // 查找是否已有同类型服务注册
        auto service_list = smap->find(service_name);
        if (service_list == smap->end())
        {
            // 创建新的服务列表
            (*smap)[service_name] = XServiceList();
            service_list = smap->find(service_name);
        }

        // 检查是否已存在相同IP和端口的服务
        auto services = service_list->second.mutable_service();
        for (auto service : (*services))
        {
            if (service.ip() == service_ip && service.port() == service_port)
            {
                stringstream ss;
                ss << service_name << "|" << service_ip << ":" << service_port << " 微服务已经注册过";
                LOGERROR(ss.str().c_str());
                res.set_return_(XMessageRes::ERROR);
                res.set_msg(ss.str());
                SendMsg(MSG_REGISTER_RES, &res);
                return;
            }
        }

        // 添加新的微服务
        auto ser = service_list->second.add_service();
        ser->set_ip(service_ip);
        ser->set_port(service_port);
        ser->set_name(service_name);
        ser->set_is_find(req.is_find());
        
        stringstream ss_info;
        ss_info << service_name << "|" << service_ip << ":" << service_port << " 新的微服务注册成功！";
        LOGINFO(ss_info.str().c_str());
    }

    // 返回成功响应
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    SendMsg(MSG_REGISTER_RES, &res);
}

/**
 * @brief XRegisterHandle构造函数
 */
XRegisterHandle::XRegisterHandle()
{
}

/**
 * @brief XRegisterHandle析构函数
 */
XRegisterHandle::~XRegisterHandle()
{
}
