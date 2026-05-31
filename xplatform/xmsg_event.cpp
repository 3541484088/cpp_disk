/**
 * @file xmsg_event.cpp
 * @brief 消息事件处理实现文件
 * 
 * 实现消息的接收、解析、分发和发送功能
 */
#include "xmsg_event.h"
#include "xmsg_com.pb.h"
#include "xtools.h"
#include "xlog_client.h"
#include <iostream>
#include <sstream>
#include <map>

using namespace std;
using namespace xmsg;
using namespace google;
using namespace protobuf;

// 同一消息类型只注册一个回调函数
static map<MsgType, XMsgEvent::MsgCBFunc> msg_callback;

/**
 * @brief 注册消息回调函数
 * @param type 消息类型
 * @param func 回调函数指针
 * 
 * 为指定消息类型注册处理回调，同一类型只能注册一次
 */
void XMsgEvent::RegCB(xmsg::MsgType type, XMsgEvent::MsgCBFunc func)
{
    if (msg_callback.find(type) != msg_callback.end())
    {
        stringstream ss;
        ss << "RegCB is error, " << type << " have been set." << endl;
        LOGERROR(ss.str().c_str());
        return;
    }
    msg_callback[type] = func;
}

/**
 * @brief 消息回调分发函数
 * @param head 消息头
 * @param msg 消息体
 * 
 * 根据消息类型查找并执行对应的回调函数
 */
void XMsgEvent::ReadCB(xmsg::XMsgHead *head, XMsg *msg)
{
    // 根据消息类型查找回调函数
    auto ptr = msg_callback.find(head->msg_type());
    if (ptr == msg_callback.end())
    {
        Clear();
        stringstream ss;
        ss << "msg error func not set! msg_type=" << head->msg_type();
        LOGDEBUG(ss.str().c_str());
        return;
    }
    
    auto func = ptr->second;
    (this->*func)(pb_head_, msg);
}
/**
 * @brief 读取消息循环入口函数
 * 
 * 循环接收消息，解析并分发到对应的回调函数处理
 */
void XMsgEvent::ReadCB()
{
    static int i = 0;
    i++;
    
    // 消息接收循环
    while (1)
    {
        // 接收消息数据
        if (!RecvMsg())
        {
            Clear();
            break;
        }
        
        if (!pb_head_) break;
        auto msg = GetMsg();
        if (!msg) break;

        // 记录接收到的消息日志
        if (pb_head_)
        {
            stringstream ss;
            ss << "[RECV] " << server_ip() << ":" << server_port() 
               << "|" << XGetPortName(server_port()) 
               << " " << client_ip() << ":" << client_port() 
               << " " << pb_head_->DebugString();
            
            // 避免日志请求消息导致循环记录
            if (pb_head_->msg_type() != xmsg::MSG_ADD_LOG_REQ)
                LOGDEBUG(ss.str().c_str());
        }

        // 分发消息到回调函数处理
        ReadCB(pb_head_, msg);

        // 清理消息数据
        Clear();
        
        // 检查连接是否需要断开
        if (is_drop_)
        {
            set_auto_delete(true);
            Close();
            return;
        }
    }
}
/**
 * @brief 接收消息数据函数
 * 
 * 分阶段接收消息：
 * 1. 接收消息头大小（4字节）
 * 2. 接收消息头数据并解析
 * 3. 根据消息头中的消息体大小接收消息体数据
 * 
 * @return 成功返回true，连接断开或出错返回false
 */
