#ifndef XROUTERSERVER_H
#define XROUTERSERVER_H
#include "xservice.h"
class XSSLCtx;

class XRouterServer :public XService
{
public:
    XServiceHandle* CreateServiceHandle();
private:
    //是否支持ssl
    //bool is_ssl_ = false;
    XSSLCtx *ssl_ctx_ = 0; //没有证书就不创建此对象
};

#endif