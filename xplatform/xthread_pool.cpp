#include "xthread_pool.h"
#include "xthread.h"
#include <thread>
#include <chrono>
#include <iostream>
#include "xtask.h"
#include <sstream>
#include "xtools.h"
#include "xlog_client.h"
#ifdef _WIN32
// 和protobuf头文件会有冲突，protobuf的头文件需要在windows.h之前
#include <windows.h>
#else
#include <signal.h>
#endif

using namespace std;

static bool is_exit_all = false;           // 全局退出标志

static vector<XThread *> all_threads;      // 所有线程实例列表
static mutex all_threads_mutex;            // 线程列表互斥锁

/**
 * @brief 退出所有线程
 * 
 * 设置全局退出标志，然后遍历所有线程并调用其Exit()方法。
 * 等待一段时间让线程有机会完成清理工作。
 */
void XThreadPool::ExitAllThread()
{
    is_exit_all = true;
    all_threads_mutex.lock();
    for (auto t : all_threads)
    {
        t->Exit();
    }
    all_threads_mutex.unlock();
    this_thread::sleep_for(chrono::milliseconds(200));
}

/**
 * @brief 等待所有线程退出
 * 
 * 阻塞当前线程，直到全局退出标志被设置为true。
 */
void XThreadPool::Wait()
{
    while (!is_exit_all)
    {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

/**
 * @class CXThreadPool
 * @brief 线程池实现类，继承自XThreadPool接口
 * 
 * 该类实现了基于轮询调度的线程池，支持任务分发和线程管理。
 * 使用round-robin算法将任务均匀分配到各个工作线程。
 */
class CXThreadPool : public XThreadPool
{
public:
    /**
     * @brief 分发任务到线程池
     * @param task 要分发的任务对象指针
     * 
     * 使用轮询算法将任务分配到下一个线程，然后唤醒该线程处理任务。
     */
    void Dispatch(XTask *task)
    {
        if (!task) return;
        int tid = (last_thread_ + 1) % thread_count_;
        last_thread_ = tid;
        XThread *t = threads_[tid];
        t->AddTask(task);
        t->Activate();
    }

    /**
     * @brief 初始化线程池
     * @param thread_count 线程池中的线程数量
     * 
     * 创建指定数量的线程实例，并将其加入全局线程列表。
     */
    void Init(int thread_count)
    {
        this->thread_count_ = thread_count;
        this->last_thread_ = -1;
        for (int i = 0; i < thread_count; i++)
        {
            XThread *t = XThread::Create();
            t->id = i + 1;
            stringstream ss;
            ss << "Create thread " << i;
            LOGDEBUG(ss.str().c_str());
            t->Start();
            threads_.push_back(t);
            all_threads_mutex.lock();
            all_threads.push_back(t);
            all_threads_mutex.unlock();
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }

private:
    int thread_count_ = 0;                   // 线程池中的线程数量
    int last_thread_ = -1;                   // 上一次分配任务的线程索引
    std::vector<XThread *> threads_;         // 线程池中的线程列表
};

/**
 * @class XThreadPoolFactory
 * @brief 线程池工厂类
 * 
 * 提供创建线程池实例的静态方法，并负责初始化平台相关的网络环境。
 */

/**
 * @brief 创建线程池实例
 * @return 返回新创建的CXThreadPool实例指针
 * 
 * 该方法是线程安全的，使用双重检查锁定确保只初始化一次。
 * 在Windows上初始化WSA，在Linux上忽略SIGPIPE信号。
 */
XThreadPool *XThreadPoolFactory::Create()
{
    static mutex mux;
    static bool is_init = false;
    mux.lock();
    if (!is_init)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#else
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
            return NULL;
#endif
        is_init = true;
    }
    mux.unlock();
    return new CXThreadPool();
}
