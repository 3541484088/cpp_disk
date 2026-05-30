#pragma once
#include "xfile_manager.h"
class XMSFileManager :public XFileManager
{
public:
    XMSFileManager();
    ~XMSFileManager();

    //获取目录
    virtual void GetDir(std::string root) override;

    virtual void InitFileManager(std::string server_ip, int server_port) override;

    virtual void UploadFile(xdisk::XFileInfo task) override;

    virtual void DownloadFile(xdisk::XFileInfo file) override;
     
    virtual void DeleteFile(xdisk::XFileInfo file) override;

    virtual void NewDir(std::string path) override;

    virtual void set_login(xmsg::XLoginRes login) override;



};

