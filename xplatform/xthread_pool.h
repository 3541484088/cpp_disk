#ifndef XTHREAD_POOL_H
#define XTHREAD_POOL_H

#ifdef _WIN32
#if defined(XCOM_STATIC)
#define XCOM_API
#elif defined(XCOM_EXPORTS)
#define XCOM_API __declspec(dllexport)
#else
#define XCOM_API __declspec(dllimport)
#endif
#else
#define XCOM_API
#endif

#include <vector>

class XThread;
class XTask;

/**
 * @class XThreadPool
 * @brief 线程池抽象基类
 * 
 * 定义了线程池的基本接口，包括初始化、任务分发、线程退出等功能。
 */
class XCOM_API XThreadPool
{
public:
    /**
     * @brief 初始化线程池
     * @param thread_count 线程池中的线程数量
     * 
     * 纯虚函数，由子类实现。创建指定数量的工作线程，
     * 每个线程初始化自己的event_base，线程间通过管道通信。
     */
    virtual void Init(int thread_count) = 0;

    /**
     * @brief 分发任务到线程池
     * @param task 要分发的任务对象指针
     * 
     * 纯虚函数，由子类实现。将任务分配给线程池中的某个线程执行。
     * 任务对象需要继承自XTask接口，用户需要实现Init()方法。
     */
    virtual void Dispatch(XTask *task) = 0;
    
    /**
     * @brief 退出所有线程
     * 
     * 静态方法，设置全局退出标志并通知所有线程退出。
     */
    static void ExitAllThread();

    /**
     * @brief 等待所有线程退出
     * 
     * 静态方法，阻塞当前线程直到所有线程退出完成。
     */
    static void Wait();
};

/**
 * @class XThreadPoolFactory
 * @brief 线程池工厂类
 * 
 * 提供创建线程池实例的静态工厂方法。
 */
class XCOM_API XThreadPoolFactory
{
public:
    /**
     * @brief 创建线程池实例
     * @return 返回新创建的XThreadPool实例指针
     * 
     * 该方法会自动初始化平台相关的网络环境（如Windows的WSA）。
     */
    static XThreadPool *Create();
};

#endif
