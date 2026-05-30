/**
 * @file xregister_server.cpp
 * @brief 注册中心服务实现
 * 
 * 实现注册中心的服务端逻辑，包括服务注册、发现、心跳管理等功能
 */
#include "xregister_server.h"
#include "xregister_handle.h"
#include "xlog_client.h"

/**
 * @brief 根据命令行参数初始化服务
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * 注册消息回调函数，设置服务端口，初始化日志客户端
 */
void XRegisterServer::main(int argc, char *argv[])
{
    // 注册消息回调函数
    XRegisterHandle::RegMsgCallback();

    // 获取服务端口（默认REGISTER_PORT）
    int port = REGISTER_PORT;
    if (argc > 1)
        port = atoi(argv[1]);

    // 初始化日志客户端
    XLogClient::Get()->set_service_name("REGISTER");
    XLogClient::Get()->set_server_port(XLOG_PORT);
    XLogClient::Get()->set_auto_connect(true);
    XLogClient::Get()->StartLog();

    // 设置服务器监听端口
    set_server_port(port);
}

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 * 
 * 创建XRegisterHandle实例，设置读取超时时间用于心跳检测
 */
XServiceHandle* XRegisterServer::CreateServiceHandle()
{
    auto handle = new XRegisterHandle();
    // 设置读取超时时间（5秒），用于检测客户端心跳
    handle->set_read_timeout_ms(5000);
    return handle;
}

/**
 * @brief 等待服务线程退出
 * 
 * 阻塞等待所有线程池任务完成
 */
void XRegisterServer::Wait()
{
    XThreadPool::Wait();
}
