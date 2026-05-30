/**
 * @file xauth_proxy.cpp
 * @brief 鉴权代理实现
 * 
 * 实现Token验证和缓存管理，处理登录响应消息
 */
#include "xauth_proxy.h"
#include "xauth_client.h"
#include <map>
#include <string>
#include <thread>
#include <chrono>
#include "xmsg_com.pb.h"
#include "xtools.h"
using namespace std;
using namespace xmsg;

// Token缓存映射
static map<string, XLoginRes> token_cache;
static mutex token_cache_mutex;

/**
 * @brief Token清理线程类
 * 
 * 定时清理过期的Token缓存
 */
class TokenThread
{
public:
    void Start()
    {
        thread th(&TokenThread::Main, this);
        th.detach();
    }
    ~TokenThread()
    {
        is_exit_ = true;
        this_thread::sleep_for(chrono::milliseconds(10));
    }
private:
    bool is_exit_ = false;
    void Main()
    {
        while (!is_exit_)
        {
            token_cache_mutex.lock();
            // 清理过期token，需要优化
            auto ptr = token_cache.begin();
            for (; ptr != token_cache.end(); )
            {
                auto tmp = ptr;
                auto tt = time(0);
                ptr++;
                if (tmp->second.expired_time() < tt)
                {
                    cout << "expired_time " << tmp->second.expired_time() << endl;
                    token_cache.erase(tmp);
                }
            }
            token_cache_mutex.unlock();
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
};

static TokenThread token_thread;

/**
 * @brief 初始化鉴权代理
 * 
 * 启动Token清理线程
 */
void XAuthProxy::InitAuth()
{
    token_thread.Start();
}

/**
 * @brief 检查Token有效性
 * @param head 消息头
 * @return 验证通过返回true
 * 
 * 验证Token是否存在、是否过期、用户名和角色名是否匹配
 */
bool XAuthProxy::CheckToken(const xmsg::XMsgHead *head)
{
    XMutex mux(&token_cache_mutex);
    string token = head->token();
    if (token.empty())
    {
        LOGINFO("XAuthProxy::CheckToken failed! token is empty!");
        return false;
    }
    auto tt = token_cache.find(token);
    if (tt == token_cache.end())
    {
        LOGINFO("XAuthProxy::CheckToken failed! the token cache not find!");
        return false;
    }
        
    if (tt->second.username() != head->username())
    {
        stringstream ss;
        ss << "XAuthProxy::CheckToken failed! username is error!(head/cache)" << head->username();
        ss << "/" << tt->second.username();
        LOGINFO(ss.str().c_str());
        return false;
    }

    if (tt->second.rolename() != head->rolename())
    {
        stringstream ss;
        ss << "XAuthProxy::CheckToken failed! rolename is error!(head/cache)" << head->rolename();
        ss << "/" << tt->second.rolename();
        LOGINFO(ss.str().c_str());
        return false;
    }
    return true;
}

/**
 * @brief 消息读取回调函数
 * @param head 消息头
 * @param msg 消息体
 * 
 * 处理登录响应消息，缓存Token信息
 */
void XAuthProxy::ReadCB(xmsg::XMsgHead *head, XMsg *msg)
{
    if (!head) return;
    // 如果是登录验证返回token成功，则本地保存
    // 当前版本只验证token有效性，不验证权限
    XLoginRes res;
    switch (head->msg_type())
    {
    case MSG_LOGIN_RES:  // 登录响应
        if (res.ParseFromArray(msg->data, msg->size))
        {
            XMutex mux(&token_cache_mutex);
            token_cache[res.token()] = res;
        }
        cout << res.DebugString();

    default:
        break;
    }
    XServiceProxyClient::ReadCB(head, msg);
}
