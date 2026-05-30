/**
 * @file xrouter_server.cpp
 * @brief 路由服务实现
 * 
 * 实现API网关的路由服务，支持SSL安全连接
 */
#include "xrouter_server.h"
#include "xrouter_handle.h"
#include "xconfig_client.h"
#include "xtools.h"
#include "xlog_client.h"
#include <string>
using namespace std;

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 * 
 * 创建XRouterHandle实例，并根据配置初始化SSL上下文
 */
XServiceHandle* XRouterServer::CreateServiceHandle()
{
    // 注意：如果需要SSL连接，需要包含applink.c
    // OPENSSL_Uplink(57CB3350,08): no OPENSSL_Applink
    // 解决方法：包含 C:\xdisk_lesson\include\openssl\applink.c
    
    // 从配置获取SSL证书路径
    string crt_path = XConfigClient::Get()->GetString("crt_path");
    string key_path = XConfigClient::Get()->GetString("key_path");
    string ca_path = XConfigClient::Get()->GetString("ca_path");
    cout << "crt_path = " << crt_path << endl;
    
    // 检查是否启用SSL
    bool is_ssl = XConfigClient::Get()->GetBool("is_ssl");
    if (is_ssl)
    {
        if (ssl_ctx_)
        {
            // 如果已经创建ssl，则不创建（如果需要修改证书地址，需要重启gateway）
        }
        else
        {
            LOGDEBUG("开始使用SSL通道");
            ssl_ctx_ = new XSSLCtx();
            ssl_ctx_->InitServer(crt_path.c_str(), key_path.c_str(), ca_path.c_str());
            set_ssl_ctx(ssl_ctx_);
        }
    }
    return new XRouterHandle();
}
