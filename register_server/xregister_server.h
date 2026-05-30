#ifndef XREGISTERSERVER_H
#define XREGISTERSERVER_H
#include "xservice.h"

////////////////////////////////////
//// 注册中心的服务端
class XRegisterServer:public XService
{
public:

    ///参数不多 初始化服务，需要先调用
    void main(int argc, char *argv[]);

    //等待线程退出
    void Wait();

    XServiceHandle* CreateServiceHandle();

};


#endif