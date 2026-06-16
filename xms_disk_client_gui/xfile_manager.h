#pragma once

#include <QObject>
#include <atomic>
#include "xms_disk_client_gui.pb.h"
#include "xmsg_com.pb.h"
#include "xmsg.h"
class XFileManager:public QObject
{

    Q_OBJECT

public:

    static XFileManager * Instance() { return instance_; }

    //获取目录
    virtual void GetDir(std::string root) = 0;

    ///开始上传文件
    ///@ file_local_path 本地文件（要上传的文件）的全路径
   // void UploadFile(std::string filename, std::string file_local_path, std::string remote_dir);

    void RefreshDir() { GetDir(root_); }

    virtual ~XFileManager() {};

    virtual void InitFileManager(std::string server_ip, int server_port) = 0;

    ///开始上传文件
    ///@ file_local_path 本地文件（要上传的文件）的全路径
    virtual void UploadFile(xdisk::XFileInfo file) = 0;

    //设置文件传输的密钥
    virtual void set_password(std::string pass) { password_ = pass; };
    virtual std::string  password() { return password_ ; };

    //等待：0~1000
    //返回上传列表，线程安全
    virtual void UploadProcess(int task_id, long long sended);
    virtual void UploadEnd(int task_id);



    ///开始下载文件
    ///@ file_local_path 本地文件（要保存的文件）的全路径
    virtual void DownloadFile(xdisk::XFileInfo file) = 0;
    virtual void DownloadProcess(int task_id, long long recved);
    virtual void DownloadEnd(int task_id);

    virtual void DeleteFile(xdisk::XFileInfo file) = 0;

    // Batch-delete coordination: call BeginBatchDelete(n) before issuing n
    // DeleteFile calls, then call DeleteDone() in each DeleteFileRes callback.
    // Only the last DeleteDone() triggers the directory refresh.
    void BeginBatchDelete(int count);
    // Returns true when this is the last pending delete (caller should refresh).
    bool DeleteDone();

    virtual void NewDir(std::string path) = 0;

    // ===== 共享文件夹接口 =====
    virtual void CreateShareFolder(const std::string &name,
                                   const std::vector<xdisk::XShareUser> &users) = 0;
    virtual void GetSharedFolders() = 0;
    virtual void GetSharedDir(int64_t folder_id, const std::string &path) = 0;
    virtual void UploadToSharedFolder(int64_t folder_id,
                                      const std::string &sub_dir) = 0;
    virtual void AddSharedUser(int64_t folder_id,
                               const std::vector<xdisk::XShareUser> &users) = 0;
    virtual void RemoveSharedUser(int64_t folder_id,
                                  const std::vector<std::string> &usernames) = 0;
    virtual void DownloadFromSharedFolder(int64_t folder_id,
                                          const std::string &filename,
                                          const std::string &filedir,
                                          const std::string &local_path) = 0;
    virtual void DeleteFromSharedFolder(int64_t folder_id,
                                        const std::string &filename,
                                        const std::string &filedir) = 0;
    //等待：0~1000
    //返回上传列表，线程安全
    //virtual void DownloadProcess(int task_id, int sended);

    //获取上传服务器列表，线程安全
    //std::list<xdisk::XFileTask> uploads();

    //添加任务ID
    int AddUploadTask(xdisk::XFileInfo file );
    int AddDownloadTask(xdisk::XFileInfo file);
    virtual void set_login(xmsg::XLoginRes login) { this->login_ = login; }
    xmsg::XLoginRes login() { return login_; }
    
    xmsg::XServiceList upload_servers();
    xmsg::XServiceList download_servers();

    void set_upload_servers(xmsg::XServiceList servers);
    void set_download_servers(xmsg::XServiceList servers);

signals:
    //刷新目录显示
    void RefreshData(xdisk::XFileInfoList file_list, std::string cur_dir);
    void RefreshUploadTask(std::list<xdisk::XFileTask> file_list);
    void RefreshDownloadTask(std::list<xdisk::XFileTask> file_list);
    void RefreshCompleteTask(std::list<xdisk::XFileTask> file_list);
    void RefreshDiskInfo(xdisk::XDiskInfo info);
    void ErrorSig(std::string str);
    void InfoSig(std::string str);  // non-error informational notice (e.g. instant upload)
    // 共享文件夹信号
    void SigSharedFolders(std::vector<xdisk::XSharedFolderInfo> folders);
    void SigSharedDir(xdisk::XFileInfoList files);
    void SigSharedUploadAuthorized(std::string authorized_filedir);
    void SigSharedDeleteDone(bool success, std::string msg);
protected:
    XFileManager() {};
    //virtual bool CreateDir() = 0;
    //virtual bool CreateUpload() = 0;
    static XFileManager *instance_;
    std::string root_ = "";

    std::list<xdisk::XFileTask> uploads_;
    std::mutex uploads_mutex_;
    std::list<xdisk::XFileTask> downloads_;
    std::mutex downloads_mutex_;
    std::list<xdisk::XFileTask> completes_;
    std::mutex completes_mutex_;
    xmsg::XLoginRes login_;

    std::atomic<int> pending_deletes_{0};

    //上传服务器列表
    std::mutex servers_mutex_;
    xmsg::XServiceList upload_servers_;

    xmsg::XServiceList download_servers_;
    std::string password_ = "";


};

