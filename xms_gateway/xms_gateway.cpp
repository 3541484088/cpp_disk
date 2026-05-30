/**
 * @file xms_gateway.cpp
 * @brief API网关服务主程序入口
 * 
 * 启动API网关服务，负责请求路由、服务代理、鉴权等功能
 */
#include <iostream>
#include "xservice.h"
#include "xrouter_server.h"
#include "xservice_proxy.h"
#include "xregister_client.h"
#include "xmsg_com.pb.h"
#include "xconfig_client.h"
#include "xauth_proxy.h"
#include "xlog_client.h"
#include "xtools.h"
#include <thread>
#include <chrono>
using namespace std;
using namespace xmsg;

// 宏定义简化调用
#define REG  XRegisterClient::Get()
#define CONF XConfigClient::Get()

/**
 * @brief 配置定时回调函数
 * 
 * 定时从注册中心获取配置中心地址，然后连接配置中心获取配置
 */
void ConfTimer()
{
    static string conf_ip = "";
    static int conf_port = 0;

    if (conf_port <= 0)
    {
        auto confs = REG->GetServcies(CONFIG_NAME, 1);
        if (confs.service_size() <= 0)
            return;
        auto conf = confs.service()[0];

        if (conf.ip().empty() || conf.port() <= 0)
            return;
        conf_ip = conf.ip();
        conf_port = conf.port();
        CONF->set_server_ip(conf_ip.c_str());
        CONF->set_server_port(conf_port);
        CONF->Connect();
    }
}

/**
 * @brief API网关服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 * 
 * 命令行参数: xms_gateway [API_GATEWAY_PORT] [REGISTER_IP] [REGISTER_PORT]
 */
int main(int argc, char *argv[])
{
    cout << "xms_gateway API_GATEWAY_PORT REGISTER_IP REGISTER_PORT" << endl;
    
    // 解析服务端口
    int server_port = API_GATEWAY_PORT;
    if (argc > 1)
        server_port = atoi(argv[1]);
    cout << "server port is " << server_port << endl;
    
    // 解析注册中心地址
    string register_ip = "";
    register_ip = XGetHostByName(API_REGISTER_SERVER_NAME);
    if (argc > 2)
        register_ip = argv[2];
    int register_port = REGISTER_PORT;
    if (argc > 3)
        register_port = atoi(argv[3]);
    if (register_ip.empty())
        register_ip = "127.0.0.1";

    // 初始化鉴权代理
    XAuthProxy::InitAuth();

    // 初始化日志客户端
    XLogClient::Get()->set_service_name(API_GATEWAY_NAME);
    XLogClient::Get()->set_server_port(XLOG_PORT);
    XLogClient::Get()->set_auto_connect(true);
    XLogClient::Get()->StartLog();

    // 配置注册中心客户端
    XRegisterClient::Get()->set_server_ip(register_ip.c_str());
    XRegisterClient::Get()->set_server_port(register_port);

    // 注册到注册中心（is_find=true表示可被发现）
    XRegisterClient::Get()->RegisterServer(API_GATEWAY_NAME, server_port, "127.0.0.1", true);

    // 等待注册中心连接成功
    XRegisterClient::Get()->WaitConnected(3);
    XRegisterClient::Get()->GetServiceReq(0);

    // 初始化服务代理
    XServiceProxy::Get()->Init();
    XServiceProxy::Get()->Start();

    // 启动配置获取
    static XGatewayConfig cur_conf;
    if (XConfigClient::Get()->StartGetConf(0, server_port, &cur_conf, ConfTimer))
        cout << "gateway config client started" << endl;

    // 创建并启动路由服务
    XRouterServer service;
    service.set_server_port(server_port);
    service.Start();
    XThreadPool::Wait();
    return 0;
}
