/**
 * @file xconfig_client.cpp
 * @brief 配置中心客户端实现
 * 
 * 实现配置中心客户端的配置获取、缓存管理、动态Proto加载等功能
 */
#include "xconfig_client.h"
#include "xtools.h"
#include "xlog_client.h"
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>
#include <map>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>

using namespace std;
using namespace google;
using namespace protobuf;
using namespace compiler;
using namespace xmsg;

// Proto文件根目录
#define PB_ROOT "root/"

// 配置缓存映射（key: ip_port）
static map<string, XConfig> conf_map;
static mutex conf_map_mutex;

// 当前微服务配置（动态消息对象）
static google::protobuf::Message *cur_service_conf = nullptr;
static mutex cur_service_conf_mutex;

// 所有配置列表
static XConfigList *all_config = nullptr;
static mutex all_config_mutex;

/**
 * @brief 获取配置中的整型参数
 * @param key 参数名称
 * @return 参数值（不存在返回0）
 * 
 * 从当前服务配置中获取指定名称的整型参数
 */
int XConfigClient::GetInt(const char *key)
{
    XMutex mux(&cur_service_conf_mutex);
    if (!cur_service_conf)return 0;
    auto field = cur_service_conf->GetDescriptor()->FindFieldByName(key);
    if (!field)
    {
        return 0;
    }
    return cur_service_conf->GetReflection()->GetInt32(*cur_service_conf, field);
}
/**
 * @brief 获取配置中的布尔参数
 * @param key 参数名称
 * @return 参数值（不存在返回false）
 * 
 * 从当前服务配置中获取指定名称的布尔参数
 */
bool XConfigClient::GetBool(const char *key)
{
    XMutex mux(&cur_service_conf_mutex);
    if (!cur_service_conf) return false;
    
    // 获取字段描述符
    auto field = cur_service_conf->GetDescriptor()->FindFieldByName(key);
    if (!field)
    {
        return false;
    }
    return cur_service_conf->GetReflection()->GetBool(*cur_service_conf, field);
}

/**
 * @brief 获取配置中的字符串参数
 * @param key 参数名称
 * @return 参数值（不存在返回空字符串）
 * 
 * 从当前服务配置中获取指定名称的字符串参数
 */
std::string XConfigClient::GetString(const char *key)
{
    XMutex mux(&cur_service_conf_mutex);
    if (!cur_service_conf) return "";
    
    // 获取字段描述符
    auto field = cur_service_conf->GetDescriptor()->FindFieldByName(key);
    if (!field)
    {
        return "";
    }
    return cur_service_conf->GetReflection()->GetString(*cur_service_conf, field);
}

/**
 * @brief 设置当前服务的配置消息对象
 * @param message 配置消息对象
 */
void XConfigClient::SetCurServiceMessage(google::protobuf::Message *message)
{
    XMutex mux(&cur_service_conf_mutex);
    cur_service_conf = message;
}
/**
 * @brief 启动配置获取（自动连接注册中心）
 * @param local_ip 本地服务IP
 * @param local_port 本地服务端口
 * @param conf_message 配置消息对象
 * @param func 定时回调函数
 * @return 启动成功返回true
 * 
 * 注册消息回调，设置本地配置，读取本地缓存，启动连接和定时获取
 */
bool XConfigClient::StartGetConf(const char *local_ip, int local_port,
    google::protobuf::Message *conf_message, ConfigTimerCBFunc func)
{
    // 注册消息回调函数
    RegMsgCallback();

    // 设置本地服务地址
    if (local_ip)
        strncpy(local_ip_, local_ip, 16);
    local_port_ = local_port;

    // 设置当前配置类型
    SetCurServiceMessage(conf_message);

    this->ConfigTimerCB = func;

    // 设置定时器（3秒）
    this->set_timer_ms(3000);

    // 读取本地缓存
    stringstream ss;
    ss << local_port_ << "_conf.cache";
    ifstream ifs;
    ifs.open(ss.str(), ios::binary);
    if (!ifs.is_open())
    {
        LOGDEBUG("load local config failed!");
    }
    else
    {
        if (conf_message)
            cur_service_conf->ParseFromIstream(&ifs);
        ifs.close();
    }

    // 连接配置中心任务加入到线程池
    StartConnect();
    return true;
}

