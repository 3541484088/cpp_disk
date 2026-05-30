#pragma once
#include "xservice_handle.h"

//////////////////////////
///处理注册中心的客户端， 响应一个请求
class XRegisterHandle:public XServiceHandle
{
public:
    XRegisterHandle();
    ~XRegisterHandle();

    //接收服务器注册请求
    void RegisterReq(xmsg::XMsgHead *head, XMsg *msg);


    //接收服务器的服务请求
    void GetServiceReq(xmsg::XMsgHead *head, XMsg *msg);
    void HeartRes(xmsg::XMsgHead *head, XMsg *msg) {};
    static void RegMsgCallback()
    {
        RegCB(xmsg::MSG_HEART_REQ, (MsgCBFunc)&XRegisterHandle::HeartRes);
        RegCB(xmsg::MSG_REGISTER_REQ, (MsgCBFunc)&XRegisterHandle::RegisterReq);
        RegCB(xmsg::MSG_GET_SERVICE_REQ, (MsgCBFunc)&XRegisterHandle::GetServiceReq);
    }

   
};
