#ifndef XCOM_TASK_H
#define XCOM_TASK_H
#include "xtask.h"
#include "xmsg.h"
#include <string>
#include "xssl_ctx.h"
XCOM_API const char * XGetPortName(unsigned short port);
class XSSLCtx;

class XCOM_API XComTask : public XTask
{
public:
    XComTask();
    virtual ~XComTask();

    // 初始化连接，发出请求调用成员 server_ip_ server_port_
    // 超时自动重试
    virtual bool Connect();
    ///初始化libbufferevent连接客户端的数据连接

    ///连接到线程池的任务列表，参数配置客户端和服务端
    virtual bool Init();

    ///清理相关资源，释放占用的头部内存auto_delete_
    virtual void Close();

    int Read(void *data, int datasize);


    void set_server_ip(const char* ip);
    const char *server_ip() { return server_ip_; }

    void set_server_port(int port) { this->server_port_ = port; }
    int server_port() { return this->server_port_; }

    //本地IP用于获取服务端
    // 客户端连接成功返回， 是谁是的进程连接到这个客户端的IP用client_ip()
    void set_local_ip(const char *ip);
    const char *local_ip() { return local_ip_; };

    //static void InitSSL();
    //virtual bool SetSSL(SSLVer ver, const char* ca_file, const char* key_file, const char *vali_ca = 0);
    //virtual void set_

    //XSSLCtx * ssl_ctx() { return ssl_ctx_; }

    ///事件回调函数
    virtual void EventCB(short what);

    //读写回调
    virtual void BeginWrite();

    //发送消息
    //virtual bool Write(const XMsg *msg);
    virtual bool Write(const void *data, int size);

    //缓存大小（未发送，）的大小
    virtual long long BufferSize();

    //添加成功连接信息回调，业务处理层
    virtual void ConnectedCB() {};

    //接收断开信息，当数据接收到的服务器断开连接，业务模块处理
    virtual void ReadCB(void *data, int size) {}

    //接收消息的回调，业务处理层 返回true，继续
    //返回false则退出当前消息处理,可以继续下一条消息
    //virtual bool ReadCB(const XMsg *msg) = 0;

    ///写完数据回调函数
    virtual void WriteCB() {};

    ///获取数据回调函数
    virtual void ReadCB() = 0;

    void set_is_recv_msg(bool isrecv) { this->is_recv_msg_ = isrecv; }


    ////////////////////////////////////////////////////////////
    ///等待连接成功
    ///@para timeout_sec 等待超时时间
    bool WaitConnected(int timeout_sec);

    /////////////////////////////////////////////////////
    ///自动连接，自动断开可以再次连接，知道提示连接成功或者超时
    bool AutoConnect(int timeout_sec);


    bool is_connecting() { return is_connecting_; }
    bool is_connected() { return is_connected_; }

    //连接断开是否自动释放资源（删除对象）
    void set_auto_delete(bool is) { auto_delete_ = is; }

    //是否自动连接 默认不断开 要设置在添加到线程池之前
    //如果自动连接 对象就不会自动释放
    void set_auto_connect(bool is)
    {
        auto_connect_ = is;
        if(is)//自动连接 线程就不会自动释放
            auto_delete_ = false;
    }

    //////////////////////////////////////////////////////////////
    ///设定定时器 只运行一次，超时调用TimerCB()回调
    /// 在Init函数中调用 连接建成之前不生效，通过set_timer_ms配置参数
    ///@para ms 定时器调用的豪秒
    virtual void SetTimer(int ms);

    ///清理所有定时器
    virtual void ClearTimer();

    /////////////////////////////////////////
    ///定时器回调函数
    virtual void TimerCB() {}

    //////////////////////////////////////////////////////////////
    /// 设定自动重连的定时器
    virtual void SetAutoConnectTimer(int ms);

    /////////////////////////////////////////
    ///自动重连定时器回调函数
    virtual void AutoConnectTimerCB() ;



    //void set_ssl(struct ssl_st *ssl) { this->ssl_ = ssl; }
    //struct ssl_st * ssl() { return ssl_; }

    //设定要在第几个线程池之前
    void set_read_timeout_ms(int ms) { read_timeout_ms_ = ms; }

    //设定要在第几个线程池之前 virtual void TimerCB() {}
    void set_timer_ms(int ms) { timer_ms_ = ms; }

    void set_client_ip(const char*ip);
    const char *client_ip() { return client_ip_; }
    const int client_port() { return client_port_; }
    void set_client_port(int port) { this->client_port_ = port; }
    bool is_closed() { return is_closed_; } //是否已经关闭，用于放自动连接，或者断开占空间

    ///是否有错误
    bool has_error() { return has_error_; }  //是否有错误

    ///错误原因 线程安全
    const char *error() { return error_; };

    // 已经写缓冲去 以XMsg *msg 发送字节大小 已发送消息头
    long long send_data_size() { return send_data_size_; }

    long long recv_data_size() { return recv_data_size_; }

protected:
    //设置错误线程安全
    void set_error(const char * err);

    char client_ip_[16] = { 0 };

    int client_port_ = 0;

    //读取缓存
    char read_buf_[4096] = { 0 };

    //本地IP用于获取服务
    char local_ip_[16] = { 0 };

    //XSSL *ssl_ = 0;
   // struct ssl_st *ssl_ = 0;
private:
    long long send_data_size_ = 0;// 已经写缓冲去 以XMsg *msg 发送字节大小
    long long recv_data_size_ = 0;

    //TimerCB 超时定时时间
    int timer_ms_ = 0;

    //读取超时时间，毫秒
    int read_timeout_ms_ = 0;

    //是否自动重连
    bool auto_connect_ = false;

    //定时器任务 close时需要清理
    struct event * auto_connect_timer_event_ = 0;

    //连接断开是否自动释放资源
    bool auto_delete_ = true;

    bool InitBev(int sock);
    /// 服务器IP
    char server_ip_[16] = {0};

    ///服务器端口
    int server_port_ = 0;

    struct bufferevent *bev_ = 0;

    //数据包任务
    XMsg msg_;

    char error_[1024] = { 0 };

    //是否有错误
    bool has_error_ = false;

    //是否关闭
    bool is_closed_ = false;

    //是否已经连接成功
    bool is_connected_ = false;

    //是否正在连接
    bool is_connecting_ = false;

    //接收消息开关
    bool is_recv_msg_ = false;

    //定时器事件
    struct event * timer_event_ = 0;

    std::mutex *mux_ = nullptr;
};

#define XMUTEX(m) ::XMutex mux__(m)

#endif