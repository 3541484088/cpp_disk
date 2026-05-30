/**
 * @file xconfig_and_register.cpp
 * @brief 配置中心与注册中心集成客户端
 * 
 * 封装注册中心和配置中心的客户端，提供一站式服务注册和配置获取功能
 */
#include "xconfig_and_register.h"
#include "xregister_client.h"
#include "xconfig_client.h"
#include <string>

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
static void ConfTimer()
{
    static string conf_ip = "";
    static int conf_port = 0;

    // 仅在未获取到配置中心地址时执行
    if (conf_port <= 0)
    {
        // 从注册中心获取配置中心服务地址
        auto confs = REG->GetServcies(CONFIG_NAME, 1);
        if (confs.service_size() <= 0)
            return;
        
        auto conf = confs.service()[0];
        if (conf.ip().empty() || conf.port() <= 0)
            return;

        // 配置配置中心地址并连接
        conf_ip = conf.ip();
        conf_port = conf.port();
        CONF->set_server_ip(conf_ip.c_str());
        CONF->set_server_port(conf_port);
        CONF->Connect();
    }
}

/**
 * @brief 初始化配置中心与注册中心客户端
 * @param service_name 服务名称
 * @param service_ip 服务IP（NULL表示127.0.0.1）
 * @param service_port 服务端口
 * @param register_ip 注册中心IP
 * @param register_port 注册中心端口
 * @param conf_message 配置消息对象
 * @return 初始化成功返回true
 * 
 * 注册服务到注册中心，并启动配置获取流程
 */
bool XConfigAndRegister::Init(const char *service_name, 
    const char *service_ip, int service_port,
    const char *register_ip, int register_port, google::protobuf::Message *conf_message)
{ 
    // 配置注册中心地址
    XRegisterClient::Get()->set_server_ip(register_ip);
    XRegisterClient::Get()->set_server_port(register_port);

    // 注册服务到注册中心
    XRegisterClient::Get()->RegisterServer(service_name, service_port,
        service_ip ? service_ip : "127.0.0.1");

    // 启动配置获取
    CONF->StartGetConf(0, service_port, conf_message, ConfTimer);

    return true;
}