#pragma once
#include "xservice.h"
class XDownloadServer:public XService
{
public:
    XDownloadServer();
    ~XDownloadServer();
    void main(int argc, char *argv[]);
    virtual XServiceHandle* CreateServiceHandle() override;
};

