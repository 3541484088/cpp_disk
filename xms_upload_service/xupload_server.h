#pragma once
#include "xservice.h"
class XUploadServer:public XService
{
public:
    XUploadServer();
    ~XUploadServer();
    void main(int argc, char *argv[]);
    virtual XServiceHandle* CreateServiceHandle() override;
};

