/**
 * @file xconfig_manager.cpp
 * @brief 配置管理器实现
 * 
 * 提供配置的管理功能，包括获取配置列表、删除配置、发送配置等
 */
#include "xconfig_manager.h"
#include "xtools.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>
#include <map>
#include <thread>
#include <chrono>
#include <fstream>
using namespace std;
using namespace google;
using namespace protobuf;
using namespace compiler;
using namespace xmsg;

// 存储获取的配置列表
static XConfigList *all_config = 0;
static mutex all_config_mutex;

/**
 * @brief 发送删除配置请求
 * @param ip 服务IP地址
 * @param port 服务端口
 */
void XConfigManager::DeleteConfig(const char *ip, int port)
{
    if (!ip || strlen(ip) == 0 || port < 0 || port > 65535)
    {
        LOGDEBUG("DeleteConfig failed! port or ip error");
        return;
    }
    XLoadConfigReq req;
    req.set_service_ip(ip);
    req.set_service_port(port);
    // 发送消息到服务端
    SendMsg(MSG_DEL_CONFIG_REQ, &req);
}

/**
 * @brief 处理删除配置响应
 * @param head 消息头
 * @param msg 消息体
 */
void XConfigManager::DeleteConfigRes(xmsg::XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("ParseFromArray failed!");
        return;
    }
    if (res.return_() == XMessageRes::OK)
    {
        LOGDEBUG("删除配置成功!");
        return;
    }
    LOGDEBUG("删除配置失败");
}

/**
 * @brief 设置登录信息
 * @param login 登录响应信息
 */
void XConfigManager::set_login(xmsg::XLoginRes login)
{
    XMutex mux(&login_mutex_);
    this->login_ = login;
}

/**
 * @brief 发送消息（带登录信息）
 * @param type 消息类型
 * @param message 消息体
 * @return 发送成功返回true
 */
bool XConfigManager::SendMsg(xmsg::MsgType type, const google::protobuf::Message *message)
{
    XMsgHead head;
    head.set_msg_type(type);
    if (!login_.token().empty())
    {
        XMutex mux(&login_mutex_);
        head.set_token(login_.token());
        head.set_username(login_.username());
        head.set_rolename(login_.rolename());
    }

    head.set_service_name(CONFIG_NAME);
    return XMsgEvent::SendMsg(&head, message);
}

/**
 * @brief 处理获取配置列表响应
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析配置列表响应，存储到本地缓存
 */
void XConfigManager::LoadAllConfigRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("响应获取配置列表 ");
    XMutex mux(&all_config_mutex);
    if (!all_config)
        all_config = new XConfigList();
    all_config->ParseFromArray(msg->data, msg->size);
}

/**
 * @brief 获取全部配置列表（阻塞函数）
 * @param page 页码
 * @param page_count 每页数量
 * @param timeout_sec 超时时间（秒）
 * @return 配置列表
 * 
 * 执行流程：
 * 1. 断开连接自动重连
 * 2. 发送获取配置列表消息
 * 3. 等待结果返回
 */
xmsg::XConfigList XConfigManager::GetAllConfig(int page, int page_count, int timeout_sec)
{
    // 清理历史数据
    {
        XMutex mux(&all_config_mutex);
        delete all_config;
        all_config = NULL;
    }
    XConfigList confs;
    
    // 步骤1：断开连接自动重连
    if (!AutoConnect(timeout_sec))
        return confs;

    // 步骤2：发送获取配置列表的消息
    XLoadAllConfigReq req;
    req.set_page(page);
    req.set_page_count(page_count);
    SendMsg(MSG_LOAD_ALL_CONFIG_REQ, &req);

    // 步骤3：等待结果返回（每10毫秒检查一次）
    int count = timeout_sec * 100;
    for (int i = 0; i < count; i++)
    {
        {
            XMutex mux(&all_config_mutex);
            if (all_config)
            {
                return *all_config;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    return confs;
}

/**
 * @brief 处理保存配置响应
 * @param head 消息头
 * @param msg 消息体
 */
void XConfigManager::SendConfigRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("接收到上传配置的反馈");
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("ParseFromArray failed!");
        if (SendConfigResCB)
            SendConfigResCB(false, "ParseFromArray failed!");
        return;
    }
    if (res.return_() == XMessageRes::OK)
    {
        LOGDEBUG("上传配置成功!");
        if (SendConfigResCB)
            SendConfigResCB(true, u8"上传配置成功!");
        return;
    }
    stringstream ss;
    ss << u8"上传配置失败:" << res.msg();
    if (SendConfigResCB)
        SendConfigResCB(false, ss.str().c_str());
    LOGDEBUG(ss.str().c_str());
}

/**
 * @brief 发送配置到配置中心
 * @param conf 配置信息
 */
void XConfigManager::SendConfig(xmsg::XConfig *conf)
{
    LOGDEBUG("发送配置");
    SendMsg(MSG_SAVE_CONFIG_REQ, conf);
}

/**
 * @brief 发送获取配置请求
 * @param ip 服务IP地址（NULL表示使用客户端连接地址）
 * @param port 服务端口
 */
void XConfigManager::LoadConfig(const char *ip, int port)
{
    LOGDEBUG("获取配置请求");
    if (port < 0 || port > 65535)
    {
        LOGDEBUG("LoadConfig failed! port error");
        return;
    }
    XLoadConfigReq req;
    if (ip)  // IP如果为NULL则取连接配置中心的地址
        req.set_service_ip(ip);
    req.set_service_port(port);
    // 发送消息到服务端
    SendMsg(MSG_LOAD_CONFIG_REQ, &req);
}

/**
 * @brief 处理获取配置响应
 * @param head 消息头
 * @param msg 消息体
 */
void XConfigManager::LoadConfigRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("获取配置响应");
    XConfig config;
    if (!config.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("LoadConfigRes conf.ParseFromArray failed!");
        return;
    }
    LOGDEBUG(config.DebugString().c_str());
    if (LoadConfigResCB)
    {
        LoadConfigResCB(config);
    }
}

/**
 * @brief XConfigManager构造函数
 */
XConfigManager::XConfigManager()
{
    RegMsgCallback();
}

/**
 * @brief XConfigManager析构函数
 */
XConfigManager::~XConfigManager()
{
}
