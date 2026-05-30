/**
 * @file xconfig_server.cpp
 * @brief 配置中心服务实现
 * 
 * 实现配置中心的服务端逻辑，包括配置管理、注册到注册中心等功能
 */
#include "xconfig_server.h"
#include "xtools.h"
#include "xregister_client.h"
#include "xconfig_handle.h"
#include "xlog_client.h"

using namespace std;

/**
 * @brief XConfigServer构造函数
 */
XConfigServer::XConfigServer()
{
}

/**
 * @brief XConfigServer析构函数
 */
XConfigServer::~XConfigServer()
{
}

/**
 * @brief 初始化配置中心服务
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * 初始化日志客户端，注册消息回调，配置端口，注册到注册中心
 * 命令行参数: config_server [REGISTER_IP] [REGISTER_PORT] [SERVICE_PORT]
 */
void XConfigServer::main(int argc, char *argv[])
{
    // 初始化日志客户端
    XLogClient::Get()->set_service_name(CONFIG_NAME);
    XLogClient::Get()->set_server_port(XLOG_PORT);
    XLogClient::Get()->set_auto_connect(true);
    XLogClient::Get()->StartLog();

    LOGDEBUG("config_server register_ip register_port service_port");

    // 注册消息回调函数
    XConfigHandle::RegMsgCallback();

    // 设置端口（默认值）
    int service_port = CONFIG_PORT;
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
    XRegisterClient::Get()->RegisterServer(CONFIG_NAME, service_port, "127.0.0.1");
}

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 * 
 * 创建XConfigHandle实例处理客户端请求
 */
XServiceHandle* XConfigServer::CreateServiceHandle()
{
    return new XConfigHandle();
}

/**
 * @brief 等待服务线程退出
 * 
 * 阻塞等待所有线程池任务完成
 */
void XConfigServer::Wait()
{
    XThreadPool::Wait();
}