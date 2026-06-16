#pragma once
#include "xservice.h"

class XShareServer : public XService
{
public:
    void main(int argc, char *argv[]);
    virtual XServiceHandle* CreateServiceHandle() override;
};
