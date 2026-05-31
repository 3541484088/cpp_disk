/**
 * @file xssl_ctx.cpp
 * @brief SSL/TLS 上下文管理器实现
 * 
 * 管理 SSL 上下文，支持客户端和服务端模式的初始化
 */
#include "xssl_ctx.h"
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
using namespace std;

/**
 * @brief 证书验证回调函数
 * @param preverify_ok 预验证结果
 * @param x509_ctx 证书存储上下文
 * @return 返回验证结果（0 表示失败，非 0 表示成功）
 * 
 * 可以在此做进一步验证，比如验证证书中的域名是否正确
 */
static int SSLVerifyCB(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    if (preverify_ok == 0)
    {
        cout << "SSL cert validate failed!" << endl;
    }
    else
    {
        cout << "SSL cert validate success!" << endl;
    }
    // 可以做进一步验证，比如验证证书中的域名是否正确
    return preverify_ok;
}

/**
 * @brief 释放 SSL 上下文资源
 */
void XSSLCtx::Close()
{
    if (ssl_ctx_)
    {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = 0;
    }
}

/**
 * @brief 设置证书验证
 * @param ca_crt CA 证书文件路径
 * 
 * 配置 SSL 上下文以验证对方证书
 */
void XSSLCtx::SetVerify(const char *ca_crt)
{
    if (!ca_crt || !ssl_ctx_ || strlen(ca_crt) == 0) return;
    
    // 设置验证模式为验证对方证书，并设置回调函数
    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, SSLVerifyCB);
    SSL_CTX_load_verify_locations(ssl_ctx_, ca_crt, 0);
}

/**
 * @brief 初始化 SSL 客户端上下文
 * @param ca_file CA 证书文件路径（用于验证服务端证书）
 * @return 初始化成功返回 true，失败返回 false
 */
bool XSSLCtx::InitClient(const char *ca_file)
{
    ssl_ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx_)
    {
        cerr << "SSL_CTX_new TLS_client_method failed!" << endl;
        return false;
    }

    // 配置对服务器证书的验证
    SetVerify(ca_file);
    return true;
}

/**
 * @brief 初始化 SSL 服务端上下文
 * @param crt_file 服务端证书文件路径
 * @param key_file 服务端私钥文件路径
 * @param ca_file CA 证书文件路径（用于验证客户端证书，可选）
 * @return 初始化成功返回 true，失败返回 false
 */
bool XSSLCtx::InitServer(const char *crt_file, const char *key_file, const char *ca_file)
{
    // 创建服务器端 SSL 上下文
    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_)
    {
        cerr << "SSL_CTX_new TLS_server_method failed!" << endl;
        return false;
    }

    // 加载证书文件
    int re = SSL_CTX_use_certificate_file(ssl_ctx_, crt_file, SSL_FILETYPE_PEM);
    if (re <= 0)
    {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cout << "Load certificate success!" << endl;

    // 加载私钥文件
    re = SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file, SSL_FILETYPE_PEM);
    if (re <= 0)
    {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cout << "Load PrivateKey success!" << endl;

    // 验证私钥与证书是否匹配
    re = SSL_CTX_check_private_key(ssl_ctx_);
    if (re <= 0)
    {
        cout << "private key does not match the certificate!" << endl;
        return false;
    }
    cout << "check_private_key success!" << endl;

    // 配置对客户端证书的验证（可选）
    SetVerify(ca_file);
    return true;
}

/**
 * @brief 创建新的 SSL 连接对象
 * @param socket 已连接的 socket 描述符（0 表示由 bufferevent 管理）
 * @return 创建的 XSSL 对象
 * 
 * 如果 socket > 0，会将 SSL 对象绑定到该 socket；否则由 bufferevent 自行创建
 */
XSSL XSSLCtx::NewXSSL(int socket)
{
    XSSL xssl;
    if (!ssl_ctx_)
    {
        cout << "ssl_ctx == 0" << endl;
        return xssl;
    }
        
    auto ssl = SSL_new(ssl_ctx_);
    if (!ssl)
    {
        cerr << "SSL_new failed!" << endl;
        return xssl;
    }
    
    // bufferevent 会自行创建 SSL 对象时不需要绑定 socket
    if (socket > 0)
        SSL_set_fd(ssl, socket);
    
    xssl.set_ssl(ssl);
    return xssl;
}

/**
 * @brief XSSLCtx 构造函数
 */
XSSLCtx::XSSLCtx()
{
}

/**
 * @brief XSSLCtx 析构函数
 */
XSSLCtx::~XSSLCtx()
{
}
