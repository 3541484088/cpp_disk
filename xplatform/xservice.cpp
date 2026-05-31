/**
 * @file xservice.cpp
 * @brief 服务端基类实现文件
 * 
 * 实现服务端的监听、连接处理和线程池管理
 */
#include "xservice.h"
#include "event2/bufferevent.h"
#include "event2/listener.h"
#include "xservice_handle.h"
#include "xlog_client.h"
#include "xtools.h"
#include <sstream>

using namespace std;

/**
 * @brief libevent监听器回调函数
 * @param ev 监听器对象
 * @param sock 客户端socket
 * @param addr 客户端地址
 * @param socklen 地址长度
 * @param arg 用户参数（XService指针）
 */
static void SListenCB(struct evconnlistener *ev, evutil_socket_t sock, struct sockaddr *addr, int socklen, void *arg)
{
    LOGDEBUG("SListenCB");
    auto task = (XService*)arg;
    task->ListenCB(sock, addr, socklen);
}
void XService::Wait()
{ 
    XThreadPool::Wait();
}
/**
 * @brief 客户端连接回调处理
 * @param client_socket 客户端socket
 * @param client_addr 客户端地址
 * @param socklen 地址长度
 * 
 * 创建服务处理对象，设置客户端信息，并将其分发到线程池处理
 */
void XService::ListenCB(int client_socket, struct sockaddr *client_addr, int socklen)
{
    // 创建服务处理对象
    auto handle = CreateServiceHandle();
    handle->set_sock(client_socket);
    handle->set_ssl_ctx(ssl_ctx());

    // 获取客户端IP和端口
    stringstream ss;
    char ip[16] = { 0 };
    auto addr = (sockaddr_in *)client_addr;
    evutil_inet_ntop(AF_INET, &addr->sin_addr.s_addr, ip, sizeof(ip));
    int client_port = ntohs(addr->sin_port);
    
    ss << "accept client ip: " << ip << " port: " << client_port << endl;
    LOGINFO(ss.str().c_str());

    // 设置客户端信息并分发到线程池
    handle->set_client_ip(ip);
    handle->set_client_port(client_port);
    thread_client_pool_->Dispatch(handle);
}
/**
 * @brief 初始化服务
 * @return 初始化成功返回true，失败返回false
 * 
 * 创建libevent监听器，绑定到指定端口
 */
bool XService::Init()
{
    if (server_port_ <= 0)
    {
        LOGERROR("server_port_ not set!");
        return false;
    }

    // 设置监听地址
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(server_port_);
    
    // 创建监听器
    auto evc = evconnlistener_new_bind(base(), SListenCB, this,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
        10,  // listen backlog
        (sockaddr*)&sin,
        sizeof(sin)
    );
    
    if (!evc)
    {
        stringstream ss;
        ss << "listen port " << server_port_ << " failed!" << endl;
        LOGERROR(ss.str().c_str());
        return false;
    }
    
    stringstream ss;
    ss << "listen port " << server_port_ << " success!" << endl;
    LOGINFO(ss.str().c_str());
    return true;
}

/**
 * @brief 启动服务
 * @return 启动成功返回true，失败返回false
 * 
 * 初始化监听线程池和客户端处理线程池，并启动监听
 */
bool XService::Start()
{
    thread_listen_pool_->Init(1);           // 监听线程池：1个线程
    thread_client_pool_->Init(thread_count_); // 客户端处理线程池：thread_count_个线程
    thread_listen_pool_->Dispatch(this);    // 将服务对象分发到监听线程
    return true;
}

/**
 * @brief XService构造函数
 * 
 * 创建监听线程池和客户端处理线程池
 */
XService::XService()
{
    this->thread_client_pool_ = XThreadPoolFactory::Create();
    this->thread_listen_pool_ = XThreadPoolFactory::Create();
}

/**
 * @brief XService析构函数
 * 
 * 释放线程池资源
 */
XService::~XService()
{
    delete thread_client_pool_;
    thread_client_pool_ = NULL;
    delete thread_listen_pool_;
    thread_listen_pool_ = NULL;
}
