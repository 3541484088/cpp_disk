#include "xthread.h"
#include "xlog_client.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <event2/event.h>
#include "xtask.h"
#include "xtools.h"
#ifdef _WIN32
#else
#include <unistd.h>
#endif

using namespace std;

// 线程唤醒的回调函数
static void NotifyCB(evutil_socket_t fd, short which, void *arg)
{
    XThread *t = (XThread *)arg;
    t->Notify(fd, which);
}

/**
 * @class CXThread
 * @brief 线程实现类，继承自XThread接口
 * 
 * 该类实现了基于libevent的线程模型，支持任务队列管理和线程唤醒机制。
 * 使用socketpair(Windows)或pipe(Linux)实现线程间通信。
 */
class CXThread :public XThread
{
public:
    /**
     * @brief 处理线程唤醒通知
     * @param fd 唤醒事件的文件描述符
     * @param which 事件类型
     * 
     * 当收到唤醒信号时，从任务队列中取出任务并执行初始化。
     * 如果任务队列为空，则直接返回。
     */
    void Notify(int fd, short which)
    {
        // 唤醒通知，没有收到完整数据会再次进入
        char buf[2] = { 0 };
#ifdef _WIN32
        int re = recv(fd, buf, 1, 0);
#else
        // linux中是管道，不能用recv
        int re = read(fd, buf, 1);
#endif
        if (re <= 0)
            return;

        stringstream ss;
        ss << id << " thread " << buf;
        LOGDEBUG(ss.str().c_str());
        XTask *task = NULL;
        // 获取任务并初始化执行
        tasks_mutex_.lock();
        if (tasks_.empty())
        {
            tasks_mutex_.unlock();
            return;
        }
        task = tasks_.front(); // 先进先出
        tasks_.pop_front();
        tasks_mutex_.unlock();
        task->Init();
    }

    /**
     * @brief 添加任务到任务队列
     * @param t 要添加的任务对象指针
     * 
     * 将任务添加到线程的任务队列中，线程被唤醒后会依次执行队列中的任务。
     */
    void AddTask(XTask *t)
    {
        if (!t) return;
        t->set_base(this->base_);
        tasks_mutex_.lock();
        tasks_.push_back(t);
        tasks_mutex_.unlock();
    }

    /**
     * @brief 唤醒线程
     * 
     * 通过向管道写入数据来唤醒线程，使其处理任务队列中的任务。
     */
    void Activate()
    {
#ifdef _WIN32
        int re = send(this->notify_send_fd_, "c", 1, 0);
#else
        int re = write(this->notify_send_fd_, "c", 1);
#endif
        if (re <= 0)
        {
            LOGERROR("XThread::Activate() failed!");
        }
    }

    /**
     * @brief 启动线程
     * 
     * 调用Setup()初始化线程环境，然后创建并分离新线程。
     */
    void Start()
    {
        Setup();
        // 启动线程
        thread th(&CXThread::Main, this);

        // 新线程与主线程分离
        th.detach();
    }

    /**
     * @brief 安装线程，初始化event_base和管道唤醒事件
     * @return true表示初始化成功，false表示失败
     * 
     * 在Windows上使用socketpair创建一对套接字用于线程间通信，
     * 在Linux上使用pipe创建管道。然后初始化libevent的event_base。
     */
    bool Setup()
    {
        // windows使用socketpair，linux使用管道
#ifdef _WIN32
        // 创建一个socketpair，可以互相通信，fds[0]用于读，fds[1]用于写
        evutil_socket_t fds[2];
        if (evutil_socketpair(AF_INET, SOCK_STREAM, 0, fds) < 0)
        {
            LOGERROR("evutil_socketpair failed!");
            return false;
        }
        // 设置成非阻塞模式
        evutil_make_socket_nonblocking(fds[0]);
        evutil_make_socket_nonblocking(fds[1]);
#else
        // 创建的管道不能用send/recv读写，应使用read/write
        int fds[2];
        if (pipe(fds))
        {
            LOGERROR("pipe failed!");
            return false;
        }
#endif

        // 读取端绑定到event事件中，写入端用于唤醒
        notify_send_fd_ = fds[1];

        // 创建libevent上下文（无锁模式，用于多线程环境）
        event_config *ev_conf = event_config_new();
        // 需要考虑打开锁，多线程调用时
        event_config_set_flag(ev_conf, EVENT_BASE_FLAG_NOLOCK);
        this->base_ = event_base_new_with_config(ev_conf);
        event_config_free(ev_conf);
        if (!base_)
        {
            LOGERROR("event_base_new_with_config failed in thread!");
            return false;
        }

        // 添加管道监听事件，用于唤醒线程执行任务
        event *ev = event_new(base_, fds[0], EV_READ | EV_PERSIST, NotifyCB, this);
        event_add(ev, 0);

        return true;
    }

private:
    /**
     * @brief 线程入口函数
     * 
     * 线程主循环，不断处理event事件和任务队列，直到收到退出信号。
     */
    void Main()
    {
        stringstream ss;
        ss << id << " XThread::Main() begin" << endl;
        LOGDEBUG(ss.str().c_str());
        if (!base_)
        {
            cerr << "XThread::Main failed! base_ is null " << endl;
            cerr << "In windows set WSAStartup(MAKEWORD(2, 2), &wsa)" << endl;
            return;
        }

        // 设置为非阻塞分发消息
        while (!is_exit_)
        {
            // 一次处理多个事件
            event_base_loop(base_, EVLOOP_NONBLOCK);
            this_thread::sleep_for(chrono::milliseconds(1));
        }

        event_base_free(base_);

        ss.str("");
        ss << id << " XThread::Main() end" << endl;
        LOGDEBUG(ss.str().c_str());
    }

    bool is_exit_ = false;           // 线程退出标志
    int notify_send_fd_ = 0;         // 唤醒信号的写文件描述符
    struct event_base *base_ = 0;    // libevent事件基础结构

    std::list<XTask*> tasks_;       // 任务列表
    std::mutex tasks_mutex_;         // 任务列表互斥锁，保证线程安全
};

/**
 * @brief 创建XThread实例的静态工厂方法
 * @return 返回新创建的CXThread实例指针
 */
XThread * XThread::Create()
{
    return new CXThread();
}

/**
 * @brief XThread构造函数
 */
XThread::XThread()
{
}

/**
 * @brief XThread析构函数
 */
XThread::~XThread()
{
}
