#ifndef XSERVICES_CLIENT_H
#define XSERVICES_CLIENT_H
#include "xmsg_event.h"
#include "xthread_pool.h"
#include <thread>

/*
使用示例：

class XMyServiceClient :public XServiceClient
{
public:
    //发送消息给服务端
    void SendMsgToServer()
    {
        //创建了一个protobuf消息
        XDirReq req;
        req.set_path("test");
        SendMsg(MSG_DIR_REQ, &req);
    }
    //接收服务器返回消息
    void SendConfigRes(xmsg::XMsgHead *head, XMsg *msg)
    {
        //创建了解析protobuf消息
        XDirRes res;
        if (!res.ParseFromArray(msg->data, msg->size))
        {
            cout<<"res.ParseFromArray failed!"<<endl;
            return;
        }
    }
    //注册接收服务器返回消息处理
    static void RegMsgCallback()
    {
        RegCB(xmsg::MSG_SAVE_CONFIG_RES, (MsgCBFunc)&XMyServiceClient::SendConfigRes);
    }
};
int main()
{
    //注册接收服务器返回消息处理
    XMyServiceClient::RegMsgCallback();

    //设定服务器IP和端口
    XMyServiceClient client;
    client.set_server_ip("127.0.0.1");
    client.set_server_port(20010);

    //创建连接，连接线程池
    client.StartConnect();

    //发送消息给服务端
    client.SendMsgToServer();

    XThreadPool::Wait();
    return 0;
}

**/

class XCOM_API XServiceClient :public XMsgEvent
{
public:
    XServiceClient();
    virtual ~XServiceClient();
    
    //////////////////////////////////////////////////
    /// 将连接放入到线程池中，创建连接
    virtual void StartConnect();


    //发送消息
    virtual bool SendMsg(xmsg::XMsgHead *head, const google::protobuf::Message *message) override;


    //发送消息
    virtual bool SendMsg(xmsg::MsgType type, const google::protobuf::Message *message) override;

    //发送文件流
    virtual bool SendMsg(xmsg::XMsgHead *head, XMsg *msg) override;

    virtual void set_service_name(std::string name) { service_name_ = name; }
    virtual void set_login(xmsg::XLoginRes *login);

    xmsg::XLoginRes *login() { return login_; }

protected:
    //获取服务器端服务器名称和登录信息到head  底层调用 dll和动态库内存在堆上分配
    // xmsg::XMsgHead GetHeadByType(xmsg::MsgType type);



private:

    //设置head的登录信息
    xmsg::XMsgHead *SetHead(xmsg::XMsgHead *head);

    XThreadPool *thread_pool_ = 0;
    
    //微服务名称
    std::string service_name_;
    xmsg::XLoginRes *login_ = 0;
    std::mutex login_mutex_;
};

#endif