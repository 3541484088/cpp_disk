#pragma once
#include <QObject>
#include <list>
#include <mutex>
#include <map>
#include "xms_disk_client_gui.pb.h"
//struct FileInfo
//{
//    //std::string iconpath;
//    std::string filename;
//    std::string filepath;
//    long long filesize = 0;     //byte
//    //std::string filesize_str;   // 100KB
//    std::string filetime;       //2020-01-20 18:30
//    bool is_dir = false;        //是否是目录
//};

class FileManager:public QObject
{
    Q_OBJECT

public:
    static FileManager *Get()
    {
        static FileManager fm;

        return &fm;
    }

    ////启动线程，发送数据
    //void StartThread();

    virtual ~FileManager() { is_exit_ = true; }

    ///创建一个获取目录的服务线程，返回线程列表
    void GetDir(std::string root);

    void RefreshDir() { GetDir(root_); }

    ///开始上传文件
    ///@ file_local_path 本地文件（要上传的文件）的全路径
    void UploadFile(std::string filename, std::string file_local_path,std::string remote_dir);

    void AddTask(xdisk::XFileMsgType, const google::protobuf::Message *msg_in);

    bool GetTask(xdisk::XFileMsgType, google::protobuf::Message *msg_out);


    //等待：0~1000
    void UploadProcess(int task_id, long long process);

signals:
    //刷新目录显示
    void RefreshData(xdisk::XFileInfoList file_list);


protected:
    FileManager();
    bool is_exit_ = false;
    std::string root_ = "";
  /*  void Main();*/

    std::mutex mutex_;
    xdisk::XFileMsgMap msg_map_;

    xdisk::XFileInfoList upload_tasks_;
};
