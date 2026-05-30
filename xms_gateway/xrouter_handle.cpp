/**
 * @file xrouter_handle.cpp
 * @brief 路由消息处理实现
 * 
 * 处理API网关的路由消息，包括鉴权验证和消息转发
 */
#include "xrouter_handle.h"
#include "xtools.h"
#include "xservice_proxy.h"
#include "xauth_proxy.h"
#include "xlog_client.h"
#include <list>
#include <thread>
using namespace std;
using namespace xmsg;

/**
 * @brief 初始化消息服务器
 */
void XRouterHandle::InitMsgServer()
{
}

/**
 * @brief 关闭消息服务器
 */
void XRouterHandle::CloseMsgServer()
{
}

/**
 * @brief 发送消息
 * @param head 消息头
 * @param msg 消息体
 * @return 发送成功返回true
 */
bool XRouterHandle::SendMsg(xmsg::XMsgHead *head, XMsg *msg)
{
    bool re = XMsgEvent::SendMsg(head, msg);
    if (re)
        cout << "消息已回复" << head->DebugString() << endl;
    else
        cout << "消息回复异常" << head->DebugString() << endl;
    return re;
}

/**
 * @brief 消息读取回调函数
 * @param head 消息头
 * @param msg 消息体
 * 
 * 处理流程：
 * 1. 验证token有效性
 * 2. 鉴权成功后转发消息到目标服务
 */
void XRouterHandle::ReadCB(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("XRouterHandle::ReadCB");
    static int i = 0;
    i++;
    cout << i << "XRouterHandle::ReadCB" << head->DebugString();

    // 获取token信息
    string token = head->token();
    string user = head->username();
    
    // 如果是CheckToken消息，空包msg也有对象，size=0
    // 验证token是否有效，是否与用户一致
    
    // 此函数由线程池调用，如果阻塞会影响同线程任务
    // 验证消息权限
    
    // 鉴权成功再发送消息
    if (!head)
        return;

    // 排除不需要鉴权的消息，需要优化
    if (head->msg_type() != MSG_LOGIN_REQ && 
        !XAuthProxy::CheckToken(head))
    {
        LOGINFO(head->DebugString());
        // 鉴权失败，暂不处理，需要重构优化回复内容
        return;
    }

    // 设置消息ID用于回调
    head->set_msg_id((long long)this);
    
    // 转发消息到服务代理
    XServiceProxy::Get()->SendMsg(head, msg, this);
}

/**
 * @brief 关闭连接
 */
void XRouterHandle::Close()
{
    XServiceProxy::Get()->DelEvent(this);
    XMsgEvent::Close();
}

/**
 * @brief XRouterHandle构造函数
 */
XRouterHandle::XRouterHandle()
{
}

/**
 * @brief XRouterHandle析构函数
 */
XRouterHandle::~XRouterHandle()
{
}
