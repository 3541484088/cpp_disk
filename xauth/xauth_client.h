#ifndef XAUTH_CLIENT_H
#define XAUTH_CLIENT_H
#include "xservice_client.h"
#include <mutex>
#include <map>
#include <vector>
#define XAUTH XAuthClient::Get()
class XAuthClient:public XServiceClient
{
public:

    ~XAuthClient();
    static XAuthClient*Get()
    {
        static XAuthClient xc;
        return &xc;
    }

    bool Login(std::string username, std::string password);


    //////////////////////////////////////////////////////////////////
    ///发送登录请求
    /// @para username 用户
    /// @para password 密码（原文，在函数内部会进行md5转换）
    //static void LoginReq(XMsgEvent *msg_ev, std::string username, std::string password);

    //////////////////////////////////////////////////////////////////
    ///发送登录请求
    /// @para username 用户
    /// @para password 密码（原文，在函数内部会进行md5转换）
    void LoginReq(std::string username, std::string password);


    //检查token 是否有效 返回本地的 login登录信息
    void CheckTokenReq(std::string token) ;


    //////////////////////////////////////////////////////////////////
    ///添加用户信息
    void AddUserReq(const xmsg::XAddUserReq *user);

    //////////////////////////////////////////////////////////////////
    ///修改密码信息
    void ChangePasswordReq(const xmsg::XChangePasswordReq *pass);


    /////////////////////////////////////////////////////////////////
    /// 获取登录
    /// @timeoue_ms 登录超时时间，返回异常错误信息失败返回
    /// 有token不需要发送，给服务器发送即可
    /// @return 失败返回错误 token 错
    bool GetLoginInfo(std::string username,xmsg::XLoginRes *out_info, int timeoue_ms=200);

    xmsg::XLoginRes GetLogin();

    //////////////////////////////////////////////////////////////////
    ///注册接收服务器返回消息处理
    static void RegMsgCallback()
    {
        RegCB(xmsg::MSG_LOGIN_RES, (MsgCBFunc)&XAuthClient::LoginRes);
        RegCB(xmsg::MSG_ADD_USER_RES, (MsgCBFunc)&XAuthClient::AddUserRes);
        RegCB(xmsg::MSG_CHANGE_PASSWORD_RES, (MsgCBFunc)&XAuthClient::ChangePasswordRes);
        RegCB(xmsg::MSG_CHECK_TOKEN_RES, (MsgCBFunc)&XAuthClient::CheckTokenRes);
    }

    //当前登录的用户名
    std::string cur_user_name(){ return cur_user_name_; }

private:
    //当前登录的用户名
    std::string cur_user_name_;
    XAuthClient();

    //登录信息缓存
    std::mutex logins_mutex_;
    std::map<std::string, xmsg::XLoginRes> login_map_;

    //////////////////////////////////////////////////////////////////
    ///接收登录信息
    void LoginRes(xmsg::XMsgHead *head, XMsg *msg);

    //////////////////////////////////////////////////////////////////
    ///接收添加用户信息
    void AddUserRes(xmsg::XMsgHead *head, XMsg *msg);

    //////////////////////////////////////////////////////////////////
    ///接收修改密码信息
    void ChangePasswordRes(xmsg::XMsgHead *head, XMsg *msg);

    //////////////////////////////////////////////////////////////////
    ///接收token检查结果
    void CheckTokenRes(xmsg::XMsgHead *head, XMsg *msg);
};
#endif
