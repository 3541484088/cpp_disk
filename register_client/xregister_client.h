#ifndef XREGISTER_CLIENT_H
#define XREGISTER_CLIENT_H
#include "xservice_client.h"

////////////////////////////////////////////////
//// 注册中心的客户端， 在windows上可以直接调用这个类
class XRegisterClient:public XServiceClient
{
public:
    static XRegisterClient *Get()
    {
        static XRegisterClient *xc = 0;
        if (!xc)
        {
            xc = new XRegisterClient();
        }
        return xc;
    }

    ~XRegisterClient();

    //添加成功连接信息回调（业务处理层）
    virtual void ConnectedCB();


    //接收服务器注册响应
    void RegisterRes(xmsg::XMsgHead *head, XMsg *msg);

    //获取服务列表的响应
    void GetServiceRes(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB(xmsg::MSG_REGISTER_RES, (MsgCBFunc)&XRegisterClient::RegisterRes);
        RegCB(xmsg::MSG_GET_SERVICE_RES, (MsgCBFunc)&XRegisterClient::GetServiceRes);
    }

    ///////////////////////////////////////////////////////////////////////
    //// 向注册中心注册本服务器的信息，这个函数调用只需要调用一次即可，调用层
    /// @para service_name 微服务名称
    /// @para port 微服务端口
    /// @para ip 微服务IP，如果传入NULL，则使用每个客户自己的地址
    /// @para is_find 是否被发现为服务提供端
    void RegisterServer(const char *service_name, int port, const char *ip, bool is_find = false);

    /// 获取所有的服务器列表，返回原始数据，每个数据是上次的服务提供者
    /// 此函数返回不是XServiceMap数据的功能函数，是一个线程
    xmsg::XServiceMap *GetAllService();


    /////////////////////////////////////////////////////////////////////////////
    /// 获取指定服务名称的微服务列表， 内部实现原理
    /// 1 等待连接成功 2 发送获取微服务请求信息 3 等待微服务列表信息，返回有可能调用的下一次函数
    /// @para service_name 服务名称
    /// @para timeout_sec 超时时间
    /// @return 服务列表
    xmsg::XServiceList GetServcies(const char *service_name, int timeout_sec);

    /////////////////////////////////////////////////////////////
    ///发送请求获取微服务列表信息
    ///@para service_name == NULL 获取全部
    void GetServiceReq(const char *service_name);

    //定时任务用于服务器注册
    virtual void TimerCB();
private:
    XRegisterClient();

    //获取缓存文件， 线程安全，操作service_map缓存
    bool LoadLocalFile();

    char service_name_[32] = {0};
    int service_port_ = 0;
    char service_ip_[16] = {0};

    //是否被发现为服务提供者
    bool is_find_ = false;


};
#endif
