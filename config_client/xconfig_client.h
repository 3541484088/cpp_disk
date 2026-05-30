#pragma once
#include "xservice_client.h"

//前端代码
namespace google
{
    namespace protobuf 
    {
        namespace compiler
        {
            class Importer;
            class DiskSourceTree;
        }
    }
}

typedef void(*ConfigTimerCBFunc) ();
class XConfigClient :public XServiceClient
{
public:
    ~XConfigClient();
    static XConfigClient *Get()
    {
        static XConfigClient cc;
        return &cc;
    }

    ConfigTimerCBFunc ConfigTimerCB = 0;

   
    bool StartGetConf(const char *local_ip, int local_port,
        google::protobuf::Message *conf_message, ConfigTimerCBFunc func);

    bool StartGetConf(const char *server_ip, int server_port,
        const char *local_ip, int local_port,
        google::protobuf::Message *conf_message, int timeout_sec = 10);


    int GetInt(const char *key);
    bool GetBool(const char *key);
    std::string GetString(const char *key);


    virtual void TimerCB();

    
    virtual bool Init();

    
    void Wait();
   
    void LoadConfig(const char *ip, int port);

    bool GetConfig(const char *ip, int port, xmsg::XConfig *out_conf,int timeout_ms = 100);

    void LoadConfigRes(xmsg::XMsgHead *head, XMsg *msg);

    
    void SetCurServiceMessage(google::protobuf::Message *message);

    
    google::protobuf::Message *LoadProto(std::string filename, std::string class_name,std::string &out_proto_code);


    static void RegMsgCallback()
    {
        //RegCB(xmsg::MSG_SAVE_CONFIG_RES, (MsgCBFunc)&XConfigClient::SendConfigRes);
        RegCB(xmsg::MSG_LOAD_CONFIG_RES, (MsgCBFunc)&XConfigClient::LoadConfigRes);
        //RegCB(xmsg::MSG_LOAD_ALL_CONFIG_RES, (MsgCBFunc)&XConfigClient::LoadAllConfigRes);
        //RegCB(xmsg::MSG_DEL_CONFIG_RES, (MsgCBFunc)&XConfigClient::DeleteConfigRes);
    }



private:
    xmsg::XLoginRes login_;
    std::mutex login_mutex_;
    XConfigClient();

    char local_ip_[16] = { 0 };
    int local_port_ = 0;

    
    google::protobuf::compiler::Importer *importer_ = 0;

    
    google::protobuf::compiler::DiskSourceTree *source_tree_ = 0;

    
    google::protobuf::Message *message_ = 0;

    
};

