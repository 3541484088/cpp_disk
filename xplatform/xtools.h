#pragma once
#ifdef _WIN32
#if defined(XCOM_STATIC)
#define XCOM_API
#elif defined(XCOM_EXPORTS)
#define XCOM_API __declspec(dllexport)
#else
#define XCOM_API __declspec(dllimport)
#endif
#else
#define XCOM_API
#endif
#include <string>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>
#include <list>
#include "xlog_client.h"
using namespace std;

XCOM_API std::string GetDirData(std::string path);


struct XToolFileInfo
{
    std::string filename = "";
    long long filesize = 0;
    bool is_dir = false;
    long long time_write = 0;
    std::string time_str = "";
};

//获取目录列表
XCOM_API std::list< XToolFileInfo > GetDirList(std::string path);

//删除文件
XCOM_API void XDelFile(std::string path);

//创建目录
XCOM_API void XNewDir(std::string path);


XCOM_API void XStringSplit(std::vector<std::string> &vec, std::string str, std::string find);


XCOM_API void XStrReplace(std::string str, std::string str_find, std::string str_replace, std::string &sreturn);


XCOM_API std::string XFormatDir(const std::string &dir);

XCOM_API std::string XTrim(const std::string& s);

XCOM_API bool XFileExist(const std::string& s);

//计算md5 128bit(16字节)
XCOM_API unsigned char *XMD5(const unsigned char *in_data, unsigned long in_data_size, unsigned char *out_md);

//计算md5_base64 (24字节)
XCOM_API std::string XMD5_base64(const unsigned char *in_data, unsigned long in_data_size);

/////////////////////////////////////////////////////////////
// XMutex互斥锁封装类
/////////////////////////////////////////////////////////////
class XCOM_API XMutex
{
public:
    //禁用拷贝构造和赋值运算符，确保锁的独占性
    XMutex(const XMutex&) = delete;
    XMutex& operator=(const XMutex&) = delete;

    //构造函数（无消息版本），自动加锁
    XMutex(std::mutex *mux);

    //构造函数（带消息版本），自动加锁，用于调试输出
    XMutex(std::mutex *mux, std::string msg);

    //析构函数，自动解锁
    ~XMutex();

    //调试模式开关
    static bool is_debug;

private:
    std::mutex *mux_ = nullptr;   //底层互斥锁指针
    std::string msg_;              //调试消息
    int index_ = 0;               //锁的序号
};

/////////////////////////////////////////////////////////////
// XAES加密类
/////////////////////////////////////////////////////////////
class XCOM_API XAES
{
public:
    //创建XAES实例
    static XAES* Create();

    //设置密钥
    //@key 密钥数据
    //@key_size 密钥长度（字节）
    //@is_enc true表示加密，false表示解密
    virtual bool SetKey(const char *key, int key_size, bool is_enc) = 0;

    //释放对象
    virtual void Drop() = 0;

    //加密
    //@in 输入数据
    //@in_size 输入数据大小
    //@out 输出缓冲区
    //@return 加密后数据长度
    virtual long long Encrypt(const unsigned char *in, long long in_size, unsigned char *out) = 0;

    //解密
    //@in 输入数据
    //@in_size 输入数据大小
    //@out 输出缓冲区
    //@return 解密后数据长度
    virtual long long Decrypt(const unsigned char *in, long long in_size, unsigned char *out) = 0;

protected:
    XAES() = default;
public:
    virtual ~XAES() = default;
};

//Base64编码
XCOM_API int Base64Encode(const unsigned char *in, int len, char *out_base64);

//Base64解码
XCOM_API int Base64Decode(const char *in, int len, unsigned char *out_data);

//获取本机IP地址
XCOM_API std::string GetLocalHostIP(const std::string &host_name);

//GBK转UTF8
XCOM_API std::string XGBKToUTF8(const std::string& gbk_str);

//获取毫秒级时间戳
XCOM_API long long GetTimestampMs();

//获取秒级时间戳
XCOM_API long long GetTimestamp();

//时间戳转字符串
XCOM_API std::string TimestampToString(long long timestamp);

//获取当前日期时间字符串
XCOM_API std::string GetDateTimeString();

//获取时间字符串
XCOM_API std::string XGetTime(int timestamp, std::string fmt);

//获取目录大小
XCOM_API long long GetDirSize(const char *path);

//获取磁盘空间
XCOM_API bool GetDiskSize(const char *dir, unsigned long long *avail, unsigned long long *total, unsigned long long *free);

//根据主机名获取IP
XCOM_API std::string XGetHostByName(std::string host_name);

//获取文件大小字符串（如1KB, 2MB等）
XCOM_API std::string XGetSizeString(long long size);

//获取文件图标文件名
XCOM_API std::string XGetIconFilename(std::string filename, bool is_dir);