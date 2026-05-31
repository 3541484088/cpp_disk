#include "xcom_task.h"

#include "xlog_client.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <iostream>
#include <string.h>
#include "xtools.h"
#include "event2/bufferevent_ssl.h"
#include <chrono>
#include <thread>
#include "xssl.h"
using namespace std;

XCOM_API const char * XGetPortName(unsigned short port)
{
    switch (port)
    {
    case API_GATEWAY_PORT:
        return API_GATEWAY_NAME;
        break;
    case REGISTER_PORT:
        return REGISTER_NAME;
        break;
    case CONFIG_PORT:
        return CONFIG_NAME;
        break;
    case XLOG_PORT:
        return XLOG_NAME;
        break;
    case DOWNLOAD_PORT:
        return DOWNLOAD_NAME;
        break;
    case DIR_PORT:
        return DIR_NAME;
        break;
    case UPLOAD_PORT:
        return UPLOAD_NAME;
        break;
    default:
        break;
    }
    return "";
}

static void SReadCB(struct bufferevent *bev, void *ctx)
{
    auto task = (XComTask*)ctx;
    task->ReadCB();
    //static int i = 0;
    //i++;
    //cout << "{"<<i<<"}" << flush;
}
static void SWriteCB(struct bufferevent *bev, void *ctx)
{
    auto task = (XComTask*)ctx;
    task->WriteCB();
}

void SAutoConnectTimerCB(evutil_socket_t s, short w, void *ctx)
{
    auto task = (XComTask*)ctx;
    task->AutoConnectTimerCB();
}
void STimerCB(evutil_socket_t s, short w, void *ctx)
{
    auto task = (XComTask*)ctx;
    task->TimerCB();
}

static void DeferredDeleteCB(evutil_socket_t, short, void *ctx)
{
    delete static_cast<XComTask *>(ctx);
}
static void SEventCB(struct bufferevent *bev, short what,void *ctx)
{
    auto task = (XComTask*)ctx;
    task->EventCB(what);
}

XComTask::XComTask()
{
    mux_ = new mutex;
}
/**
 * @brief XComTask 析构函数
 */
XComTask::~XComTask()
{
    delete mux_;
    mux_ = NULL;
}

/**
 * @brief 设置错误信息
 * @param err 错误信息字符串
 */
void XComTask::set_error(const char *err)
{
    if (!err) return;
    strncpy(error_, err, sizeof(error_));
}

/**
 * @brief 设置客户端 IP 地址
 * @param ip IP 地址字符串
 */
void XComTask::set_client_ip(const char *ip)
{
    if (!ip) return;
    strncpy(client_ip_, ip, sizeof(client_ip_));
}


//void XComTask::InitSSL()
//{
//    XSSLCtx::InitSSL();
//}

//bool XComTask::SetSSL(SSLVer ver, const char* ca_file, const char* key_file, const char *vali_ca )
//{
//    XMutex mux(mux_);
//    auto ssl_ctx = new XSSLCtx();
//    bool re = ssl_ctx->Init(ver, ca_file, key_file,vali_ca);
//    if (!re)
//    {
//        LOGERROR("XComTask::SetSSL failed!");
//        ssl_ctx->Close();
//        delete ssl_ctx;
//        return re;
//    }
//    this->ssl_ctx_ = ssl_ctx;
//    return true;
//}

/**
 * @brief 设置自动重连定时器
 * @param ms 定时器间隔（毫秒）
 * 
 * 当连接断开时，定时尝试重新连接服务器
 */
void XComTask::SetAutoConnectTimer(int ms)
{
    if (!base())
    {
        LOGERROR("SetAutoConnectTimer failed : base not set!");
        return;
    }
    if (auto_connect_timer_event_)
    {
        event_free(auto_connect_timer_event_);
        auto_connect_timer_event_ = 0;
    }

    auto_connect_timer_event_ = event_new(base(), -1, EV_PERSIST, SAutoConnectTimerCB, this);
    if (!auto_connect_timer_event_)
    {
        LOGERROR("SetAutoConnectTimer failed : event_new failed!");
        return;
    }
    int sec = ms / 1000;        // 秒
    int us = (ms % 1000) * 1000; // 微秒
    timeval tv = { sec, us };
    event_add(auto_connect_timer_event_, &tv);
}

