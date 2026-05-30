#pragma once

#include "xservice_client.h"

typedef void(*ConfigResCBFunc) (bool is_ok, const char *msg);
typedef void(*GetConfigResCBFunc) (xmsg::XConfig);


#define MCONF XConfigManager::Get()
class XConfigManager :public XServiceClient
{
public:
    static XConfigManager *Get()
    {
        static XConfigManager cc;
        return &cc;
    }
    XConfigManager();
    ~XConfigManager();

    
    
    void DeleteConfig(const char *ip, int port);

    
    void DeleteConfigRes(xmsg::XMsgHead *head, XMsg *msg);

    xmsg::XConfigList GetAllConfig(int page, int page_count, int timeout_sec);

    
    void LoadAllConfigRes(xmsg::XMsgHead *head, XMsg *msg);

    
    void SendConfig(xmsg::XConfig *conf);

    
    void SendConfigRes(xmsg::XMsgHead *head, XMsg *msg);


    void LoadConfig(const char *ip, int port);

    
    void LoadConfigRes(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB(xmsg::MSG_SAVE_CONFIG_RES, (MsgCBFunc)&XConfigManager::SendConfigRes);
        RegCB(xmsg::MSG_LOAD_CONFIG_RES, (MsgCBFunc)&XConfigManager::LoadConfigRes);
        RegCB(xmsg::MSG_LOAD_ALL_CONFIG_RES, (MsgCBFunc)&XConfigManager::LoadAllConfigRes);
        RegCB(xmsg::MSG_DEL_CONFIG_RES, (MsgCBFunc)&XConfigManager::DeleteConfigRes);
    }


    
    void set_login(xmsg::XLoginRes login);

    
    ConfigResCBFunc SendConfigResCB = 0;

    GetConfigResCBFunc LoadConfigResCB = 0;




private:
    xmsg::XLoginRes login_;
    
    std::mutex login_mutex_;

    virtual bool  SendMsg(xmsg::MsgType type, const google::protobuf::Message *message);

   // xmsg::XConfig config_;
    //std::mutex config_mutex_;
};

