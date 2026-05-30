/**
 * @file config_server.cpp
 * @brief 配置中心服务主程序入口
 * 
 * 启动配置中心服务，处理配置管理、配置查询等功能
 */
#include <iostream>
#include "config_dao.h"
#include "xmsg_com.pb.h"
#include "xtools.h"
#include "xconfig_server.h"

using namespace std;
using namespace xmsg;

/**
 * @brief 配置中心服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 * 
 * 初始化数据库连接，创建配置表，启动配置中心服务
 */
int main(int argc, char *argv[])
{
    cout << "Config Server" << endl;

    // 初始化数据库连接
    if (ConfigDao::Get()->Init())
    { 
        cout << "ConfigDao::Get()->Init Success!" << endl;
        // 创建数据库表（如果不存在）
        ConfigDao::Get()->Install();
    }

    // 创建并启动配置中心服务
    XConfigServer config;
    config.main(argc, argv);
    config.Start();
    config.Wait();

    return 0;
}