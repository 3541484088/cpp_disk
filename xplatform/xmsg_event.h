#pragma once
#include "xmsg_type.pb.h"
#include "xmsg.h"
#include "xcom_task.h"
#include "xmsg_com.pb.h"


//基于libbufferevent接口，直接等到XComTask的封装
class XCOM_API XMsgEvent:public XComTask
{
public:
   
    virtual void DropInMsg() { is_drop_ = true; };

    ////读取消息套接字消息
    virtual void ReadCB();

    //消息回调处理函数，默认发送给用户注册的函数路径处理
    //返回false 则退出处理消息循环
    virtual void ReadCB(xmsg::XMsgHead *head, XMsg *msg);

    //////////////////////////////////////////
    /// 接收数据包
    /// 1 确认接收到的消息  (检查消息完整性）
    /// 2 消息接收不完全   (等待下一次接收) 
    /// 3 消息接收超过    （退出接收占空间）
    /// @return 1 2 返回true 3返回false
    bool RecvMsg();

    /////////////////////////////////////////
    /// 获取已经接收的数据包，过滤掉头部信息
    /// 可得到处理类形XMsg
    /// @return 如果没有接收完整数据包则返回NULL
    XMsg *GetMsg();

    //////////////////////////////////
    /// 发送消息，包含头部和负载，自动计算
    /// @type 消息类型
    /// @message 消息内容
    /// @return 如果使用占，必须保证bev未断开
    virtual bool  SendMsg(xmsg::MsgType type, const google::protobuf::Message *message);
    virtual bool  SendMsg(xmsg::XMsgHead *head, const google::protobuf::Message *message);
    virtual bool  SendMsg(xmsg::XMsgHead *head, XMsg *msg);
    //virtual bool  SendLog(const google::protobuf::Message *message);

    /////////////////////////////////////
    /// 清空接收信息，头部信息数据，用于接收下一条消息
    void Clear();

    void Close();

    typedef void (XMsgEvent::*MsgCBFunc) (xmsg::XMsgHead *head, XMsg *msg);
    ////////////////////////////////////////////////////
    /// 注册消息处理类的回调函数，同一个消息类型只能注册一个回调函数
    /// @para type 消息类型
    /// @para func 消息回调函数
    static void RegCB(xmsg::MsgType type, MsgCBFunc func);


private:
    bool is_drop_ = false;
    XMsg head_; //消息头
    XMsg msg_;  //消息内容

    //pb消息头
    xmsg::XMsgHead *pb_head_ = 0;
};
