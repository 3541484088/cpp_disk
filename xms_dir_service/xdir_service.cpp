/**
 * @file xdir_service.cpp
 * @brief 目录服务实现
 * 
 * 实现目录服务的基础功能
 */
#include "xdir_service.h"
#include "xdir_handle.h"

/**
 * @brief 创建服务处理对象
 * @return XServiceHandle指针
 */
XServiceHandle* XDirService::CreateServiceHandle()
{
    return new XDirHandle();
}

/**
 * @brief XDirService构造函数
 */
XDirService::XDirService()
{
}

/**
 * @brief XDirService析构函数
 */
XDirService::~XDirService()
{
}