/**
 * @brief 自动重连定时器回调
 * 
 * 如果当前未连接且未正在连接，则尝试连接服务器
 */
void XComTask::AutoConnectTimerCB()
{
    if (is_connected())
        return;
    if (!is_connecting())
    {
        Connect();
        cout << "." << flush;
    }
}

/**
 * @brief 设置定时器
 * @param ms 定时器间隔（毫秒）
 * 
 * 周期性触发 TimerCB() 回调，在 Init 之后调用
 */
void XComTask::SetTimer(int ms)
{
    if (!base())
    {
        LOGERROR("SetTimer failed : base not set!");
        return;
    }

    if (timer_event_)
    {
        event_free(timer_event_);
        timer_event_ = 0;
    }

    timer_event_ = event_new(base(), -1, EV_PERSIST, STimerCB, this);
    if (!timer_event_)
    {
        LOGERROR("set timer failed : event_new failed!");
        return;
    }
    int sec = ms / 1000;        // 秒
    int us = (ms % 1000) * 1000; // 微秒
    timeval tv = { sec, us };
    event_add(timer_event_, &tv);
}
void XComTask::set_local_ip(const char *ip)
{
    strncpy(this->local_ip_, ip, sizeof(local_ip_));
}

void XComTask::set_server_ip(const char* ip)
{
    strncpy(this->server_ip_, ip, sizeof(server_ip_));
}

int XComTask::Read(void *data, int datasize)
{
    if (!bev_)
    {
        LOGERROR("bev not set");
        return 0;
    }
    int re = bufferevent_read(bev_, data, datasize);
    if(re>0)
        recv_data_size_ += re;
    return re;
}

/**
 * @brief 清除所有定时器
 */
void XComTask::ClearTimer()
{
    if (auto_connect_timer_event_)
        event_free(auto_connect_timer_event_);
    auto_connect_timer_event_ = 0;
    if (timer_event_)
        event_free(timer_event_);
    timer_event_ = 0;
}

/**
 * @brief 关闭连接并释放资源
 * 
 * 如果设置了 auto_delete，会通过延迟删除机制自动释放对象
 */
void XComTask::Close()
{
    {
        XMutex mux(mux_);
        if (is_closed_) return;
        is_connected_ = false;
        is_connecting_ = false;
        is_closed_ = true;
        if (bev_)
        {
            // BEV_OPT_CLOSE_ON_FREE 标志会自动关闭 ssl 和 socket
            // 但可能不会立即释放，需要通过 event_base_loop 轮询来清理
            bufferevent_free(bev_);
            bev_ = NULL;
        }

        if (msg_.data)
            delete[] msg_.data;
        msg_.data = NULL;
        msg_.size = 0;
    }
    // 如果启用自动删除，通过延迟删除机制释放对象，避免在回调中直接删除
    if (auto_delete_)
    {
        ClearTimer();
        auto *self = this;
        set_auto_delete(false);
        if (base())
        {
            timeval tv = { 0, 0 };
            event_base_once(base(), -1, EV_TIMEOUT, DeferredDeleteCB, self, &tv);
        }
        else
        {
            delete self;
        }
    }
        
}

/**
 * @brief 获取输出缓冲区大小（未发送的数据量）
 * @return 缓冲区中待发送的数据字节数
 */
long long XComTask::BufferSize()
{
    XMutex mux(mux_);
    if (!bev_) return 0;
    auto evbuf = bufferevent_get_output(bev_);
    auto len = evbuffer_get_length(evbuf);
    return len;
}

