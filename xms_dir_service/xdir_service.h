#pragma once
#include "xservice.h"

class XDirService:public XService
{
public:
    XDirService();
    ~XDirService();
    XServiceHandle* CreateServiceHandle();
};

