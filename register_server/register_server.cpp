/**
 * @file register_server.cpp
 * @brief 注册中心服务主程序入口
 * 
 * 启动注册中心服务，处理服务注册、发现和心跳管理
 */
#include <iostream>
#include "xregister_server.h"

using namespace std;

/**
 * @brief 注册中心服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 * 
 * 创建注册中心服务实例，初始化并启动服务监听
 */
int main(int argc, char *argv[])
{
    cout << "Register Server" << endl;
    
    // 创建注册中心服务实例
    XRegisterServer server;
    
    // 初始化服务（解析命令行参数，设置端口等）
    // 使用方式: register_server [端口号]，默认端口 REGISTER_PORT
    server.main(argc, argv);

    // 启动服务线程，开始监听端口
    server.Start();
   
    // 阻塞等待线程池退出
    server.Wait();
    return 0;
}