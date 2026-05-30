#pragma once

#include "xservice.h"
class XLogServer:public XService
{
public:
    ///参数不多 初始化服务，需要先调用
    void main(int argc, char *argv[]);


    XServiceHandle* CreateServiceHandle();
    XLogServer();
    ~XLogServer();
};