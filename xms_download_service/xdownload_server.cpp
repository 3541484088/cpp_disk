/**
 * @file xdownload_server.cpp
 * @brief 文件下载服务实现
 * 
 * 实现文件下载服务的初始化、注册到注册中心等功能
 */
#include "xdownload_handle.h"
#include "xdownload_server.h"
#include "xlog_client.h"
#include "xregister_client.h"
#include "xtools.h"
#include <sstream>
using namespace std;

/**
 * @brief 初始化文件下载服务
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * 命令行参数: xms_download_service [REGISTER_IP] [REGISTER_PORT] [SERVICE_PORT]
 */
void XDownloadServer::main(int argc, char *argv[])
{
    // 注册消息回调函数
    XDownloadHandle::RegMsgCallback();
    LOGDEBUG("xms_download_service register_ip register_port service_port ");

    // 设置默认端口
    int service_port = DOWNLOAD_PORT;
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
    
    // 注册到注册中心（is_find=true表示可被发现）
    XRegisterClient::Get()->RegisterServer(DOWNLOAD_NAME, service_port, "127.0.0.1", true);

    // 初始化日志客户端
    auto log = XLogClient::Get();
    log->set_service_name(DOWNLOAD_NAME);
    string logfile = DOWNLOAD_NAME;
    stringstream ss;
    ss << DOWNLOAD_NAME << "_" << service_port << ".log";
    log->set_local_file(ss.str());
    log->StartLog();
}

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 */
XServiceHandle* XDownloadServer::CreateServiceHandle()
{
    return new XDownloadHandle();
}

/**
 * @brief XDownloadServer构造函数
 */
XDownloadServer::XDownloadServer()
{
}

/**
 * @brief XDownloadServer析构函数
 */
XDownloadServer::~XDownloadServer()
{
}
