/**
 * @file xauth_server.cpp
 * @brief 认证服务主程序
 * 
 * 提供用户认证、token管理等功能的微服务
 */
#include <iostream>
#include "xservice.h"
#include "xauth_handle.h"
#include "xconfig_and_register.h"
#include "xauth_dao.h"
#include "xlog_client.h"
#include <thread>
#include "xtools.h"

using namespace std;
using namespace xmsg;

/**
 * @class XAuthService
 * @brief 认证服务类
 * 
 * 继承自XService，实现认证服务的监听和连接处理
 */
class XAuthService : public XService
{
public:
    /**
     * @brief 创建服务处理对象
     * @return XServiceHandle指针
     * 
     * 创建XAuthHandle实例处理客户端连接
     */
    XServiceHandle* CreateServiceHandle()
    {
        return new XAuthHandle();
    }
};

/**
 * @brief 认证服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 * 
 * 启动认证服务，初始化数据库连接、注册消息回调、启动服务监听
 */
int main(int argc, char *argv[])
{
    cout << "xauth_server SERVER_PORT REGISTER_IP REGISTER_PORT" << endl;
    cout << "xauth_server install" << endl;

    // 初始化数据库连接
    if (!XAuthDao::Get()->Init())
    {
        cout << "DB init failed!" << endl;
        return -1;
    }

    // 创建数据库表
    if (!XAuthDao::Get()->Install())
    {
        cout << "DB create table failed!" << endl;
        return -2;
    }

    // 注册消息回调函数
    XAuthHandle::RegMsgCallback();

    // 获取服务端口（默认AUTH_PORT）
    int server_port = AUTH_PORT;
    if (argc > 1)
        server_port = atoi(argv[1]);
    cout << "server port is " << server_port << endl;

    // 获取注册中心地址
    string register_ip = XGetHostByName(API_REGISTER_SERVER_NAME);
    if (argc > 2)
        register_ip = argv[2];
    if (register_ip.empty())
        register_ip = "127.0.0.1";

    // 获取注册中心端口（默认REGISTER_PORT）
    int register_port = REGISTER_PORT;
    if (argc > 3)
        register_port = atoi(argv[3]);

    // 初始化日志客户端
    static XAuthConfig config;
    XLogClient::Get()->set_service_name(AUTH_NAME);
    XLogClient::Get()->set_server_port(XLOG_PORT);
    XLogClient::Get()->set_auto_connect(true);
    XLogClient::Get()->StartLog();

    // 初始化配置和服务注册
    XConfigAndRegister::Init(AUTH_NAME, "127.0.0.1", server_port,
        register_ip.c_str(), register_port, &config);

    // 启动认证服务
    XAuthService service;
    service.set_server_port(server_port);
    service.Start();

    // 等待服务结束
    XThreadPool::Wait();
    return 0;
}