/**
 * @brief 向缓冲区写入数据
 * @param data 数据指针
 * @param size 数据长度
 * @return 写入成功返回 true，失败返回 false
 */
bool XComTask::Write(const void *data, int size)
{
    XMutex mux(mux_);
    if (!bev_ || !data || size <= 0) return false;
    int re = bufferevent_write(bev_, data, size);
    if (re != 0) return false;
    send_data_size_ += size;
    return true;
}

/**
 * @brief 触发写事件
 * 
 * 强制触发写回调，用于唤醒等待写入的操作
 */
void XComTask::BeginWrite()
{
    if (!bev_) return;
    bufferevent_trigger(bev_, EV_WRITE, 0);
}

/**
 * @brief 事件回调函数
 * @param what 事件标志位
 * 
 * 处理连接建立、错误、超时、断开等事件
 */
void XComTask::EventCB(short what)
{
    stringstream ss;
    ss << "SEventCB:" << what;
    
    if (what & BEV_EVENT_CONNECTED)
    {
        stringstream ss;
        ss << "connect server " << server_ip_ << ":" << server_port_ << " " << XGetPortName(server_port_) << " success!";
        LOGINFO(ss.str().c_str());
        
        // 更新连接状态
        is_connected_ = true;
        is_connecting_ = false;
        
        // 如果是 SSL 连接，打印证书信息
        auto ssl = bufferevent_openssl_get_ssl(bev_);
        if (ssl)
        {
            XSSL xssl;
            xssl.set_ssl(ssl);
            xssl.PrintCert();
            xssl.PrintCipher();
        }
        
        // 调用连接成功回调
        ConnectedCB();
    }

    // 处理连接错误
    if (what & BEV_EVENT_ERROR)
    {
        auto ssl = bufferevent_openssl_get_ssl(bev_);
        if (ssl)
        {
            XSSL xssl;
            xssl.set_ssl(ssl);
            xssl.PrintCert();
        }
        ss << " BEV_EVENT_ERROR ";
        int sock = bufferevent_getfd(bev_);
        int err = evutil_socket_geterror(sock);
        ss << server_ip() << ":" << server_port() << " " << XGetPortName(server_port());
        ss << " " << local_ip() << " " << XGBKToUTF8(evutil_socket_error_to_string(err));
        LOGINFO(ss.str().c_str());
        has_error_ = true;
        std::string utf8_error = XGBKToUTF8(evutil_socket_error_to_string(err));
        strcpy(error_, utf8_error.c_str());
        Close();
    }
    
    // 处理超时事件
    if (what & BEV_EVENT_TIMEOUT)
    {
        ss << " BEV_EVENT_TIMEOUT";
        LOGINFO(ss.str().c_str());
        has_error_ = true;
        strcpy(error_, "BEV_EVENT_TIMEOUT");
        Close();
    }
    
    // 处理连接断开
    if (what & BEV_EVENT_EOF)
    {
        ss << " BEV_EVENT_EOF";
        LOGINFO(ss.str().c_str());
        Close();
    }
}
/**
 * @brief 连接到服务器
 * @return 连接发起成功返回 true，失败返回 false
 * 
 * 注意：这是异步连接，连接结果通过 EventCB 回调通知
 */
bool XComTask::Connect()
{
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(server_port_);
    evutil_inet_pton(AF_INET, server_ip_, &sin.sin_addr.s_addr);
    
    XMutex mux(mux_);
    is_connected_ = false;
    is_connecting_ = false;
    
    // 初始化 bufferevent（如果尚未初始化）
    if (!bev_) InitBev(-1);
    if (!bev_)
    {
        LOGERROR("XComTask::Connect failed! bev is null!");
        return false;
    }
    
    int re = bufferevent_socket_connect(bev_, (sockaddr*)&sin, sizeof(sin));
    if (re != 0)
    {
        return false;
    }

    // 标记为正在连接状态
    is_connecting_ = true;
    return true;
}

