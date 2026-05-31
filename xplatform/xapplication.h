#ifndef XAPPLICATION_H
#define XAPPLICATION_H
#include <string>

class XApplication
{
public:    
    static std::string Application;         //应用名称
    static std::string ServerName;          //服务器名，一般是服务名和一个唯一服务标识
    static std::string BasePath;            //应用程序根路径，用于保存自动系统的配置目录
    static std::string DataPath;            //应用程序数据路径，用于保存通用的配置文件
    static std::string LocalIp;             //本地IP
    static std::string LogPath;             //log路径
    static std::string LogLevel;            //log日志级别
    static std::string Local;               //本地监听地址
    static std::string Node;                //本地node地址
    static std::string Log;                 //日志服务的地址
    static std::string Config;              //配置服务的地址
    static std::string Notify;              //消息通知服务
    static std::string ConfigFile;          //配置配置文件路径
    /**
     * 应用构造
     */
    XApplication();

    /**
     * 应用析构
     */
    ~XApplication();

    /**
     * 初始化
     * @param argv
     */
    void main(int argc, char *argv[]);
    
    /**
     * 等待
     */
    void WaitForShutdown();

protected:
    /**
    * 初始化, 只在进程的第一个
    */
    virtual void Initialize() = 0;

    /**
    * 销毁, 进程只执行一次
    */
    virtual void DestroyApp() = 0;

};

#endif