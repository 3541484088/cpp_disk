/**
 * @file xms_upload_service.cpp
 * @brief 文件上传服务主程序入口
 * 
 * 启动文件上传服务，处理文件分片上传请求
 */
#include "xlog_client.h"
#include "xupload_server.h"
#include <iostream>

using namespace std;

/**
 * @brief 文件上传服务主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    XUploadServer server;
    server.main(argc, argv);
    server.Start();
    std::cout << UPLOAD_NAME << endl;
    server.Wait();
    return 0;
}
