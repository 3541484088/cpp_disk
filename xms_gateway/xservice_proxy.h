#ifndef XSERVICEPROXY_H
#define XSERVICEPROXY_H
#include <map>
#include <vector>
#include <string>
#include "xservice_proxy_client.h"
class XServiceProxy
{
public:
    static XServiceProxy *Get()
    {
        static XServiceProxy xs;
        return &xs;
    }
    XServiceProxy();
    ~XServiceProxy();
    ///初始化微服务列表，注册中心的获取服务器列表
    bool Init();

    //返回负载均衡好的客户端连接，发送数据返回
    bool SendMsg(xmsg::XMsgHead *head, XMsg *msg,XMsgEvent *ev);

    //删除消息回调
    void DelEvent(XMsgEvent *ev);


    //启动线程
    void Start();

    //停止线程
    void Stop();

    void Main();

private:

    bool is_exit_ = false;
    //缓存的微服务列表和客户端连接
    std::map < std::string, std::vector<XServiceProxyClient *>> client_map_;

    std::mutex client_map_mutex_;
    //记录上次查询的下标
    std::map<std::string, int>client_map_last_index_;


    //记录所有的callback任务
    std::map<XMsgEvent*, XServiceProxyClient*> callbacks_;
    std::mutex callbacks_mutex_;


};

#endif
