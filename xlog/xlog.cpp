/**
 * @file xlog.cpp
 * @brief 日志服务主程序入口
 * 
 * 启动日志服务，处理日志存储和查询请求
 */
#include <iostream>
#include "xlog_dao.h"
#include "xlog_server.h"
#include "xlog_client.h"
#include <thread>
#include <chrono>

using namespace std;
using namespace xmsg;

/**
 * @brief 日志服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    cout << "XLog Server" << endl;

    // 初始化数据库连接并创建日志表
    if (XLogDAO::Get()->Init())
    {
        cout << "XLogDAO::Get()->Init Success!" << endl;
        XLogDAO::Get()->Install();
    }

    // 创建并启动日志服务
    XLogServer xlog;
    xlog.set_server_port(XLOG_PORT);
    xlog.main(argc, argv);
    xlog.Start();

    XThreadPool::Wait();
    return 0;
}
