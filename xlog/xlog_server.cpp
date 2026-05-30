/**
 * @file xlog_server.cpp
 * @brief 日志服务实现
 * 
 * 实现日志服务的初始化、注册到注册中心等功能
 */
#include "xlog_server.h"
#include "xlog_handle.h"
#include "xlog_dao.h"
#include "xregister_client.h"
#include "xlog_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "xtools.h"

using namespace std;

/**
 * @brief 初始化日志服务
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * 命令行参数: xlog [REGISTER_IP] [REGISTER_PORT] [SERVICE_PORT]
 */
void XLogServer::main(int argc, char *argv[])
{
    // 关闭日志客户端的控制台打印（避免日志循环）
    XLogClient::Get()->set_is_print(false);
    
    // 注册消息回调函数
    XLogHandle::RegMsgCallback();
    
    // 设置默认端口
    int service_port = XLOG_PORT;
    int register_port = REGISTER_PORT;
    string register_ip = XGetHostByName(API_REGISTER_SERVER_NAME);

    // 解析命令行参数
    if (argc > 1)
        register_ip = argv[1];
    if (argc > 2)
        register_port = atoi(argv[2]);
    if (argc > 3)
        service_port = atoi(argv[3]);

    // 设置服务监听端口
    set_server_port(service_port);

    // 配置注册中心客户端
    XRegisterClient::Get()->set_server_ip(register_ip.c_str());
    XRegisterClient::Get()->set_server_port(register_port);

    // 注册到注册中心
    XRegisterClient::Get()->RegisterServer(XLOG_NAME, service_port, "127.0.0.1");
}

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 * 
 * 创建XLogHandle实例处理客户端请求
 */
XServiceHandle* XLogServer::CreateServiceHandle()
{
    return new XLogHandle();
}

/**
 * @brief XLogServer构造函数
 */
XLogServer::XLogServer()
{
}

/**
 * @brief XLogServer析构函数
 */
XLogServer::~XLogServer()
{
}
