#pragma once
#include "xservice_client.h"
#include "xms_disk_client_gui.pb.h"
#include <fstream>
#include "xtools.h"
class XDownloadClient :public XServiceClient
{
public:
    XDownloadClient();
    ~XDownloadClient();
    virtual void ConnectedCB() override;
    virtual void Close() override;
    void DownloadFileRes(xmsg::XMsgHead *head, XMsg *msg);
    void DownloadSliceReq(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_FILE_RES, (MsgCBFunc)&XDownloadClient::DownloadFileRes);
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_SLICE_REQ, (MsgCBFunc)&XDownloadClient::DownloadSliceReq);
    }
    bool set_file(xdisk::XFileInfo file);
    bool CheckAndFixDuplicateExtension();

    void TimerCB() override;

    
    //void set_save_path(std::string path) { this->save_path_ = path; }
private:
    int task_id_ = 0;
    //std::string save_path_ = "";


    xdisk::XFileInfo file_;
    std::ofstream ofs_;

    //char *slice_buf_ = 0;
    //char *slice_buf_enc_ = 0;

    //开始传输数据时已经接收的值，需要确认数据已经接收成功
    long long begin_recv_data_size_ = -1;

    //std::list<std::string> md5_base64s_;
    std::string all_md5_base64_ = "";

    //加密文件类
    XAES *aes_ = 0;

};
