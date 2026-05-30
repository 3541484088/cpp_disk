/**
 * @file xms_dir_service.cpp
 * @brief 目录服务主程序入口
 * 
 * 启动目录服务，处理文件目录的增删改查等操作
 */
#include <iostream>
#include "xdir_service.h"
#include "xdir_handle.h"
#include "xregister_client.h"
#include "xlog_client.h"
#include "xtools.h"
#include <string>
using namespace std;

/**
 * @brief 目录服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    std::cout << "xms_dir_service!\n";
    
    // 解析注册中心地址
    string register_ip = XGetHostByName(API_REGISTER_SERVER_NAME);
    if (argc > 1)
        register_ip = argv[1];
    XRegisterClient::Get()->set_server_ip(register_ip.c_str());

    // 注册消息回调
    XDirHandle::RegMsgCallback();
    
    // 注册到注册中心
    XRegisterClient::Get()->RegisterServer(DIR_NAME, DIR_PORT, "127.0.0.1");

    // 初始化日志客户端
    auto log = XLogClient::Get();
    log->set_service_name(DIR_NAME);
    log->set_is_print(true);
    log->StartLog();

    // 创建并启动目录服务
    XDirService service;
    service.set_server_port(DIR_PORT);
    service.Start();
    XThreadPool::Wait();
    return 0;
}
