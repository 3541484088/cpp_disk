/**
 * @file xlog_handle.cpp
 * @brief 日志服务消息处理实现
 * 
 * 处理日志添加请求，将日志写入数据库
 */
#include "xlog_handle.h"
#include "xlog_dao.h"
#include <iostream>

using namespace std;
using namespace xmsg;

/**
 * @brief 处理添加日志请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析日志请求消息，补充客户端IP（如果未指定），然后保存到数据库
 */
void XLogHandle::AddLogReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XAddLogReq req;
    
    // 解析日志请求消息
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        cout << "AddLogReq failed!" << endl;
        return;
    }
    
    // 输出日志接收指示
    cout << "L" << flush;   
    
    // 如果未指定服务IP，使用客户端连接IP
    if (req.service_ip().empty())
    {
        req.set_service_ip(client_ip());
    }
    
    // 保存日志到数据库
    XLogDAO::Get()->AddLog(&req);
}

/**
 * @brief XLogHandle析构函数
 */
XLogHandle::~XLogHandle()
{
}