/**
 * @brief 启动配置获取（指定配置中心地址）
 * @param server_ip 配置中心IP
 * @param server_port 配置中心端口
 * @param local_ip 本地服务IP
 * @param local_port 本地服务端口
 * @param conf_message 配置消息对象
 * @param timeout_sec 连接超时时间
 * @return 启动成功返回true
 * 
 * 设置配置中心地址，注册消息回调，启动连接并等待成功
 */
bool XConfigClient::StartGetConf(
    const char *server_ip, int server_port,
    const char *local_ip, int local_port, 
    google::protobuf::Message *conf_message, int timeout_sec)
{
    //注册消息回调函数
    RegMsgCallback();

    //设置配置中心的IP和端口
    set_server_ip(server_ip);
    set_server_port(server_port);

    //_CRT_SECURE_NO_WARNINGS
    if(local_ip)
        strncpy(local_ip_,local_ip,16);
    local_port_ = local_port;

    //设置当前配置类型
    SetCurServiceMessage(conf_message);

    //连接配置中心任务加入到线程池
    StartConnect();


    //等待连接配置中心成功 如果第一次连接失败，不会把定时配置加入线程池，需要调整为定时自动重连，发送配置获取消息？
    if (!WaitConnected(timeout_sec))
    {
        cout << "连接配置中心失败" << endl;
        return false;
    }
    if (local_port_ > 0)
        LoadConfig(local_ip_, local_port_);
    //设定获取配置的定时时间（毫秒）
    SetTimer(3000);

    return true;
}
/**
 * @brief 初始化配置客户端
 * @return 初始化成功返回true
 * 
 * 调用父类初始化，并立即执行一次定时器回调确保消息及时获取
 */
bool XConfigClient::Init()
{
    XServiceClient::Init();
    
    // 立即调用一次定时器，确保消息及时获取
    TimerCB();
    return true;
}

/**
 * @brief 定时器回调函数
 * 
 * 执行用户自定义回调，然后发送配置获取请求
 */
void XConfigClient::TimerCB()
{
    // 执行用户自定义回调
    if (ConfigTimerCB)
        ConfigTimerCB();
    
    // 发送配置获取请求
    if (local_port_ > 0)
        LoadConfig(local_ip_, local_port_);
}

/**
 * @brief 等待线程池退出
 * 
 * 阻塞等待所有线程池任务完成
 */
void XConfigClient::Wait()
{
    XThreadPool::Wait();
}

/**
 * @brief 发送配置获取请求
 * @param ip 服务IP（NULL表示使用客户端连接地址）
 * @param port 服务端口
 * 
 * 向配置中心发送获取指定服务配置的请求
 */
void XConfigClient::LoadConfig(const char *ip, int port)
{
    LOGDEBUG("获取配置请求");
    if (port < 0 || port>65535)
    {
        LOGDEBUG("LoadConfig failed!port error");
        return;
    }
    {
        //清理了上一次的配置 不清理可能照成获取的是旧数据
        //stringstream key;
        //key << ip << "_" << port;
        //XMutex mux(&cur_service_conf_mutex);
        //conf_map.erase(key.str());
    }
    XLoadConfigReq req;
    if(ip) //IP如果为NULL 则取连接配置中心的地址
        req.set_service_ip(ip);
    req.set_service_port(port);
    //发送消息到服务端
    SendMsg(MSG_LOAD_CONFIG_REQ, &req);
}

/**
 * @brief 从缓存获取配置（带超时等待）
 * @param ip 服务IP
 * @param port 服务端口
 * @param out_conf 输出配置对象
 * @param timeout_ms 超时时间（毫秒）
 * @return 获取成功返回true，超时返回false
 * 
 * 在指定超时时间内等待配置缓存更新，找到后复制到输出参数
 */
