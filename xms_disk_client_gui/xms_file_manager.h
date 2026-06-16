#pragma once
#include "xfile_manager.h"
class XMSFileManager :public XFileManager
{
public:
    XMSFileManager();
    ~XMSFileManager();

    virtual void GetDir(std::string root) override;
    virtual void InitFileManager(std::string server_ip, int server_port) override;
    virtual void UploadFile(xdisk::XFileInfo task) override;
    virtual void DownloadFile(xdisk::XFileInfo file) override;
    virtual void DeleteFile(xdisk::XFileInfo file) override;
    virtual void NewDir(std::string path) override;
    virtual void set_login(xmsg::XLoginRes login) override;

    // 共享文件夹接口
    virtual void CreateShareFolder(const std::string &name,
                                   const std::vector<xdisk::XShareUser> &users) override;
    virtual void GetSharedFolders() override;
    virtual void GetSharedDir(int64_t folder_id, const std::string &path) override;
    virtual void UploadToSharedFolder(int64_t folder_id,
                                      const std::string &sub_dir) override;
    virtual void AddSharedUser(int64_t folder_id,
                               const std::vector<xdisk::XShareUser> &users) override;
    virtual void RemoveSharedUser(int64_t folder_id,
                                  const std::vector<std::string> &usernames) override;
    virtual void DownloadFromSharedFolder(int64_t folder_id,
                                          const std::string &filename,
                                          const std::string &filedir,
                                          const std::string &local_path) override;
    virtual void DeleteFromSharedFolder(int64_t folder_id,
                                        const std::string &filename,
                                        const std::string &filedir) override;
};

