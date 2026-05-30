/**
 * @file xservice_proxy_client.cpp
 * @brief 服务代理客户端实现
 * 
 * 实现服务代理客户端的消息转发和回调管理
 */
#include "xservice_proxy_client.h"
#include "xtools.h"
#include "xauth_proxy.h"
#include "xlog_client.h"
using namespace std;

/**
 * @brief 创建服务代理客户端
 * @param service_name 服务名称
 * @return 服务代理客户端指针
 * 
 * 根据服务名称创建对应类型的代理客户端
 */
XServiceProxyClient* XServiceProxyClient::Create(std::string service_name)
{
    if (service_name == AUTH_NAME)
    {
        return new XAuthProxy();
    }
    return new XServiceProxyClient();
}

/**
 * @brief 发送消息
 * @param head 消息头
 * @param msg 消息体
 * @param ev 消息事件对象
 * @return 发送成功返回true
 */
bool XServiceProxyClient::SendMsg(xmsg::XMsgHead *head, XMsg *msg, XMsgEvent *ev)
{
    RegEvent(ev);
    head->set_msg_id((long long)ev);
    return XMsgEvent::SendMsg(head, msg);
}

/**
 * @brief 删除事件回调
 * @param ev 消息事件对象
 */
void XServiceProxyClient::DelEvent(XMsgEvent *ev)
{
    XMutex mux(&callback_task_mutex_);
    callback_task_.erase((long long)ev);
}

/**
 * @brief 注册一个事件
 * @param ev 消息事件对象
 */
void XServiceProxyClient::RegEvent(XMsgEvent *ev)
{
    XMutex mux(&callback_task_mutex_);
    callback_task_[(long long)ev] = ev;
}

/**
 * @brief 消息读取回调函数
 * @param head 消息头
 * @param msg 消息体
 * 
 * 接收服务端响应消息，转发给对应的XRouterHandle
 */
void XServiceProxyClient::ReadCB(xmsg::XMsgHead *head, XMsg *msg)
{
    if (!head || !msg) return;

    cout << "***************************************" << endl;
    cout << head->DebugString();
    
    // 转发给XRouterHandle
    // 每个XServiceProxyClient可能对应多个XRouterHandle
    auto router = callback_task_.find(head->msg_id());
    if (router == callback_task_.end())
    {
        LOGDEBUG("callback_task_ can't find");
        return; 
    }
    // 是否线程安全？通常通过，需要验证
    router->second->SendMsg(head, msg);
}

/**
 * @brief XServiceProxyClient构造函数
 */
XServiceProxyClient::XServiceProxyClient()
{
}

/**
 * @brief XServiceProxyClient析构函数
 */
XServiceProxyClient::~XServiceProxyClient()
{
}
