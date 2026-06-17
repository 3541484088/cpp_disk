/**
 * @file xauth_dao.cpp
 * @brief 认证服务数据访问层实现
 * 
 * 提供用户认证相关的数据库操作，包括用户登录、注册、密码修改、token验证等
 */
#include "xauth_dao.h"
#include "xtools.h"
#include "LXMysql.h"
#include "xmsg_com.pb.h"
#include <thread>
#include <mutex>

using namespace std;
using namespace LX;
using namespace xmsg;

// 数据库操作互斥锁
static mutex auth_mutex;

/**
 * @brief 初始化数据库连接
 * @return 初始化成功返回true，失败返回false
 * 
 * 创建LXMysql实例，配置自动重连和超时时间，读取数据库配置并连接
 * 同时创建默认管理员用户（root/123456）
 */
bool XAuthDao::Init()
{
    {
        XMutex mux(&auth_mutex);

        // 创建数据库实例
        if (!my_)
            my_ = new LXMysql();
        
        // 初始化数据库驱动
        if (!my_->Init())
        {
            LOGDEBUG("XAuthDao my_->Init() failed!");
            return false;
        }

        // 配置自动重连
        my_->SetReconnect(true);
        
        // 设置连接超时时间
        my_->SetConnectTimeout(3);
        
        // 读取数据库配置并连接
        if (!my_->InputDBConfig())
        {
            LOGDEBUG("my_->Connect failed!");
            return false;
        }
    }
    
    LOGDEBUG("my_->Connect success!");
    
    // 创建默认管理员用户（如果不存在）
    XAddUserReq user;
    user.set_username("root");
    string password = "123456";
    auto md5_password = XMD5_base64((unsigned char *)password.data(), password.size());
    user.set_password(md5_password);
    AddUser(&user);
    
    return true;
}


/**
 * @brief 添加用户
 * @param user 用户信息
 * @return 添加成功返回true，失败返回false
 * 
 * 将用户信息插入xms_auth表
 */
bool XAuthDao::AddUser(const xmsg::XAddUserReq *user)
{
    LOGDEBUG("XAuthDao::AddUser");

    XMutex mux(&auth_mutex);
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }

    // 准备用户数据
    XDATA data;
    data["xms_username"] = user->username().c_str();
    data["xms_password"] = user->password().c_str();
    data["xms_rolename"] = user->rolename().c_str();

    // 插入数据库
    if (!my_->Insert(data, "xms_auth"))
    {
        LOGERROR("Insert xms_auth failed!");
        return false;
    }
    return true;
}

/**
 * @brief 修改密码
 * @param pass 密码修改请求
 * @return 修改成功返回true，失败返回false
 * 
 * 更新xms_auth表中指定用户的密码
 */
bool XAuthDao::ChangePassword(const xmsg::XChangePasswordReq *pass)
{
    LOGDEBUG("XAuthDao::ChangePassword");

    XMutex mux(&auth_mutex);
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }

    // 验证旧密码是否正确
    string check_where = " where xms_username='" + pass->username() +
                         "' and xms_password='" + pass->password() + "'";
    auto rows = my_->GetResult(("select xms_username from xms_auth" + check_where).c_str());
    if (rows.empty())
    {
        LOGERROR("Old password verification failed!");
        return false;
    }

    // 准备更新数据
    XDATA data;
    data["xms_password"] = pass->new_password().c_str();
    string where = " where xms_username='" + pass->username() + "'";

    // 更新数据库
    if (!my_->Update(data, "xms_auth", where))
    {
        LOGERROR("Update xms_auth failed!");
        return false;
    }
    return true;
}

/**
 * @brief 安装数据库表
 * @return 安装成功返回true，失败返回false
 * 
 * 创建xms_auth和xms_token两张表（如果不存在）
 */
bool XAuthDao::Install()
{
    LOGDEBUG("XAuthDao::Install()");

    XMutex mux(&auth_mutex);
    if (!my_)
    {
        LOGERROR("mysql not init");
        return false;
    }

    string sql = "";

    // 创建用户认证表
    sql = "CREATE TABLE IF NOT EXISTS `xms_auth` ( \
        `id` INT AUTO_INCREMENT,\
        `xms_username` VARCHAR(128),\
        `xms_password` VARCHAR(1024),\
        `xms_rolename` VARCHAR(128),\
        PRIMARY KEY(`id`),\
        UNIQUE KEY `xms_username_UNIQUE` (`xms_username`)\
        );";

    if (!my_->Query(sql.c_str()))
    {
        LOGINFO("CREATE TABLE xms_auth failed!");
        return false;
    }
    LOGINFO("CREATE TABLE xms_auth success!");
    
    // 创建token表
    sql = "CREATE TABLE IF NOT EXISTS `xms_token` ( \
        `id` INT AUTO_INCREMENT,\
        `xms_username` VARCHAR(1024),\
        `xms_rolename` VARCHAR(128),\
        `token` VARCHAR(64),\
        `expired_time` int,\
        PRIMARY KEY(`id`));";

    if (!my_->Query(sql.c_str()))
    {
        LOGINFO("CREATE TABLE xms_token failed!");
        return false;
    }
    LOGINFO("CREATE TABLE xms_token success!");

    return true;
}


