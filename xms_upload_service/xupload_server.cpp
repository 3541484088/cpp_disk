/**
 * @file xupload_server.cpp
 * @brief 文件上传服务实现
 * 
 * 实现文件上传服务的初始化、注册到注册中心等功能
 */
#include "xupload_server.h"
#include "xupload_handle.h"
#include "xlog_client.h"
#include "xregister_client.h"
#include "xtools.h"
#include <sstream>
using namespace std;

/**
 * @brief 初始化文件上传服务
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * 命令行参数: xms_upload_service [REGISTER_IP] [REGISTER_PORT] [SERVICE_PORT]
 */
void XUploadServer::main(int argc, char *argv[])
{
    // 注册消息回调函数
    XUploadHandle::RegMsgCallback();
    LOGDEBUG("xms_upload_service register_ip register_port service_port ");

    // 设置默认端口
    int service_port = UPLOAD_PORT;
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
    XRegisterClient::Get()->RegisterServer(UPLOAD_NAME, service_port, "127.0.0.1", true);
    
    // 初始化日志客户端
    auto log = XLogClient::Get();
    log->set_service_name(UPLOAD_NAME);
    log->set_is_print(true);
    string logfile = UPLOAD_NAME;
    stringstream ss;
    ss << UPLOAD_NAME << "_" << service_port << ".log";
    log->set_local_file(ss.str());
    log->StartLog();
}

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 */
XServiceHandle* XUploadServer::CreateServiceHandle()
{
    return new XUploadHandle();
}

/**
 * @brief XUploadServer构造函数
 */
XUploadServer::XUploadServer()
{
}

/**
 * @brief XUploadServer析构函数
 */
XUploadServer::~XUploadServer()
{
}
