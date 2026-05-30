/**
 * @file xlog_dao.cpp
 * @brief 日志服务数据访问层实现
 * 
 * 实现日志数据的存储操作，基于MySQL数据库存储日志信息
 */
#include "xlog_dao.h"
#include "xmsg_com.pb.h"
#include "LXMysql.h"
#include "xtools.h"
#include <thread>

using namespace std;
using namespace LX;
using namespace xmsg;

// 数据库操作互斥锁
static mutex my_mutex;

/**
 * @brief 添加日志记录
 * @param req 日志请求对象
 * @return 添加成功返回true，失败返回false
 * 
 * 将日志信息插入到数据库表中
 */
bool XLogDAO::AddLog(const xmsg::XAddLogReq *req)
{
    if (!req) return false;

    // 构建数据对象
    XDATA data;
    data["service_name"] = req->service_name().c_str();
    data["service_ip"] = req->service_ip().c_str();
    
    int service_port = req->service_port();
    data["service_port"] = &service_port;
    
    data["log_txt"] = req->log_txt().c_str();
    
    int log_time = req->log_time();
    data["log_time"] = &log_time;
    
    int log_level = req->log_level();
    data["log_level"] = &log_level;
    
    // 线程安全的数据库插入操作
    XMutex mux(&my_mutex);
    if (!my_)
    {
        cout << "mysql not init" << endl;
        return false;
    }
    
    return my_->InsertBin(data, "xms_log");
}
/**
 * @brief 初始化数据库连接
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化MySQL连接，设置重连和超时参数
 */
bool XLogDAO::Init()
{
    XMutex mux(&my_mutex);

    // 创建数据库连接实例
    if (!my_)
        my_ = new LXMysql();
    
    // 初始化数据库驱动
    if (!my_->Init())
    {
        cout << "my_->Init() failed!" << endl;
        return false;
    }

    // 设置自动重连
    my_->SetReconnect(true);

    // 设置连接超时
    my_->SetConnectTimeout(3);
    
    // 连接数据库（从配置文件读取配置）
    if (!my_->InputDBConfig())
    {
        cout << "my_->Connect failed!" << endl;
        return false;
    }
    
    cout << "my_->Connect success!" << endl;
    return true;
}

/**
 * @brief 安装日志表（创建表结构）
 * @return 创建成功返回true，失败返回false
 * 
 * 如果日志表不存在则创建
 */
bool XLogDAO::Install()
{
    cout << "XLogDAO::Install()" << endl;

    XMutex mux(&my_mutex);
    if (!my_)
    {
        cout << "mysql not init" << endl;
        return false;
    }

    string table_name = "xms_log";

    // 创建日志表SQL
    string sql = "CREATE TABLE IF NOT EXISTS `" + table_name + "` ( \
        `id` INT AUTO_INCREMENT,\
        `service_name` VARCHAR(16),\
        `service_port` INT,\
        `service_ip` VARCHAR(16),\
        `log_txt` VARCHAR(4096),\
        `log_time` INT,\
        `log_level` INT,\
        PRIMARY KEY(`id`));";

    if (!my_->Query(sql.c_str()))
    {
        cout << "CREATE TABLE " << table_name << " failed!" << endl;
        return false;
    }
    
    cout << "CREATE TABLE " << table_name << " success!" << endl;
    return true;
}

/**
 * @brief XLogDAO构造函数
 */
XLogDAO::XLogDAO()
{
}

/**
 * @brief XLogDAO析构函数
 */
XLogDAO::~XLogDAO()
{
}