/**
 * @brief 验证Token有效性
 * @param head 消息头（包含token）
 * @param user_res 用户响应对象
 * @return 验证成功返回true，失败返回false
 * 
 * 根据token查询xms_token表，验证token是否有效并获取用户信息
 */
bool XAuthDao::CheckToken(const xmsg::XMsgHead *head, xmsg::XLoginRes *user_res)
{
    LOGDEBUG("XAuthDao::CheckToken");
    string token = "";

    // 检查消息头是否有效
    if (!head || head->token().empty())
    {
        token = "token is null";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    token = head->token();
    user_res->set_res(XLoginRes::ERROR);

    XMutex mux(&auth_mutex);
    if (!my_)
    {
        token = "mysql not init";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 查询token信息
    string table_name = "xms_token";
    stringstream ss;
    ss << "select xms_username, xms_rolename, expired_time from " << table_name;
    ss << " where token='" << token << "'";
    
    auto rows = my_->GetResult(ss.str().c_str());
    if (rows.size() == 0)
    {
        token = "token invalid!";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 设置用户信息
    user_res->set_username(rows[0][0].data);
    user_res->set_rolename(rows[0][1].data);
    user_res->set_res(XLoginRes::OK);
    return true;
}

/**
 * @brief 用户登录
 * @param user_req 登录请求（包含用户名和密码）
 * @param user_res 登录响应（包含token和用户信息）
 * @param timeout_sec token过期时间（秒）
 * @return 登录成功返回true，失败返回false
 * 
 * 验证用户名密码，生成token并存储到xms_token表
 */
bool XAuthDao::Login(const xmsg::XLoginReq *user_req, xmsg::XLoginRes *user_res, int timeout_sec)
{
    LOGDEBUG("XAuthDao::Login");
    string token = "";
    user_res->set_res(XLoginRes::ERROR);
    XMutex mux(&auth_mutex);
    if (!my_)
    {
        
        token = "mysql not init";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }
    // 检查用户名是否为空
    if (user_req->username().empty())
    {
        token = "user_name is empty!";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 验证用户名和密码
    string table_name = "xms_auth";
    stringstream ss;
    ss << "select xms_username, xms_rolename from " << table_name;
    ss << " where xms_username='" << user_req->username() << "' and xms_password='" << user_req->password() << "'";
    
    auto rows = my_->GetResult(ss.str().c_str());
    if (rows.size() == 0)
    {
        token = "username or password error!";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 获取用户信息
    string rolename = rows[0][1].data;
    string username = user_req->username();
    user_res->set_rolename(rolename);
    user_res->set_username(username);

    // 生成token过期时间
    int now = time(0);
    int expired_time = now + timeout_sec;

    // 插入token记录
    XDATA data;
    data["@token"] = "UUID()";
    data["xms_username"] = username.c_str();
    data["xms_rolename"] = rolename.c_str();
    ss.str("");
    ss << expired_time;
    string expired = ss.str();
    data["expired_time"] = expired.c_str();

    if (!my_->Insert(data, "xms_token"))
    {
        token = "Insert token failed!";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 获取刚插入的token
    int id = my_->GetInsertID();
    ss.str("");
    ss << "select token, expired_time from xms_token where id=" << id;
    rows = my_->GetResult(ss.str().c_str());
    
    if (rows.size() <= 0 || rows[0][0].data == NULL || rows[0][0].size <= 0)
    {
        token = "Insert token id error!";
        LOGERROR(token);
        user_res->set_token(token);
        return false;
    }

    // 设置登录成功响应
    user_res->set_res(XLoginRes::OK);
    token = rows[0][0].data;
    user_res->set_token(token);
    user_res->set_expired_time(atoi(rows[0][1].data));

    // 清理过期token（可考虑移到定时任务）
    ss.str("");
    ss << "delete from xms_token where expired_time < " << now;
    if (!my_->Query(ss.str().c_str()))
    {
        LOGDEBUG(ss.str().c_str());
    }

    return true;
}

/**
 * @brief XAuthDao构造函数
 */
XAuthDao::XAuthDao()
{
}

/**
 * @brief XAuthDao析构函数
 */
XAuthDao::~XAuthDao()
{
}
