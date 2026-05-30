#ifndef XROUTER_HANDLE_H
#define XROUTER_HANDLE_H
#include "xservice_handle.h"
class XRouterHandle : public XServiceHandle
{
public:
    XRouterHandle();
    ~XRouterHandle();
    
    virtual void ReadCB(xmsg::XMsgHead *head, XMsg *msg);

    //添加断开任务时，清理任务
    virtual void Close();

    static void InitMsgServer();

    static void CloseMsgServer();
    
    //发送 转发proxy回调
    bool  SendMsg(xmsg::XMsgHead *head, XMsg *msg);

};

#endif
