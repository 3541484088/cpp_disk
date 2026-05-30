/**
 * @file xauth_client.cpp
 * @brief 认证客户端实现
 * 
 * 提供用户登录、注册、密码修改等认证功能的客户端接口
 */
#include "xauth_client.h"
#include "xtools.h"
#include <thread>
#include <chrono>
using namespace std;
using namespace xmsg;

/**
 * @brief 用户登录（简化接口）
 * @param username 用户名
 * @param password 密码（明文，内部会进行MD5加密）
 * @return 登录成功返回 true，失败返回 false
 */
bool XAuthClient::Login(std::string username, std::string password)
{
    LoginReq(username, password);

    auto res = GetLogin();
    if (res.res() == XLoginRes::ERROR)
        return false;
    return true;
}

/**
 * @brief 获取登录结果
 * @return 登录响应对象
 */
xmsg::XLoginRes XAuthClient::GetLogin()
{
    xmsg::XLoginRes res;

    if (!GetLoginInfo(cur_user_name_, &res, 3000))
    {
        res.set_res(XLoginRes::ERROR);
        return res;
    }
    return res;
}

/**
 * @brief 获取登录信息（阻塞等待）
 * @param username 用户名
 * @param out_info 输出的登录信息
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功获取返回 true，超时返回 false
 * 
 * 轮询等待登录响应，用于异步登录后获取结果
 */
bool XAuthClient::GetLoginInfo(string username, xmsg::XLoginRes *out_info, int timeout_ms)
{
    if (!out_info) return false;
    int count = timeout_ms / 10;
    if (count <= 0) count = 1;
    
    for (int i = 0; i < count; i++)
    {
        {
            logins_mutex_.lock();
            cout << login_map_.size() << ";" << flush;
            auto login_ptr = login_map_.find(username);
            if (login_ptr == login_map_.end())
            {
                logins_mutex_.unlock();
                this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            auto login = login_ptr->second;
            logins_mutex_.unlock();

            if (login.res() == XLoginRes::OK)
            {
                out_info->CopyFrom(login);
                return true;
            }
            return false;
        }
    }
    return false;
}

/**
 * @brief 发送 Token 验证请求
 * @param token 待验证的 token
 */
void XAuthClient::CheckTokenReq(std::string token)
{
}

/**
 * @brief 处理 Token 验证响应
 * @param head 消息头
 * @param msg 消息体
 */
void XAuthClient::CheckTokenRes(xmsg::XMsgHead *head, XMsg *msg)
{
}

/**
 * @brief 发送登录请求
 * @param username 用户名
 * @param password 密码（明文，内部会进行MD5加密后发送）
 */
void XAuthClient::LoginReq(std::string username, std::string password)
{
    cur_user_name_ = username;
    XLoginReq req;
    req.set_username(username);
    
    // 对密码进行 MD5 加密
    auto md5_password = XMD5_base64((unsigned char *)password.data(), password.size());
    req.set_password(md5_password);
    
    // 清除上次该用户的登录信息
    {
        XMUTEX(&logins_mutex_);
        login_map_.erase(username);
    }

    SendMsg(MSG_LOGIN_REQ, &req);
}

/**
 * @brief 发送添加用户请求
 * @param user 用户信息
 * 
 * 密码会在内部进行 MD5 加密后发送
 */
void XAuthClient::AddUserReq(const xmsg::XAddUserReq *user)
{
    if (!user) return;
    XAddUserReq req;
    req.CopyFrom(*user);
    string pass = user->password();
    
    // 对密码进行 MD5 加密
    auto pass_md = XMD5_base64((unsigned char*)pass.c_str(), pass.size());
    req.set_password(pass_md);
    
    SendMsg(MSG_ADD_USER_REQ, &req);

    static int i = 0;
    i++;
    cout << i << " XAuthClient::Get()->AddUserReq(&req);" << endl;
}

/**
 * @brief 发送修改密码请求
 * @param pass 密码修改请求信息
 */
void XAuthClient::ChangePasswordReq(const xmsg::XChangePasswordReq *pass)
{
    SendMsg(MSG_CHANGE_PASSWORD_REQ, pass);
}

/**
 * @brief 处理登录响应
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析登录响应并存储到 login_map_ 中
 */
void XAuthClient::LoginRes(xmsg::XMsgHead *head, XMsg *msg)
{
    static int count;
    count++;
    cout << "LoginRes " << count << flush;
    
    XLoginRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("XLoginRes ParseFromArray failed!");
        return;
    }

    LOGINFO(res.DebugString());
    LOGINFO(head->DebugString());
    
    {
        cout << "begin XMUTEX(&logins_mutex_)" << endl;
        XMUTEX(&logins_mutex_);
        cout << "end XMUTEX(&logins_mutex_)" << endl;
        
        if (!res.username().empty())
        {
            login_map_[res.username()] = res;
            set_login(&res);
        }
    }
}

/**
 * @brief 处理添加用户响应
 * @param head 消息头
 * @param msg 消息体
 */
void XAuthClient::AddUserRes(xmsg::XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("AddUser failed! ParseFromArray failed!");
        return;
    }
    if (res.return_() == XMessageRes::ERROR)
    {
        LOGDEBUG(res.msg().c_str());
        LOGDEBUG("AddUser failed!");
        return;
    }
    LOGDEBUG("AddUser success!");
}

/**
 * @brief 处理修改密码响应
 * @param head 消息头
 * @param msg 消息体
 */
void XAuthClient::ChangePasswordRes(xmsg::XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("ChangePassword failed! ParseFromArray failed!");
        return;
    }
    if (res.return_() == XMessageRes::ERROR)
    {
        LOGDEBUG(res.msg().c_str());
        LOGDEBUG("ChangePasswordRes failed!");
        return;
    }
    LOGDEBUG("ChangePasswordRes success!");
}

/**
 * @brief XAuthClient 构造函数
 * 
 * 注册消息回调并设置服务名称
 */
XAuthClient::XAuthClient()
{
    RegMsgCallback();
    set_service_name(AUTH_NAME);
}

/**
 * @brief XAuthClient 析构函数
 */
XAuthClient::~XAuthClient()
{
}
