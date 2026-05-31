/**
 * @file xlog_client.cpp
 * @brief 日志客户端实现
 * 
 * 提供日志记录、发送和本地存储功能
 */
#include "xlog_client.h"
#include "xtools.h"
#include "xmsg_com.pb.h"
using namespace xmsg;
using namespace std;

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace xms
{
    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param msg 日志消息
     * @param filename 源文件名
     * @param line 行号
     */
    void XLog(xmsg::XLogLevel level, std::string msg, const char *filename, int line)
    {
         XAddLogReq req;
         req.set_log_level(level);
         req.set_log_txt(msg);
         req.set_filename(filename);
         req.set_line(line);
         XLogClient::Get()->AddLog(&req);
    }
}

/**
 * @brief 添加日志到队列
 * @param req 日志请求
 */
void XLogClient::AddLog(const xmsg::XAddLogReq *req)
{
    if (!req) return;
    if (req->log_level() < log_level_)
        return;

    
    string level_str = "Debug";
    switch (req->log_level())
    {
    case XLOG_DEBUG:
        level_str = "DEBUG";
        break;
    case XLOG_INFO:
        level_str = "INFO";
        break;
    case XLOG_ERROR:
        level_str = "ERROR";
        break;
    case XLOG_FATAL:
        level_str = "FATAL";
        break;
    default:
        break;
    }
    string log_time = XGetTime(0, "%F %T");

    // DEBUG:c:\\xdisk_lesson_test\\src\\xplatform\\xservice.cpp:33
    // accept client ip : 127.0.0.1 port : 58291
    stringstream log_text;
    log_text << "=============================================================\n";
    log_text << log_time<<" "<<level_str << "|" << req->filename() << ":" << req->line() << "\n";
    log_text << req->log_txt();

    if (is_print_)
    {
#ifdef _WIN32
        // 设置Windows控制台为UTF-8编码
        static bool utf8_set = false;
        if (!utf8_set)
        {
            // 设置控制台输入和输出编码为UTF-8
            SetConsoleCP(CP_UTF8);
            SetConsoleOutputCP(CP_UTF8);
            utf8_set = true;
        }
        
        // 将UTF-8字符串转换为宽字符并输出
        std::string log_str = log_text.str();
        log_str += "\n";
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE)
        {
            // 将UTF-8转换为宽字符
            int wlen = MultiByteToWideChar(CP_UTF8, 0, log_str.c_str(), -1, NULL, 0);
            if (wlen > 0)
            {
                std::wstring wlog_str(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, log_str.c_str(), -1, &wlog_str[0], wlen);
                
                DWORD written = 0;
                WriteConsoleW(hConsole, wlog_str.c_str(), static_cast<DWORD>(wlog_str.size() - 1), &written, NULL);
            }
        }
#else
        cout << log_text.str() << endl;
#endif
    }
        
    if (log_ofs_)
    {
        log_ofs_ << "==========================================\n";
        log_ofs_ << req->DebugString();
    }
    XAddLogReq tmp = *req;
    if (tmp.log_time() <= 0)
    {
        tmp.set_log_time(time(0));
    }

    tmp.set_service_port(service_port_);
    tmp.set_service_name(service_name_);

    XMutex mux(&logs_mutex_);
    logs_.push_back(tmp);

}

/**
 * @brief 定时器回调
 * 
 * 定期发送日志到服务器
 */
void XLogClient::TimerCB()
{
    if (!is_connected())
        return;
    for (;;)
    {
        XAddLogReq log;
        {
            XMutex mux(&logs_mutex_);
            if (logs_.empty())
                return;
            log = logs_.front();
            logs_.pop_front();
        }
        if (!SendMsg(MSG_ADD_LOG_REQ, &log))
            return;
    }
}

/**
 * @brief 启动日志服务
 * @return 启动成功返回true
 */
bool XLogClient::StartLog()
{
    // 设置默认日志服务器地址
    // 如果未配置，使用默认值
    if (strlen(server_ip()) == 0)
        set_server_ip("127.0.0.1");
    if (server_port() <= 0)
        set_server_port(XLOG_PORT);

    // 设置自动重连
    set_auto_connect(true);

    // 设置定时器时间
    set_timer_ms(100);

    // 开始连接线程
    StartConnect();

    //LoadLocalFile();
    return true;
}

XLogClient::XLogClient()
{
}

XLogClient::~XLogClient()
{
}