bool XConfigClient::GetConfig(const char *ip, int port, xmsg::XConfig *out_conf, int timeout_ms)
{
    // 计算循环次数（每10毫秒检查一次）
    int count = timeout_ms / 10;
    stringstream key;
    key << ip << "_" << port;

    for (int i = 0; i < count; i++)
    {
        XMutex mutex(&conf_map_mutex);
        
        // 查找配置
        auto conf = conf_map.find(key.str());
        if (conf == conf_map.end())
        {
            this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        
        // 复制配置
        out_conf->CopyFrom(conf->second);
        return true;
    }
    
    LOGDEBUG("Can't find conf");
    return false;
}

/**
 * @brief 处理配置响应消息
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析配置响应，更新内存缓存，并保存到本地文件
 */
void XConfigClient::LoadConfigRes(xmsg::XMsgHead *head, XMsg *msg)
{
    LOGDEBUG("获取配置响应");
    
    XConfig conf;
    if (!conf.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("LoadConfigRes conf.ParseFromArray failed!");
        return;
    }
    
    LOGDEBUG(conf.DebugString().c_str());
    
    // 构建缓存key（ip_port格式）
    stringstream key;
    key << conf.service_ip() << "_" << conf.service_port();
    
    // 更新配置缓存
    conf_map_mutex.lock();
    conf_map[key.str()] = conf;
    conf_map_mutex.unlock();
   
    // 如果没有本地配置，直接返回
    if (local_port_ <= 0 || !cur_service_conf)
        return;
    
    // 判断是否为当前服务的配置
    stringstream local_key;
    string ip = local_ip_;
    if (ip.empty())
    {
        ip = conf.service_ip();
    }
    local_key << ip << "_" << local_port_;
    
    if (key.str() != local_key.str())
    {
        return;
    }
    
    // 更新当前服务配置
    XMutex mux(&cur_service_conf_mutex);
    if (!cur_service_conf->ParseFromString(conf.private_pb()))
    {
        return;
    }
    
    LOGDEBUG(cur_service_conf->DebugString().c_str());
    
    // 保存到本地文件（文件名格式：[port]_conf.cache）
    stringstream ss;
    ss << local_port_ << "_conf.cache";
    ofstream ofs;
    ofs.open(ss.str(), ios::binary);
    if (!ofs.is_open())
    {
        LOGDEBUG("save local config failed!");
        return;
    }
    
    cur_service_conf->SerializePartialToOstream(&ofs);
    ofs.close();
}

/**
 * @brief Proto文件解析错误收集器
 * 
 * 收集并记录Proto文件解析过程中的语法错误
 */
class ConfError : public MultiFileErrorCollector
{
public:
    void AddError(const std::string& filename, int line, int column,
        const std::string& message)
    {
        stringstream ss;
        ss << filename << "|" << line << "|" << column << "|" << message;
        LOGDEBUG(ss.str().c_str());
        errors_.push_back(ss.str());
    }
    
    std::vector<std::string> errors_;
};

static ConfError conf_error;

/**
 * @brief 动态加载Proto文件并创建消息对象（线程不安全）
 * @param filename Proto文件路径
 * @param class_name 消息类型名称（为空则使用第一个类型）
 * @param out_proto_code 输出Proto代码字符串
 * @return 创建的消息对象指针
 * 
 * 动态加载指定的Proto文件，解析其中的消息类型，并创建对应的消息对象
 */
Message *XConfigClient::LoadProto(std::string filename, std::string class_name, std::string &out_proto_code)
{
    // 需要空间清理 
    delete importer_;
    importer_ = new Importer(source_tree_, &conf_error);
    if (!importer_)
    {
        LOGINFO("Failed to create Importer!");
        return NULL;
    }
    LOGINFO("Importer created successfully");
    
    // 检查文件是否存在
    std::ifstream test_file(filename);
    if (!test_file.good())
    {
        LOGINFO(string("File does not exist or cannot be read: " + filename).c_str());
        return NULL;
    }
    LOGINFO(string("File exists and is readable: " + filename).c_str());
    test_file.close();
    
    //1 加载proto文件
    // 如果是绝对路径直接使用，否则使用root/前缀
    string path;
    if (filename.find(':') != string::npos || filename.find("\\\\") != string::npos)
    {
        // Windows绝对路径 (如 C:\xxx\test.proto 或 \\server\share)
        LOGINFO(string("Absolute path detected: " + filename).c_str());
        
        // 将proto文件所在目录添加到搜索路径，以便解析import
        size_t last_slash = filename.find_last_of("\\/");
        if (last_slash != string::npos)
        {
            string dir = filename.substr(0, last_slash);
            std::replace(dir.begin(), dir.end(), '\\', '/');
            source_tree_->MapPath("", dir);
            
            // 只用文件名导入，MapPath("", dir)会把""映射到proto目录
            path = filename.substr(last_slash + 1);
            LOGINFO(string("Mapped proto directory: " + dir + ", importing: " + path).c_str());
        }
        else
        {
            path = filename;
        }
    }
    else
    {
        // 相对路径，使用root/前缀
        path = PB_ROOT;
        path += filename;
        LOGINFO(string("Relative path: " + path).c_str());
    }
    //返回proto文件描述符
    LOGINFO(string("Attempting to import proto file: " + path).c_str());
    std::cout << "[XConfigClient] Attempting to import proto file: " << path << std::endl;
    auto file_desc = importer_->Import(path);
    std::cout << "[XConfigClient] Import returned: " << (file_desc ? "success" : "NULL") << std::endl;
    if (!file_desc)
    {
        stringstream ss;
        ss << "Failed to import proto file: " << path;
        LOGINFO(ss.str().c_str());
        std::cout << "[XConfigClient] ERROR: Failed to import proto file: " << path << std::endl;
        // 输出所有收集到的错误
        for (auto& err : conf_error.errors_)
        {
            LOGINFO(string("Error: " + err).c_str());
            std::cout << "[XConfigClient] Error: " << err << std::endl;
        }
        return NULL;
    }
    LOGDEBUG(file_desc->DebugString());
    stringstream ss;
    ss << filename << "proto 文件加载成功";
    LOGDEBUG(ss.str().c_str());
    
    //获取类型描述符
    //如果class_name为空，则使用第一个类型
    const Descriptor *message_desc = 0;
    if (class_name.empty())
    {
        if (file_desc->message_type_count() <= 0)
        {
            LOGDEBUG("proto文件中没有message");
            return NULL;
        }
        //取第一个类型
        message_desc = file_desc->message_type(0);
    }
    else
    {
        //包含命名空间的类名 xmsg.XDirConfig
        string class_name_pack = "";
        //查找类型 命名空间，是否要用户提供

        //用户没有提供命名空间 
        if (class_name.find('.') == class_name.npos)
        {
            if (file_desc->package().empty())
            {
                class_name_pack = class_name;
            }
            else
            {
                class_name_pack = file_desc->package();
                class_name_pack += ".";
                class_name_pack += class_name;
            }
        }
        else
        {
            class_name_pack = class_name;
        }
        message_desc = importer_->pool()->FindMessageTypeByName(class_name_pack);

        if (!message_desc)
        {
            string log = "proto文件中没有指定的message ";
            log += class_name_pack;
            LOGDEBUG(log.c_str());
            return NULL;
        }
    }
   
    LOGDEBUG(message_desc->DebugString());

    //反射生成message对象

    //动态创建消息类型的工厂，不能销毁，销毁后由此创建的message也失效
    static DynamicMessageFactory factory;
    
    //创建一个类型原型
    auto message_proto = factory.GetPrototype(message_desc);
    delete message_;
    message_ = message_proto->New();
    LOGDEBUG(message_->DebugString());
    /*
    syntax="proto3";	//版本号
    package xmsg;		//命名空间
    message XDirConfig
    {
        string root = 1;
    }
    */
    
    //auto enum_type = message_desc->enum_type(0);
    //cout << enum_type->DebugString();
    //syntax="proto3";	//版本号
    out_proto_code = "syntax=\"";
    out_proto_code += file_desc->SyntaxName(file_desc->syntax());
    out_proto_code += "\";\n";
    //package xmsg;		//命名空间
    out_proto_code += "package ";
    out_proto_code += file_desc->package();
    out_proto_code += ";\n";

    map<string, const EnumDescriptor*> enum_descs;
    //添加依赖的枚举
    for (int i = 0; i < message_desc->field_count(); i++)
    {
        auto field = message_desc->field(i);
        if (field->type() == FieldDescriptor::TYPE_ENUM)
        {
            if (enum_descs.find(field->enum_type()->name()) != enum_descs.end())
                continue;
            enum_descs[field->enum_type()->name()] = field->enum_type();
            out_proto_code += field->enum_type()->DebugString();
            out_proto_code += "\n";
        }

    }

    //message XDirConfig
    out_proto_code += message_desc->DebugString();
    return message_;
}

/**
 * @brief XConfigClient构造函数
 * 
 * 初始化Proto文件导入器所需的源文件树，配置文件加载路径
 */
XConfigClient::XConfigClient()
{
    source_tree_ = new DiskSourceTree();
    source_tree_->MapPath("", "");
    source_tree_->MapPath(PB_ROOT, "");
}

/**
 * @brief XConfigClient析构函数
 */
XConfigClient::~XConfigClient()
{
}