/**
 * @brief 初始化 bufferevent
 * @param sock socket 描述符（-1 表示由 bufferevent 创建）
 * @return 初始化成功返回 true，失败返回 false
 * 
 * 根据是否配置 SSL 上下文，创建普通或 SSL 类型的 bufferevent
 */
bool XComTask::InitBev(int sock)
{
    // 创建 bufferevent，-1 表示由 bufferevent 自行创建 socket
    if (!ssl_ctx())
    {
        // 普通 TCP 连接
        bev_ = bufferevent_socket_new(base(), sock, BEV_OPT_CLOSE_ON_FREE);
        if (!bev_)
        {
            LOGERROR("bufferevent_socket_new failed!");
            return false;
        }
    }
    else
    {
        // SSL/TLS 加密连接
        auto xssl = ssl_ctx()->NewXSSL(sock);
        
        if (sock < 0)
        {
            // 客户端模式
            bev_ = bufferevent_openssl_socket_new(base(), sock, xssl.ssl(),
                BUFFEREVENT_SSL_CONNECTING,
                BEV_OPT_CLOSE_ON_FREE  // bufferevent_free 时同时关闭 socket 和 ssl
            );
        }
        else
        {
            // 服务端模式
            bev_ = bufferevent_openssl_socket_new(base(), sock, xssl.ssl(),
                BUFFEREVENT_SSL_ACCEPTING,
                BEV_OPT_CLOSE_ON_FREE  // bufferevent_free 时同时关闭 socket 和 ssl
            );
        }
        
        if (!bev_)
        {
            LOGERROR("bufferevent_openssl_socket_new failed!");
            return false;
        }
    }

    // 设置读取超时
    if (read_timeout_ms_ > 0)
    {
        timeval read_tv = { read_timeout_ms_ / 1000,
            (read_timeout_ms_ % 1000) * 1000 };
        bufferevent_set_timeouts(bev_, &read_tv, 0);
    }

    // 设置定时器
    if (timer_ms_ > 0)
    {
        SetTimer(timer_ms_);
    }

    // 设置回调函数
    bufferevent_setcb(bev_, SReadCB, SWriteCB, SEventCB, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);
    return true;
}

/**
 * @brief 初始化通信任务
 * @return 初始化成功返回 true，失败返回 false
 * 
 * 创建 bufferevent 并根据配置决定是否立即连接服务器
 */
bool XComTask::Init()
{
    int comsock = this->sock();
    if (comsock <= 0)
        comsock = -1;
    
    {
        XMutex mux(mux_);
        InitBev(comsock);
    }

    // 如果没有配置服务器地址，仅初始化不连接
    if (server_ip_[0] == '\0')
    {
        return true;
    }
    
    // 配置自动重连定时器（3秒间隔）
    SetAutoConnectTimer(3000);

    // 尝试连接服务器
    return Connect();
}

/**
 * @brief 自动连接并等待连接完成
 * @param timeout_sec 超时时间（秒）
 * @return 连接成功返回 true，超时返回 false
 * 
 * 如果当前未连接，尝试连接并阻塞等待直到连接成功或超时
 */
bool XComTask::AutoConnect(int timeout_sec)
{
    // 如果已连接，直接返回成功
    if (is_connected())
        return true;
    // 如果未正在连接，发起连接
    if (!is_connecting())
        Connect();
    // 等待连接完成
    return WaitConnected(timeout_sec);
}

/**
 * @brief 阻塞等待连接完成
 * @param timeout_sec 超时时间（秒）
 * @return 连接成功返回 true，超时返回 false
 * 
 * 通过轮询方式等待连接状态变为已连接
 */
bool XComTask::WaitConnected(int timeout_sec)
{
    // 每 10 毫秒检查一次，总共检查 timeout_sec * 100 次
    int count = timeout_sec * 100;
    for (int i = 0; i < count; i++)
    {
        if (is_connected())
            return true;
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    return is_connected();
}
