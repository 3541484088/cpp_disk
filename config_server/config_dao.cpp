/**
 * @file config_dao.cpp
 * @brief 配置中心数据访问层实现
 * 
 * 实现配置数据的增删改查操作，基于MySQL数据库存储配置信息
 */
#include "config_dao.h"
#include "LXMysql.h"
#include "xtools.h"
#include <string>

using namespace LX;
using namespace std;
using namespace xmsg;

// 配置表名称
#define CONIG_TABLE "xms_service_config"

// 数据库操作互斥锁
static mutex my_mutex;

/**
 * @brief 删除配置
 * @param ip 服务IP地址
 * @param port 服务端口
 * @return 删除成功返回true，失败返回false
 * 
 * 根据IP和端口删除对应的配置记录
 */
bool ConfigDao::DeleteConfig(const char *ip, int port)
{
    LOGDEBUG("DeleteConfig");
    
    XMutex mux(&my_mutex);
    
    // 检查数据库连接
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }
    
    // 参数校验
    if (!ip || port <= 0 || port > 65535 || strlen(ip) == 0)
    {
        LOGERROR("DeleteConfig failed! ip or port error");
        return false;
    }
    
    // 构建删除SQL
    string table_name = CONIG_TABLE;
    stringstream ss;
    ss << "delete from " << table_name;
    ss << " where service_ip='" << ip << "' and service_port=" << port;
    
    return my_->Query(ss.str().c_str());
}
/**
 * @brief 分页加载所有配置
 * @param page 页码（从1开始）
 * @param page_count 每页数量
 * @return 配置列表
 * 
 * 从数据库分页查询所有服务配置信息
 */
xmsg::XConfigList ConfigDao::LoadAllConfig(int page, int page_count)
{
    XConfigList confs;
    LOGDEBUG("LoadAllConfig");
    
    XMutex mux(&my_mutex);
    
    // 检查数据库连接
    if (!my_)
    {
        LOGERROR("mysql not init");
        return confs;
    }
    
    // 参数校验
    if (page <= 0 || page_count <= 0)
    {
        LOGERROR("LoadAllConfig parameter error");
        return confs;
    }
    
    // 构建分页查询SQL
    // 分页公式: limit (page-1)*page_count, page_count
    string table_name = CONIG_TABLE;
    stringstream ss;
    ss << "select `service_name`, `service_ip`, `service_port` from " << table_name;
    ss << " order by id desc";
    ss << " limit " << (page - 1) * page_count << "," << page_count;
    
    LOGDEBUG(ss.str());
    
    // 执行查询并构建结果
    auto rows = my_->GetResult(ss.str().c_str());
    for (auto row : rows)
    {
        auto conf = confs.add_config();
        conf->set_service_name(row[0].data ? row[0].data : "");
        conf->set_service_ip(row[1].data ? row[1].data : "");
        conf->set_service_port(row[2].data ? atoi(row[2].data) : 0);
    }
    
    return confs;
}

/**
 * @brief 根据IP和端口加载配置
 * @param ip 服务IP地址
 * @param port 服务端口
 * @return 配置信息
 * 
 * 根据服务IP和端口从数据库加载对应的配置信息
 */
xmsg::XConfig ConfigDao::LoadConfig(const char *ip, int port)
{
    XConfig conf;
    LOGDEBUG("ConfigDao::LoadConfig");
    
    XMutex mux(&my_mutex);
    
    // 检查数据库连接
    if (!my_)
    {
        LOGERROR("mysql not init");
        return conf;
    }
    
    // 参数校验
    if (!ip || port <= 0 || port > 65535 || strlen(ip) == 0)
    {
        LOGERROR("LoadConfig failed! ip or port error");
        return conf;
    }
    
    // 构建查询SQL
    string table_name = CONIG_TABLE;
    stringstream ss;
    ss << "select private_pb from " << table_name;
    ss << " where service_ip='" << ip << "' and service_port=" << port;
    
    auto rows = my_->GetResult(ss.str().c_str());
    
    // 检查查询结果
    if (rows.size() == 0)
    {
        stringstream log_ss;
        log_ss << "download config failed! not result ";
        log_ss << ip << ":" << port;
        LOGDEBUG(log_ss.str().c_str());
        return conf;
    }
    
    // 解析配置数据
    auto row = rows[0];
    if (!row[0].data || row[0].size <= 0)
    {
        LOGDEBUG("download config failed! empty private_pb");
        return conf;
    }
    
    if (!conf.ParseFromArray(row[0].data, row[0].size))
    {
        LOGDEBUG("download config failed! ParseFromArray failed!");
        return conf;
    }
    
    LOGDEBUG("download config success!");
    LOGDEBUG(conf.DebugString());
    return conf;
}

