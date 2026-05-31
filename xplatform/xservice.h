#pragma once
#include "xtask.h"
#include "xservice_handle.h"
#include "xthread_pool.h"
class XCOM_API XService :public XTask
{
public:
    XService();
    ~XService();

    //主要接口，毎个客户端连接，运行此函数处理客户端请求，创建到线程池
    virtual XServiceHandle* CreateServiceHandle() = 0;
    ///服务初始化 线程池的回调
    bool Init();

    ///开始服务，创建新线程，客户端请求会放到线程池
    bool Start();

    //设置服务器端口
    void set_server_port(int port) { this->server_port_ = port; }

    void ListenCB(int client_socket, struct sockaddr *addr, int socklen);

    void Wait();
private:
    
    //接收用户连接放到线程池
    XThreadPool *thread_listen_pool_ = 0;

    //接收用户请求数据的线程池
    XThreadPool *thread_client_pool_ = 0;

    //客户端数据处理线程池线程数
    int thread_count_ = 10;

    //设置服务器端口
    int server_port_ = 0;


};
