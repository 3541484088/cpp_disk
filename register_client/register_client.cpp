/**
 * @file register_client.cpp
 * @brief 注册中心客户端测试程序
 * 
 * 测试注册中心客户端的服务注册和服务发现功能
 */
#include <iostream>
#include "xregister_client.h"
#include "xtools.h"
#include <thread>
#include <chrono>

using namespace std;

/**
 * @brief 注册中心客户端测试主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    cout << "Register Client" << endl;
    cout << "Usage: register_client [REGISTER_IP] [REGISTER_PORT]" << endl;

    // 设置注册中心地址（默认127.0.0.1:REGISTER_PORT）
    string ip = "127.0.0.1";
    int port = REGISTER_PORT;
    
    if (argc > 1)
    {
        ip = argv[1];
    }
    if (argc > 2)
    {
        port = atoi(argv[2]);
    }

    // 配置注册中心客户端
    XRegisterClient::Get()->set_server_ip(ip.c_str());
    XRegisterClient::Get()->set_server_port(port);
    
    // 注册测试服务
    XRegisterClient::Get()->RegisterServer("test", 20020, nullptr);
    
    // 等待连接成功
    XRegisterClient::Get()->WaitConnected(3);

    // 循环获取服务列表
    for (;;)
    {
        // 获取全部服务列表
        XRegisterClient::Get()->GetServiceReq(nullptr);
        
        // 获取服务列表并打印
        auto services = XRegisterClient::Get()->GetAllService();
        if (services)
            LOGINFO(services->DebugString());
        
        this_thread::sleep_for(chrono::seconds(5));
    }

    XThreadPool::Wait();
    return 0;
}

