/**
 * @file xapplication.cpp
 * @brief 应用程序全局配置管理
 * 
 * 提供全局应用配置项的管理，包括服务器名称、IP地址、路径等配置
 */
#include "xapplication.h"
#include "xthread_pool.h"

std::string XApplication::Application;   // 应用程序名称
std::string XApplication::ServerName;    // 服务器名称，通常为"服务名.节点标识"格式
std::string XApplication::LocalIp;       // 本地IP地址
std::string XApplication::BasePath;      // 应用基础路径，用于存放系统使用的临时目录
std::string XApplication::DataPath;      // 应用数据路径，用于存放持久化数据
std::string XApplication::Local;         // 本地化设置
std::string XApplication::Node;          // 节点地址
std::string XApplication::Log;           // 日志服务器地址
std::string XApplication::Config;        // 配置服务器地址
std::string XApplication::Notify;        // 消息通知配置
std::string XApplication::LogPath;       // 日志文件路径
std::string XApplication::LogLevel;      // 日志级别
std::string XApplication::ConfigFile;    // 配置文件路径

/**
 * @brief 应用程序初始化入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 */
void XApplication::main(int argc, char *argv[])
{
    // 应用初始化逻辑
}

/**
 * @brief 等待应用程序关闭
 * 
 * 阻塞等待线程池中的所有任务完成，然后优雅关闭应用
 */
void XApplication::WaitForShutdown()
{
    XThreadPool::Wait();
}

/**
 * @brief XApplication 构造函数
 */
XApplication::XApplication()
{
}

/**
 * @brief XApplication 析构函数
 */
XApplication::~XApplication()
{
}
