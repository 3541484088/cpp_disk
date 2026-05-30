/**
 * @file LXMysql.cpp
 * @brief MySQL数据库操作封装实现
 * 
 * 封装MySQL C API，提供简洁的数据库操作接口
 */
#include <mysql.h>
#include "LXMysql.h"

#include <iostream>
#include <fstream>
using namespace std;

// 平台相关的配置文件路径
#ifdef _WIN32
#include <conio.h>
#include <windows.h>

static std::string GetMysqlConfigPath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exe_path(path);
    size_t pos = exe_path.find_last_of("\\/");
    if (pos != std::string::npos)
        return exe_path.substr(0, pos + 1) + "xms_mysql_init.conf";
    return "xms_mysql_init.conf";
}

/**
 * @brief 获取密码输入（Windows平台）
 * @param out 输出缓冲区
 * @param out_size 缓冲区大小
 * @return 实际输入的字符数
 */
static int GetPassword(char *out, int out_size)
{
    for (int i = 0; i < out_size; i++)
    {
        char p = _getch();
        if (p == '\r' || p == '\n')
        {
            return i;
        }
        cout << "*" << flush;
        out[i] = p;
    }
    return 0;
}
#else
static std::string GetMysqlConfigPath()
{
    return "/etc/xms_mysql_init.conf";
}

/**
 * @brief 获取密码输入（Linux平台）
 * @param out 输出缓冲区
 * @param out_size 缓冲区大小
 * @return 实际输入的字符数
 */
static int GetPassword(char *out, int out_size)
{
    bool is_begin = false;
    for (int i = 0; i < out_size; )
    {
        system("stty -echo");
        char p = cin.get();
        if (p != '\r' && p != '\n')
            is_begin = true;
        if (!is_begin) continue;
        system("stty echo");
        if (p == '\r' || p == '\n')
            return i;
        cout << "*" << flush;
        out[i] = p;
        i++;
    }
    return 0;
}
#endif

namespace LX {

    /**
     * @brief MySQL连接信息结构体
     */
    struct MysqlInfo
    {
        char host[128] = { 0 };
        char user[128] = { 0 };
        char pass[128] = { 0 };
        char db_name[128] = { 0 };
        int port = 3306;
    };

    /**
     * @brief 接收用户输入数据库配置
     * @return 连接成功返回true
     * 
     * 从配置文件读取或交互式输入数据库连接信息
     */
    bool LXMysql::InputDBConfig()
    {
        if (!mysql && !Init())
        {
            cerr << "InputDBConfig failed! msyql is not init!" << endl;
            return false;
        }
        
        // 如果输入过就不用输入
        ifstream ifs;
        MysqlInfo db;
        ifs.open(GetMysqlConfigPath().c_str(), ios::binary);
        if (ifs.is_open())
        {
            ifs.read((char *)&db, sizeof(db));
            if (ifs.gcount() == sizeof(db))
            {
                ifs.close();
                return Connect(db.host, db.user, db.pass, db.db_name, db.port);
            }
            ifs.close();
        }
        
        // 交互式输入数据库配置
        cout << "input the db set" << endl;
        cout << "input db host:";
        cin >> db.host;
        cout << "input db user:";
        cin >> db.user;
        cout << "input db pass:";
        GetPassword(db.pass, sizeof(db.pass) - 1);
        cout << endl;
        cout << "input db dbname(xms):";
        cin >> db.db_name;
        cout << "input db port(3306):";
        cin >> db.port;
        
        // 保存配置到文件
        ofstream ofs;
        ofs.open(GetMysqlConfigPath().c_str(), ios::binary);
        if (ofs.is_open())
        {
            ofs.write((char *)&db, sizeof(db));
            ofs.close();
        }

        return Connect(db.host, db.user, db.pass, db.db_name, db.port);
    }

