/**
 * @file xauth_handle.cpp
 * @brief 认证服务消息处理实现
 * 
 * 处理用户登录、token验证、用户注册、密码修改等认证相关请求
 */
#include "xauth_handle.h"
#include "xauth_dao.h"
#include "xtools.h"
#include <string>

using namespace std;
using namespace xmsg;

/**
 * @brief Token验证请求处理
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析Token验证请求，调用DAO层验证token有效性
 */
void XAuthHandle::CheckTokenReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XLoginRes res;
    XAuthDao::Get()->CheckToken(head, &res);
    head->set_msg_type(MSG_CHECK_TOKEN_RES);
    SendMsg(head, &res);
}

/**
 * @brief 用户登录请求处理
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析登录请求，验证用户名密码，生成token并返回
 */
void XAuthHandle::LoginReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // token过期时间，默认30分钟（可考虑从配置读取）
    int timeout_sec = 1800;

    // 登录请求对象
    XLoginReq req;
    
    // 登录响应对象
    XLoginRes res;
    
    // 解析protobuf消息
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("LoginReq failed!");
        res.set_res(XLoginRes::ERROR);
        res.set_token("LoginReq ParseFromArray failed!");
        SendMsg(MSG_LOGIN_RES, &res);
        return;
    }

    // 调用DAO层进行登录验证
    bool re = XAuthDao::Get()->Login(&req, &res, timeout_sec);
    if (!re)
    {
        LOGDEBUG("XAuthDao::Get()->Login failed!");
        res.set_res(XLoginRes::ERROR);
        res.set_token("username or password failed!");
    }

    // 设置响应消息类型并发送
    head->set_msg_type(MSG_LOGIN_RES);
    SendMsg(head, &res);
}

/**
 * @brief 添加用户请求处理
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析添加用户请求，创建新用户账户
 */
void XAuthHandle::AddUserReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XAddUserReq req;
    XMessageRes res;
    
    // 解析请求消息
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("XAddUserReq ParseFromArray failed!");
        SendMsg(MSG_ADD_USER_RES, &res);
        return;
    }

    // 调用DAO层添加用户
    bool re = XAuthDao::Get()->AddUser(&req);
    if (!re)
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("XAuthDao::Get()->AddUser failed!");
        SendMsg(MSG_ADD_USER_RES, &res);
        return;
    }

    // 设置成功响应
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK!");
    head->set_msg_type(MSG_ADD_USER_RES);
    SendMsg(head, &res);
}

/**
 * @brief 修改密码请求处理
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析修改密码请求，更新用户密码
 */
void XAuthHandle::ChangePasswordReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XChangePasswordReq req;
    XMessageRes res;
    
    // 解析请求消息
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("XChangePasswordReq ParseFromArray failed!");
        SendMsg(MSG_CHANGE_PASSWORD_RES, &res);
        return;
    }

    // 调用DAO层修改密码
    bool re = XAuthDao::Get()->ChangePassword(&req);
    if (!re)
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("XAuthDao::Get()->ChangePassword failed!");
        SendMsg(MSG_CHANGE_PASSWORD_RES, &res);
        return;
    }

    // 设置成功响应
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK!");
    head->set_msg_type(MSG_CHANGE_PASSWORD_RES);
    SendMsg(head, &res);
}

/**
 * @brief XAuthHandle构造函数
 */
XAuthHandle::XAuthHandle()
{
}

/**
 * @brief XAuthHandle析构函数
 */
XAuthHandle::~XAuthHandle()
{
}
