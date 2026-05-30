/**
 * @file xservice_proxy.cpp
 * @brief 服务代理实现
 * 
 * 实现微服务的代理功能，包括服务发现、负载均衡、消息转发等
 */
#include "xservice_proxy.h"
#include "xmsg_com.pb.h"
#include "xtools.h"
#include "xlog_client.h"
#include "xregister_client.h"
#include <thread>
#include <chrono>
using namespace std;
using namespace xmsg;

/**
 * @brief 初始化微服务列表（从注册中心获取），建立连接
 * @return 初始化成功返回true
 */
bool XServiceProxy::Init()
{
    // 从注册中心获取微服务列表
    return true;
}

/**
 * @brief 清理消息回调
 * @param ev 消息事件对象
 */
void XServiceProxy::DelEvent(XMsgEvent *ev)
{
    if (!ev) return;
    XMutex mux(&callbacks_mutex_);
    auto call = callbacks_.find(ev);
    if (call == callbacks_.end())
    {
        LOGDEBUG("callbacks_ not find!");
        return;
    }
    call->second->DelEvent(ev);
}

/**
 * @brief 负载均衡找到客户端连接，进行数据发送
 * @param head 消息头
 * @param msg 消息体
 * @param ev 消息事件对象
 * @return 发送成功返回true
 */
bool XServiceProxy::SendMsg(xmsg::XMsgHead *head, XMsg *msg, XMsgEvent *ev)
{
    if (!head || !msg) return false;
    string service_name = head->service_name();
    
    // MSG_GET_OUT_SERVICE：获取微服务，只能获取is_find=true的微服务
    if (head->msg_type() == MSG_GET_OUT_SERVICE_REQ)
    {
        // 负载均衡找到客户端连接
        XServiceList services;
        services.set_name(service_name);
        auto client_list = client_map_.find(service_name);
        if (client_list == client_map_.end())
        {
            return ev->SendMsg(head, &services);
        }
        // 找到is_find=true的和可以连接的
        for (auto c : client_list->second)
        {
            if (!c->is_find() || !c->is_connected())
                continue;
            auto ser = services.add_service();
            ser->set_ip(c->server_ip());
            ser->set_port(c->server_port());
        }
        head->set_msg_type(MSG_GET_OUT_SERVICE_RES);
        return ev->SendMsg(head, &services);
    }

    XMutex mux(&client_map_mutex_);

    // 负载均衡找到客户端连接
    auto client_list = client_map_.find(service_name);
    if (client_list == client_map_.end())
    {
        stringstream ss;
        ss << service_name << " client_map_ not find!";
        LOGDEBUG(ss.str().c_str());
        return false;
    }

    // 轮询找到可用的微服务连接
    int cur_index = client_map_last_index_[service_name];
    int list_size = client_list->second.size();
    for (int i = 0; i < list_size; i++)
    {
        cur_index++;
        cur_index = cur_index % list_size;
        client_map_last_index_[service_name] = cur_index;
        auto client = client_list->second[cur_index];
        if (client->is_connected())
        {
            // 用于退出清理
            XMutex mux(&callbacks_mutex_);
            callbacks_[ev] = client;

            // 转发消息
            return client->SendMsg(head, msg, ev);
        }
    }
    LOGDEBUG("can't find proxy");
    return false;
}

/**
 * @brief 开启自动重连的线程
 */
void XServiceProxy::Start()
{
    thread th(&XServiceProxy::Main, this);
    th.detach();
}

/**
 * @brief 停止线程
 */
void XServiceProxy::Stop()
{
}

/**
 * @brief 主循环函数
 * 
 * 自动从注册中心获取服务列表并维护连接
 */
void XServiceProxy::Main()
{
    // 自动重连
    while (!is_exit_)
    {
        // 从注册中心获取微服务的列表更新
        // 发送请求到注册中心
        XRegisterClient::Get()->GetServiceReq(0);
        this_thread::sleep_for(chrono::milliseconds(200));
        auto service_map = XRegisterClient::Get()->GetAllService();
        if (!service_map)
        {
            LOGDEBUG("GetAllService : service_map is NULL");
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }
        auto smap = service_map->service_map();
        if (smap.empty())
        {
            LOGDEBUG("XServiceProxy : service_map->service_map is NULL");
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }

        // 遍历所有的微服务名称列表
        for (auto m : smap)
        {
            // 遍历单个微服务
            for (auto s : m.second.service())
            {
                string service_name = m.first;

                // 不连接自己
                if (service_name == API_GATEWAY_NAME)
                {
                    continue;
                }

                // 此微服务是否已经连接
                XMutex mux(&client_map_mutex_);
                // 第一个微服务，创建对象，开启连接
                if (client_map_.find(service_name) == client_map_.end())
                {
                    client_map_[service_name] = std::vector<XServiceProxyClient *>();
                }

                // 列表中是否已有此微服务
                bool isfind = false;
                for (auto c : client_map_[service_name])
                {
                    if (s.ip() == c->server_ip() && s.port() == c->server_port())
                    {
                        isfind = true;
                        break;
                    }
                }
                if (isfind)
                    continue;

                // 根据类型创建不同的proxy
                auto proxy = XServiceProxyClient::Create(service_name);
                proxy->set_is_find(s.is_find());
                proxy->set_server_ip(s.ip().c_str());
                proxy->set_server_port(s.port());
                // 设置关闭后对象自动清理
                proxy->set_auto_delete(false);

                // 连接任务加入线程池
                proxy->StartConnect();
                client_map_[service_name].push_back(proxy);
                client_map_last_index_[service_name] = 0;
            }
        }

        // 定时全部重新获取
        for (auto m : client_map_)
        {
            for (auto c : m.second)
            {
                if (c->is_connected())
                    continue;
                if (!c->is_connecting())
                {
                    LOGDEBUG("start connect service");
                    c->Connect();
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

/**
 * @brief XServiceProxy构造函数
 */
XServiceProxy::XServiceProxy()
{
}

/**
 * @brief XServiceProxy析构函数
 */
XServiceProxy::~XServiceProxy()
{
}
