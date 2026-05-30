/**
 * @file xms_download_service.cpp
 * @brief 文件下载服务主程序入口
 * 
 * 启动文件下载服务，处理文件分片下载请求
 */
#include "xlog_client.h"
#include "xdownload_server.h"
#include <iostream>

using namespace std;

/**
 * @brief 文件下载服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    XDownloadServer server;
    server.main(argc, argv);
    server.Start();
    std::cout << DOWNLOAD_NAME << endl;
    server.Wait();
    return 0;
}
