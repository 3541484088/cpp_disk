/**
 * @file LXData.cpp
 * @brief 数据封装类实现
 * 
 * 提供数据存储、文件读写和编码转换功能
 */
#include "LXData.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h> 
#else
#include <iconv.h>
#endif

using namespace std;
namespace LX
{
#ifndef _WIN32
/**
 * @brief 字符编码转换（Linux平台）
 * @param from_cha 源编码
 * @param to_cha 目标编码
 * @param in 输入数据
 * @param inlen 输入长度
 * @param out 输出缓冲区
 * @param outlen 输出缓冲区大小
 * @return 转换结果
 */
static size_t Convert(char *from_cha, char *to_cha, char *in, size_t inlen, char *out, size_t outlen)
{
    // 转换上下文
    iconv_t cd;
    cd = iconv_open(to_cha, from_cha);
    if (cd == 0)
        return -1;
    memset(out, 0, outlen);
    char **pin = &in;
    char **pout = &out;
    // 转换字节数，转换GBK时可能不正确 >=0就成功
    size_t re = iconv(cd, pin, &inlen, pout, &outlen);
    iconv_close(cd);
    return re;
}
#endif

    /**
     * @brief 整型数据构造函数
     * @param d 整型数据指针
     */
    LXData::LXData(const int *d)
    {
        this->type = LX_TYPE_LONG;
        this->data = (const char*)d;
        this->size = sizeof(int);
    }

    /**
     * @brief 字符串数据构造函数
     * @param data 字符串数据
     */
    LXData::LXData(const char* data)
    {
        this->type = LX_TYPE_STRING;
        if (!data) return;
        this->data = data;
        this->size = strlen(data);
    }

    /**
     * @brief 从文件加载数据
     * @param filename 文件名
     * @return 加载成功返回true
     * 
     * 读取文件内容并写入到data和size中
     */
    bool LXData::LoadFile(const char* filename)
    {
        if (!filename) return false;
        fstream in(filename, ios::in | ios::binary);
        if (!in.is_open())
        {
            cerr << "LoadFile " << filename << " failed!" << endl;
            return false;
        }
        // 获取文件大小
        in.seekg(0, ios::end);
        size = in.tellg();
        in.seekg(0, ios::beg);
        if (size <= 0)
        {
            return false;
        }
        data = new char[size];
        int readed = 0;
        while (!in.eof())
        {
            in.read((char*)data + readed, size - readed);
            if (in.gcount() > 0)
                readed += in.gcount();
            else
                break;
        }
        in.close();
        this->type = LX_TYPE_BLOB;
        return true;
    }

    /**
     * @brief 保存数据到文件
     * @param filename 文件名
     * @return 保存成功返回true
     */
    bool LXData::SaveFile(const char * filename)
    {
        if (!data || size <= 0)
            return false;
        fstream out(filename, ios::out|ios::binary);
        if (!out.is_open())
        {
            cout << "SaveFile failed!open failed!"<< filename << endl;
            return false;
        }
        out.write(data, size);
        out.close();
        return true;
    }

    /**
     * @brief 释放LoadFile分配的data空间
     */
    void LXData::Drop()
    {
        delete data;
        data = NULL;
    }

    /**
     * @brief UTF-8编码转换为GBK编码
     * @return 转换后的GBK字符串
     */
    std::string LXData::UTF8ToGBK()
    {
        string re = "";
        // 1. UTF8转为unicode (Windows下为UTF16)
#ifdef _WIN32
        // 1.1 统计转换后字节数
        int len = MultiByteToWideChar(CP_UTF8,    // 转换的格式
            0,          // 默认的转换方式
            data,       // 输入的字节
            -1,         // 输入的字符串大小 -1 到\0
            0,          // 输出
            0           // 输出的空间大小
        );
        if (len <= 0)
            return re;
        wstring udata;
        udata.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, data, -1, (wchar_t*)udata.data(), len);

        // 2. unicode转GBK
        len = WideCharToMultiByte(CP_ACP, 0, (wchar_t*)udata.data(), -1, 0, 0,
            0,  // 失败默认的字符
            0   // 是否使用默认的字符 
        );
        if (len <= 0)
            return re;
        re.resize(len);
        WideCharToMultiByte(CP_ACP, 0, (wchar_t*)udata.data(), -1, (char*)re.data(), len, 0, 0);
#else
        re.resize(1024);
        int inlen = strlen(data);
        Convert((char*)"utf-8", (char*)"gbk", (char*)data, inlen, (char*)re.data(), re.size());
        int outlen = strlen(re.data());
        re.resize(outlen);
#endif
        return re;
    }

    /**
     * @brief GBK编码转换为UTF-8编码
     * @return 转换后的UTF-8字符串
     */
    std::string LXData::GBKToUTF8()
    {
        string re = "";
#ifdef _WIN32    
        // GBK转unicode
        // 1.1 统计转换后字节数
        int len = MultiByteToWideChar(CP_ACP,    // 转换的格式
            0,          // 默认的转换方式
            data,       // 输入的字节
            -1,         // 输入的字符串大小 -1 到\0
            0,          // 输出
            0           // 输出的空间大小
        );
        if (len <= 0)
            return re;
        wstring udata;
        udata.resize(len);
        MultiByteToWideChar(CP_ACP, 0, data, -1, (wchar_t*)udata.data(), len);

        // 2. unicode转utf-8
        len = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)udata.data(), -1, 0, 0,
            0,  // 失败默认的字符
            0   // 是否使用默认的字符 
        );
        if (len <= 0)
            return re;
        re.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)udata.data(), -1, (char*)re.data(), len, 0, 0);
#else
        re.resize(1024);
        int inlen = strlen(data);
        Convert((char*)"gbk", (char*)"utf-8", (char*)data, inlen, (char*)re.data(), re.size());
        int outlen = strlen(re.data());
        re.resize(outlen);
#endif
        return re;
    }
}