bool XMsgEvent::RecvMsg()
{
    // 步骤1：接收消息头大小
    if (!head_.size)
    {
        int len = Read(&head_.size, sizeof(head_.size));
        if (len <= 0 || head_.size <= 0)
        {
            return false;
        }

        // 为消息头分配内存
        if (!head_.Alloc(head_.size))
        {
            stringstream ss;
            ss << "head_.Alloc failed! size=" << head_.size;
            LOGDEBUG(ss.str().c_str());
            return false;
        }
    }

    // 步骤2：接收消息头数据
    if (!head_.Recved())
    {
        int len = Read(
            head_.data + head_.recv_size,  // 从上次接收位置继续
            head_.size - head_.recv_size
        );
        if (len <= 0)
        {
            return true;  // 还没接收完，继续等待
        }
        head_.recv_size += len;

        if (!head_.Recved())
            return true;

        // 解析protobuf消息头
        if (!pb_head_)
        {
            pb_head_ = new XMsgHead();
        }
        if (!pb_head_->ParseFromArray(head_.data, head_.size))
        {
            stringstream ss;
            ss << "pb_head.ParseFromArray failed! size=" << head_.size;
            LOGDEBUG(ss.str().c_str());
            return false;
        }
        
        // 如果消息体为空
        if (pb_head_->msg_size() == 0)
        {
            msg_.type = pb_head_->msg_type();
            msg_.size = 0;
            return true;
        }
        else
        {
            // 为消息体分配内存
            if (!msg_.Alloc(pb_head_->msg_size()))
            {
                stringstream ss;
                ss << "msg_.Alloc failed! msg_size=" << pb_head_->msg_size();
                LOGDEBUG(ss.str().c_str());
                return false;
            }
        }

        msg_.type = pb_head_->msg_type();
    }

    // 步骤3：接收消息体数据
    if (!msg_.Recved())
    {
        int len = Read(
            msg_.data + msg_.recv_size,  // 从上次接收位置继续
            msg_.size - msg_.recv_size
        );
        if (len <= 0)
        {
            return true;  // 还没接收完，继续等待
        }
        msg_.recv_size += len;
    }

    if (msg_.Recved())
    {
        cout << "+" << flush;
    }

    return true;
}

/**
 * @brief 获取接收到的消息体
 * @return 消息体指针，如果未接收完成返回NULL
 */
XMsg *XMsgEvent::GetMsg()
{
    if (msg_.Recved())
        return &msg_;
    return NULL;
}

/**
 * @brief 关闭连接
 * 
 * 先清理消息数据，然后关闭底层连接
 */
void XMsgEvent::Close()
{
    Clear();
    XComTask::Close();
}

/**
 * @brief 清理消息数据
 * 
 * 重置消息头和消息体，准备接收下一条消息
 */
void XMsgEvent::Clear()
{
    head_.Clear();
    msg_.Clear();
}

/**
 * @brief 发送消息
 * @param head 消息头
 * @param msg 消息体
 * @return 发送成功返回true，失败返回false
 * 
 * 消息格式：[消息头大小(4字节)][消息头数据][消息体数据]
 */
bool XMsgEvent::SendMsg(xmsg::XMsgHead *head, XMsg *msg)
{
    if (!head)  // 支持只发送消息头
        return false;
    
    head->set_msg_size(msg->size);
    
    // 序列化消息头
    string head_str = head->SerializeAsString();
    int headsize = head_str.size();
    
    // 记录发送日志（避免日志请求循环）
    if (head)
    {
        stringstream ss;
        ss << "[SEND] " << server_ip() << ":" << server_port() 
           << " " << XGetPortName(server_port()) 
           << " " << head->DebugString();
        
        if (head->msg_type() != xmsg::MSG_ADD_LOG_REQ)
            LOGDEBUG(ss.str().c_str());
    }

    // 步骤1：发送消息头大小（4字节）
    int re = Write(&headsize, sizeof(headsize));
    if (!re) return false;

    // 步骤2：发送消息头数据
    re = Write(head_str.data(), head_str.size());
    if (!re) return false;

    // 步骤3：发送消息体数据（支持空消息体）
    if (msg->size > 0)
    {
        re = Write(msg->data, msg->size);
        if (!re) return false;
    }
    return true;
}

/**
 * @brief 发送消息（protobuf消息体版本）
 * @param head 消息头
 * @param message protobuf消息对象
 * @return 发送成功返回true，失败返回false
 * 
 * 将protobuf消息序列化为字节流后发送
 */
bool XMsgEvent::SendMsg(xmsg::XMsgHead *head, const Message *message)
{
    if (!message || !head)
        return false;
    
    // 序列化消息体
    string msg_str = message->SerializeAsString();
    int msg_size = msg_str.size();
    
    XMsg msg;
    msg.data = (char*)msg_str.data();
    msg.size = msg_size;
    
    return SendMsg(head, &msg);
}

/**
 * @brief 发送消息（简化版本）
 * @param type 消息类型
 * @param message protobuf消息对象
 * @return 发送成功返回true，失败返回false
 * 
 * 自动创建消息头并发送
 */
bool XMsgEvent::SendMsg(MsgType type, const Message *message)
{
    if (!message)
        return false;
    
    XMsgHead head;
    head.set_msg_type(type);
    return SendMsg(&head, message);
}
