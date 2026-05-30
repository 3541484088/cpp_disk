#ifndef XSERVICEPROXYCLIENT
#define XSERVICEPROXYCLIENT
#include "xservice_client.h"
#include <map>


class XServiceProxyClient :public XServiceClient
{
public:

    static XServiceProxyClient* Create(std::string service_name);

    ~XServiceProxyClient();
    virtual void ReadCB(xmsg::XMsgHead *head, XMsg *msg);

    //发送数据，添加标识
    virtual bool SendMsg(xmsg::XMsgHead *head, XMsg *msg, XMsgEvent *ev);

    //注册一个事件
    void RegEvent(XMsgEvent *ev);
    void DelEvent(XMsgEvent *ev);

    //是否被发现为服务提供者
    bool is_find() { return is_find_; }
    void set_is_find(bool is) { this->is_find_ = is; }
protected:

    XServiceProxyClient();
    bool is_find_ = false;
         
    //消息转换的等待一个proxy应答对应的是XMsgEvent
    //的值指针为返回值需要注意64位
    std::map<long long, XMsgEvent *> callback_task_;
    std::mutex callback_task_mutex_;
};

#endif