    /**
     * @brief 初始化MySQL连接
     * @return 初始化成功返回true
     */
    bool LXMysql::Init()
    {
        Close();
        cout << "LXMysql::Init()" << endl;
        // 新创建一个MYSQL对象
        mysql = mysql_init(0);
        if (!mysql)
        {
            cerr << "mysql_init failed!" << endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 清理占用的所有资源
     */
    void LXMysql::Close()
    {
        FreeResult();

        if (mysql)
        {
            mysql_close((MYSQL*)mysql);
            mysql = NULL;
        }
        cout << "LXMysql::Close()" << endl;
    }

    /**
     * @brief 数据库连接
     * @param host 主机地址
     * @param user 用户名
     * @param pass 密码
     * @param db 数据库名
     * @param port 端口号
     * @param flag 连接标志（支持多条语句）
     * @return 连接成功返回true
     */
    bool LXMysql::Connect(const char *host, const char *user, const char *pass, const char *db, unsigned short port, unsigned long flag)
    {
        if (!mysql && !Init())
        {
            cerr << "Mysql connect failed! msyql is not init!" << endl;
            return false;
        }
        if (!mysql_real_connect((MYSQL*)mysql, host, user, pass, db, port, 0, flag))
        {
            cerr << "Mysql connect failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        // 设置连接字符集为UTF-8，确保中文数据正确存储
        mysql_set_character_set((MYSQL*)mysql, "utf8mb4");
        cout << "mysql connect success!" << endl;
        return true;
    }

    /**
     * @brief 执行SQL查询
     * @param sql SQL语句
     * @param sqllen SQL语句长度
     * @return 执行成功返回true
     */
    bool LXMysql::Query(const char *sql, unsigned long sqllen)
    {
        if (!mysql)
        {
            cerr << "Query failed:mysql is NULL" << endl;
            return false;
        }
        if (!sql)
        {
            cerr << "sql is null" << endl;
            return false;
        }
        if (sqllen <= 0)
            sqllen = (unsigned long)strlen(sql);
        if (sqllen <= 0)
        {
            cerr << "Query sql is empty or wrong format!" << endl;
            return false;
        }

        int re = mysql_real_query((MYSQL*)mysql, sql, sqllen);
        if (re != 0)
        {
            cerr << "mysql_real_query failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 设置MySQL参数
     * @param opt 参数选项
     * @param arg 参数值
     * @return 设置成功返回true
     */
    bool LXMysql::Options(LX_OPT opt, const void *arg)
    {
        if (!mysql)
        {
            cerr << "Option failed:mysql is NULL" << endl;
            return false;
        }
        int re = mysql_options((MYSQL*)mysql, (mysql_option)opt, arg);
        if (re != 0)
        {
            cerr << "mysql_options failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 设置连接超时时间
     * @param sec 超时秒数
     * @return 设置成功返回true
     */
    bool LXMysql::SetConnectTimeout(int sec)
    {
        return Options(LX_OPT_CONNECT_TIMEOUT, &sec);
    }

    /**
     * @brief 设置自动重连
     * @param isre 是否自动重连
     * @return 设置成功返回true
     */
    bool LXMysql::SetReconnect(bool isre)
    {
        return Options(LX_OPT_RECONNECT, &isre);
    }

    /**
     * @brief 存储全部结果
     * @return 成功返回true
     */
    bool LXMysql::StoreResult()
    {
        if (!mysql)
        {
            cerr << "StoreResult failed:mysql is NULL" << endl;
            return false;
        }
        FreeResult();
        result = mysql_store_result((MYSQL*)mysql);
        if (!result)
        {
            cerr << "mysql_store_result failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 开始接收结果
     * @return 成功返回true
     */
    bool LXMysql::UseResult()
    {
        if (!mysql)
        {
            cerr << "UseResult failed:mysql is NULL" << endl;
            return false;
        }
        FreeResult();
        result = mysql_use_result((MYSQL*)mysql);
        if (!result)
        {
            cerr << "mysql_use_result failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 释放结果集占用的空间
     */
    void LXMysql::FreeResult()
    {
        if (result)
        {
            mysql_free_result((MYSQL_RES*)result);
            result = NULL;
        }
    }

    /**
     * @brief 获取一行数据
     * @return 数据行向量
     */
    std::vector<LXData> LXMysql::FetchRow()
    {
        std::vector<LXData> re;
        if (!result)
        {
            return re;
        }
        MYSQL_ROW row = mysql_fetch_row((MYSQL_RES*)result);
        if (!row)
        {
            return re;
        }

        // 列数
        int num = mysql_num_fields((MYSQL_RES*)result);
        unsigned long *lens = mysql_fetch_lengths((MYSQL_RES*)result);
        for (int i = 0; i < num; i++)
        {
            LXData data;
            if (row[i])
            {
                data.data = row[i];
                data.size = lens ? lens[i] : 0;
            }
            else
            {
                data.data = "";
                data.size = 0;
            }
            auto field = mysql_fetch_field_direct((MYSQL_RES*)result, i);
            if (field)
                data.type = (FIELD_TYPE)field->type;
            re.push_back(data);
        }
        return re;
    }

    /**
     * @brief 生成INSERT SQL语句
     * @param kv 字段键值对
     * @param table 表名
     * @return SQL语句字符串
     */
    std::string LXMysql::GetInsertSql(XDATA kv, std::string table)
    {
        string sql = "";
        if (kv.empty() || table.empty())
            return "";
        sql = "insert into `";
        sql += table;
        sql += "`";
        string keys = "";
        string vals = "";

        // 迭代map
        for (auto ptr = kv.begin(); ptr != kv.end(); ptr++)
        {
            // 字段名
            keys += "`";
            // 去掉@（@前缀表示函数调用）
            if (ptr->first[0] == '@')
                keys += ptr->first.substr(1, ptr->first.size() - 1);
            else
                keys += ptr->first;
            keys += "`,";
            if (ptr->first[0] == '@')
            {
                vals += ptr->second.data;
            }
            else
            {
                vals += "'";
                vals += ptr->second.data;
                vals += "'";
            }
            vals += ",";
        }
        // 去除多余的逗号
        keys[keys.size() - 1] = ' ';
        vals[vals.size() - 1] = ' ';

        sql += "(";
        sql += keys;
        sql += ")values(";
        sql += vals;
        sql += ")";
        return sql;
    }

    /**
     * @brief 插入非二进制数据
     * @param kv 字段键值对
     * @param table 表名
     * @return 插入成功返回true
     */
    bool LXMysql::Insert(XDATA kv, std::string table)
    {
        if (!mysql)
        {
            cerr << "Insert failed:mysql is NULL" << endl;
            return false;
        }
        string sql = GetInsertSql(kv, table);
        cout << sql << endl;
        if (sql.empty())
            return false;
        if (!Query(sql.c_str()))
            return false;
        int num = mysql_affected_rows((MYSQL*)mysql);
        if (num <= 0)
            return false;
        return true;
    }

    /**
     * @brief 获取上一次插入的ID号
     * @return 插入ID
     */
    int LXMysql::GetInsertID()
    {
        if (!mysql)
        {
            cerr << "GetInsertID failed:mysql is NULL" << endl;
            return 0;
        }
        return mysql_insert_id((MYSQL*)mysql);
    }

    /**
     * @brief 插入二进制数据
     * @param kv 字段键值对
     * @param table 表名
     * @return 插入成功返回true
     */
    bool LXMysql::InsertBin(XDATA kv, std::string table)
    {
        string sql = "";
        if (kv.empty() || table.empty() || !mysql)
            return false;
        sql = "insert into `";
        sql += table;
        sql += "`";
        string keys = "";
        string vals = "";
        // 绑定字段
        MYSQL_BIND bind[256] = { 0 };
        int i = 0;
        // 迭代map
        for (auto ptr = kv.begin(); ptr != kv.end(); ptr++)
        {
            // 字段名
            keys += "`";
            keys += ptr->first;
            keys += "`,";

            vals += "?,";
            bind[i].buffer = (char*)ptr->second.data;
            bind[i].buffer_length = ptr->second.size;
            bind[i].buffer_type = (enum_field_types)ptr->second.type;
            i++;
        }
        // 去除多余的逗号
        keys[keys.size() - 1] = ' ';
        vals[vals.size() - 1] = ' ';

        sql += "(";
        sql += keys;
        sql += ")values(";
        sql += vals;
        sql += ")";
        
        // 预处理SQL语句
        MYSQL_STMT *stmt = mysql_stmt_init((MYSQL*)mysql);
        if (!stmt)
        {
            cerr << "mysql_stmt_init failed!" << mysql_error((MYSQL*)mysql) << endl;
            return false;
        }
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_prepare failed!" << mysql_stmt_error(stmt) << endl;
            return false;
        }

        if (mysql_stmt_bind_param(stmt, bind) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_bind_param failed!" << mysql_stmt_error(stmt) << endl;
            return false;
        }
        if (mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_execute failed!" << mysql_stmt_error(stmt) << endl;
            return false;
        }
        mysql_stmt_close(stmt);
        return true;
    }

    /**
     * @brief 获取更新数据的SQL语句
     * @param kv 字段键值对
     * @param table 表名
     * @param where WHERE条件（用户要包含where）
     * @return SQL语句字符串
     */
    std::string LXMysql::GetUpdateSql(XDATA kv, std::string table, std::string where)
    {
        string sql = "";
        if (kv.empty() || table.empty())
            return "";
        sql = "update `";
        sql += table;
        sql += "` set ";
        for (auto ptr = kv.begin(); ptr != kv.end(); ptr++)
        {
            sql += "`";
            sql += ptr->first;
            sql += "`='";
            sql += ptr->second.data;
            sql += "',";
        }
        // 去除多余的逗号
        sql[sql.size() - 1] = ' ';
        sql += " ";
        sql += where;
        return sql;
    }

    /**
     * @brief 更新数据
     * @param kv 字段键值对
     * @param table 表名
     * @param where WHERE条件
     * @return 影响的行数
     */
    int LXMysql::Update(XDATA kv, std::string table, std::string where)
    {
        if (!mysql) return -1;
        string sql = GetUpdateSql(kv, table, where);
        if (sql.empty())
            return -1;
        if (!Query(sql.c_str()))
        {
            return -1;
        }
        return mysql_affected_rows((MYSQL*)mysql);
    }

    /**
     * @brief 更新二进制数据
     * @param kv 字段键值对
     * @param table 表名
     * @param where WHERE条件
     * @return 影响的行数
     */
    int LXMysql::UpdateBin(XDATA kv, std::string table, std::string where)
    {
        if (!mysql || kv.empty() || table.empty())
        {
            return -1;
        }
        string sql = "";
        sql = "update `";
        sql += table;
        sql += "` set ";
        MYSQL_BIND bind[256] = { 0 };
        int i = 0;
        for (auto ptr = kv.begin(); ptr != kv.end(); ptr++)
        {
            sql += "`";
            sql += ptr->first;
            sql += "`=?,";
            bind[i].buffer = (char*)ptr->second.data;
            bind[i].buffer_length = ptr->second.size;
            bind[i].buffer_type = (enum_field_types)ptr->second.type;
            i++;
        }
        // 去除多余的逗号
        sql[sql.size() - 1] = ' ';
        sql += " ";
        sql += where;

        // 预处理SQL语句上下文
        MYSQL_STMT *stmt = mysql_stmt_init((MYSQL*)mysql);
        if (!stmt)
        {
            cerr << "mysql_stmt_init failed!" << mysql_error((MYSQL*)mysql) << endl;
            return -1;
        }
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_prepare failed!" << mysql_error((MYSQL*)mysql) << endl;
            return -1;
        }

        if (mysql_stmt_bind_param(stmt, bind) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_bind_param failed!" << mysql_stmt_error(stmt) << endl;
            return -1;
        }
        if (mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            cerr << "mysql_stmt_execute failed!" << mysql_stmt_error(stmt) << endl;
            return -1;
        }
        int count = mysql_stmt_affected_rows(stmt);
        mysql_stmt_close(stmt);
        return count;
    }

    /**
     * @brief 开始事务
     * @return 成功返回true
     */
    bool LXMysql::StartTransaction()
    {
        return Query("set autocommit=0");
    }

    /**
     * @brief 停止事务
     * @return 成功返回true
     */
    bool LXMysql::StopTransaction()
    {
        return Query("set autocommit=1");
    }

    /**
     * @brief 提交事务
     * @return 成功返回true
     */
    bool LXMysql::Commit()
    {
        return Query("commit");
    }

    /**
     * @brief 回滚事务
     * @return 成功返回true
     */
    bool LXMysql::Rollback()
    {
        return Query("rollback");
    }

    /**
     * @brief 简易接口，返回SELECT的数据结果
     * @param sql SQL查询语句
     * @return 数据行集合
     * 
     * 每次调用清理上一次的结果集
     */
    XROWS LXMysql::GetResult(const char *sql)
    {
        FreeResult();
        XROWS rows;
        if (!Query(sql))
            return rows;
        if (!StoreResult())
            return rows;
        for (;;)
        {
            auto row = FetchRow();
            if (row.empty()) break;
            rows.push_back(row);
        }
        return rows;
    }
}