/**
 * @brief 保存配置（新增或更新）
 * @param conf 配置信息
 * @return 保存成功返回true，失败返回false
 * 
 * 先检查配置是否已存在，存在则更新，不存在则插入新记录
 */
bool ConfigDao::SaveConfig(const xmsg::XConfig *conf)
{
    LOGDEBUG("ConfigDao::SaveConfig");
    
    XMutex mux(&my_mutex);
    
    // 检查数据库连接
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }
    
    // 参数校验
    if (!conf || conf->service_ip().empty())
    {
        LOGERROR("ConfigDao::SaveConfig failed, conf value error!");
        return false;
    }
    
    // 构建数据对象
    string table_name = CONIG_TABLE;
    XDATA data;
    data["service_name"] = LXData(conf->service_name().c_str());
    int port = conf->service_port();
    data["service_port"] = LXData(&port);
    data["service_ip"] = LXData(conf->service_ip().c_str());

    // 序列化配置对象为private_pb
    string private_pb;
    conf->SerializeToString(&private_pb);
    data["private_pb"].data = private_pb.c_str();
    data["private_pb"].size = private_pb.size();
    data["proto"].data = conf->proto().c_str();
    data["proto"].size = conf->proto().size();

    // 构建WHERE条件
    stringstream ss;
    ss << " where service_ip='";
    ss << conf->service_ip() << "' and service_port=" << conf->service_port();
    
    string where = ss.str();
    string sql = "select id from ";
    sql += table_name;
    sql += where;
    
    LOGDEBUG(sql);
    
    // 查询是否已存在
    auto rows = my_->GetResult(sql.c_str());
    if (rows.size() > 0)
    {
        // 已存在，执行更新
        int count = my_->UpdateBin(data, table_name, where);
        if (count >= 0)
        {
            LOGDEBUG("config update success");
            return true;
        }
        LOGDEBUG("config update failed");
        return false;
    }

    // 不存在，执行插入
    bool re = my_->InsertBin(data, table_name);
    if (re)
    {
        LOGDEBUG("config insert success");
    }
    else
    {
        LOGDEBUG("config insert failed");
    }
    return re;
}
/**
 * @brief 安装配置表（创建表结构）
 * @return 创建成功返回true，失败返回false
 * 
 * 如果配置表不存在则创建
 */
bool ConfigDao::Install()
{
    LOGDEBUG("ConfigDao::Install()");
    
    XMutex mux(&my_mutex);
    
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }

    // 创建配置表SQL
    string sql = "CREATE TABLE IF NOT EXISTS `xms_service_config` ( \
        `id` INT AUTO_INCREMENT,\
        `service_name` VARCHAR(16),\
        `service_port` INT,\
        `service_ip` VARCHAR(16),\
        `private_pb` VARCHAR(4096),\
        `proto` VARCHAR(4096),\
        PRIMARY KEY(`id`));";

    if (!my_->Query(sql.c_str()))
    {
        LOGINFO("CREATE TABLE xms_service_config failed!");
        return false;
    }
    
    LOGINFO("CREATE TABLE xms_service_config success!");
    return true;
}

/**
 * @brief 初始化数据库连接
 * @return 初始化成功返回true，失败返回false
 * 
 * 初始化MySQL连接，设置重连和超时参数
 */
bool ConfigDao::Init()
{
    XMutex mux(&my_mutex);

    // 创建数据库连接实例
    if (!my_)
        my_ = new LXMysql();
    
    // 初始化数据库驱动
    if (!my_->Init())
    {
        LOGDEBUG("my_->Init() failed!");
        return false;
    }

    // 设置自动重连
    my_->SetReconnect(true);
    
    // 设置连接超时
    my_->SetConnectTimeout(3);
    
    // 连接数据库（从配置文件读取配置）
    if (!my_->InputDBConfig())
    {
        LOGDEBUG("my_->Connect failed!");
        return false;
    }
    
    LOGDEBUG("my_->Connect success!");
    return true;
}

/**
 * @brief ConfigDao构造函数
 */
ConfigDao::ConfigDao()
{
}

/**
 * @brief ConfigDao析构函数
 */
ConfigDao::~ConfigDao()
{
}
