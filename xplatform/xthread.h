#ifndef XTHREAD_H
#define XTHREAD_H

#include <list>
#include <mutex>
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

class XTask;

/**
 * @class XThread
 * @brief 线程抽象基类
 * 
 * 定义了线程的基本接口，包括启动、唤醒、任务管理等功能。
 * 使用静态工厂方法Create()创建线程实例。
 */
class XCOM_API XThread
{
public:
    /**
     * @brief 创建XThread实例的静态工厂方法
     * @return 返回新创建的XThread实例指针
     */
    static XThread* Create();
    
    /**
     * @brief 启动线程
     * 
     * 纯虚函数，由子类实现具体的线程启动逻辑。
     */
    virtual void Start() = 0;

    /**
     * @brief 安装线程，初始化event_base和管道唤醒事件
     * @return true表示初始化成功，false表示失败
     * 
     * 纯虚函数，由子类实现线程环境的初始化工作。
     */
    virtual bool Setup() = 0;

    /**
     * @brief 处理线程唤醒通知
     * @param fd 唤醒事件的文件描述符
     * @param which 事件类型
     * 
     * 纯虚函数，由子类实现唤醒通知的处理逻辑。
     */
    virtual void Notify(int fd, short which) = 0;

    /**
     * @brief 唤醒线程
     * 
     * 纯虚函数，由子类实现线程唤醒机制。
     */
    virtual void Activate() = 0;

    /**
     * @brief 添加任务到任务队列
     * @param t 要添加的任务对象指针
     * 
     * 纯虚函数，由子类实现任务添加逻辑。
     * 每个线程可以同时处理多个任务，共享同一个event_base。
     */
    virtual void AddTask(XTask *t) = 0;
	
    /**
     * @brief 析构函数
     */
	~XThread();

    /**
     * @brief 线程ID
     * 
     * 用于标识不同的线程实例。
     */
	int id = 0;

    /**
     * @brief 退出线程
     * 
     * 设置退出标志，线程主循环检测到该标志后会退出。
     */
    void Exit()
    {
        is_exit_ = true;
    }

protected:
    /**
     * @brief 构造函数
     * 
     * 保护构造函数，禁止直接实例化，必须通过Create()方法创建。
     */
    XThread();
    
    /**
     * @brief 线程退出标志
     * 
     * 当设置为true时，线程主循环会退出。
     */
    bool is_exit_ = false;
};

#endif
