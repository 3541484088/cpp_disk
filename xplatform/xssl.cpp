/**
 * @file xssl.cpp
 * @brief SSL/TLS 连接封装实现
 * 
 * 封装 OpenSSL 库的 SSL 连接操作，提供加密通信能力
 */
#include "xssl.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
using namespace std;

/**
 * @brief 关闭 SSL 连接并释放资源
 */
void XSSL::Close()
{
    if (ssl_)
    {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = 0;
    }
}

/**
 * @brief 通过 SSL 连接发送数据
 * @param data 要发送的数据指针
 * @param data_size 数据长度
 * @return 成功发送的字节数，失败返回 0
 */
int XSSL::Write(const void *data, int data_size)
{
    if (!ssl_) return 0;
    return SSL_write(ssl_, data, data_size);
}

/**
 * @brief 通过 SSL 连接接收数据
 * @param buf 接收缓冲区指针
 * @param buf_size 缓冲区大小
 * @return 成功接收的字节数，失败返回 0
 */
int XSSL::Read(void *buf, int buf_size)
{
    if (!ssl_) return 0;
    return SSL_read(ssl_, buf, buf_size);
}

/**
 * @brief 打印对方证书信息
 */
void XSSL::PrintCert()
{
    // 空实现：OpenSSL 3.0 兼容性问题
}

/**
 * @brief 打印当前使用的加密算法
 */
void XSSL::PrintCipher()
{
    if (!ssl_) return;
    cout << "Cipher: " << SSL_get_cipher(ssl_) << endl;
}

/**
 * @brief 客户端建立 SSL 连接
 * @return 连接成功返回 true，失败返回 false
 * 
 * 注意：socket 必须已经完成 TCP connect
 */
bool XSSL::Connect()
{
    if (!ssl_)
        return false;
    
    int re = SSL_connect(ssl_);
    if (re <= 0)
    {
        cout << "XSSL::Connect() failed!" << endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    cout << "SSL_connect success!" << endl;
    PrintCipher();
    PrintCert();
    return true;
}

/**
 * @brief 服务端接受 SSL 连接
 * @return 接受成功返回 true，失败返回 false
 * 
 * 完成 SSL 握手，包括证书验证和密钥协商
 */
bool XSSL::Accept()
{
    if (!ssl_)
        return false;
    
    // 执行 SSL 握手，包括证书验证和密钥协商
    int re = SSL_accept(ssl_);
    if (re <= 0)
    {
        cout << "XSSL::Accept() failed!" << endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    cout << "SSL_accept success!" << endl;
    PrintCipher();
    return true;
}

/**
 * @brief XSSL 构造函数
 */
XSSL::XSSL()
{
}

/**
 * @brief XSSL 析构函数
 */
XSSL::~XSSL()
{
}
