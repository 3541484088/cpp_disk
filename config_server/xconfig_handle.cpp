/**
 * @file xconfig_handle.cpp
 * @brief 配置中心消息处理实现
 * 
 * 处理配置管理相关的消息：保存配置、加载配置、删除配置、加载全部配置
 */
#include "xconfig_handle.h"
#include "xtools.h"
#include "config_dao.h"

using namespace xmsg;
using namespace std;

/**
 * @brief 处理保存配置请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析配置信息，保存到数据库，并返回处理结果
 */
void XConfigHandle::SaveConfig(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("接收到保存配置的消息");
    
    XMessageRes res;
    XConfig conf;
    XMsgHead h;
    h.set_msg_id(head->msg_id());

    // 解析配置消息
    if (!conf.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("XConfigHandle::SaveConfig failed! format error!");
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("format error");
        h.set_msg_type(MSG_SAVE_CONFIG_RES);
        SendMsg(&h, &res);
        return;
    }

    // 如果没有指定IP，使用客户端IP
    if (conf.service_ip().empty())
    {
        string ip = client_ip();
        conf.set_service_ip(ip);
    }

    // 保存配置到数据库
    if (ConfigDao::Get()->SaveConfig(&conf))
    {
        res.set_return_(XMessageRes::OK);
        res.set_msg("OK");
        h.set_msg_type(MSG_SAVE_CONFIG_RES);
        SendMsg(&h, &res);
        return;
    }

    res.set_return_(XMessageRes::ERROR);
    res.set_msg("insert db failed!");
    h.set_msg_type(MSG_SAVE_CONFIG_RES);
    SendMsg(&h, &res);
}

/**
 * @brief 处理加载配置请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 根据IP和端口从数据库加载配置信息并返回
 */
void XConfigHandle::LoadConfig(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("接收到下载配置的消息");
    
    XLoadConfigReq req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("LoadConfig ParseFromArray failed!");
        return;
    }

    // 如果没有指定IP，使用客户端IP
    string ip = req.service_ip();
    if (ip.empty())
    {
        ip = client_ip();
    }

    // 根据IP和端口获取配置
    XConfig conf = ConfigDao::Get()->LoadConfig(ip.c_str(), req.service_port());
    
    XMsgHead h;
    h.set_msg_id(head->msg_id());
    h.set_msg_type(MSG_LOAD_CONFIG_RES);
    SendMsg(&h, &conf);
}

/**
 * @brief 处理删除配置请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 根据IP和端口删除配置信息并返回处理结果
 */
void XConfigHandle::DeleteConfig(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("删除配置");
    
    XLoadConfigReq req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("DeleteConfig ParseFromArray failed!");
        return;
    }

    XMessageRes res;

    // 根据IP和端口删除配置
    if (ConfigDao::Get()->DeleteConfig(req.service_ip().c_str(), req.service_port()))
    {
        res.set_return_(XMessageRes::OK);
        res.set_msg("OK");
        XMsgHead h;
        h.set_msg_id(head->msg_id());
        h.set_msg_type(MSG_DEL_CONFIG_RES);
        SendMsg(&h, &res);
        return;
    }

    res.set_return_(XMessageRes::ERROR);
    res.set_msg("ConfigDao::Get()->DeleteConfig failed!");
    XMsgHead h;
    h.set_msg_id(head->msg_id());
    h.set_msg_type(MSG_DEL_CONFIG_RES);
    SendMsg(&h, &res);
}

/**
 * @brief 处理加载全部配置请求（支持分页）
 * @param head 消息头
 * @param msg 消息体
 * 
 * 分页加载数据库中的所有配置信息并返回
 */
void XConfigHandle::LoadAllConfig(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("下载全部配置（有分页）");
    
    XLoadAllConfigReq req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("LoadAllConfig ParseFromArray failed!");
        return;
    }

    // 分页加载所有配置
    auto confs = ConfigDao::Get()->LoadAllConfig(req.page(), req.page_count());

    XMsgHead h;
    h.set_msg_id(head->msg_id());
    h.set_msg_type(MSG_LOAD_ALL_CONFIG_RES);
    SendMsg(&h, &confs);
}

/**
 * @brief XConfigHandle构造函数
 */
XConfigHandle::XConfigHandle()
{
}

/**
 * @brief XConfigHandle析构函数
 */
XConfigHandle::~XConfigHandle()
{
}
