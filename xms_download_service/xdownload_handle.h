#pragma once
#include "xservice_handle.h"
#include "xms_disk_client_gui.pb.h"
#include <list>
#include <fstream>
//每个请求只处理一个文件
class XDownloadHandle:public XServiceHandle
{
public:
    XDownloadHandle();
    ~XDownloadHandle();

    void DownloadFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void DownloadFileBegin(xmsg::XMsgHead *head, XMsg *msg);
    void DownloadSliceRes(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_FILE_REQ, (MsgCBFunc)&XDownloadHandle::DownloadFileReq);
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_FILE_BEGTIN, (MsgCBFunc)&XDownloadHandle::DownloadFileBegin);
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_SLICE_RES, (MsgCBFunc)&XDownloadHandle::DownloadSliceRes);
    }
private:
    void SendSlice();
    xdisk::XFileInfo file_;         //当前发送的文件
    xdisk::XFileSlice cur_slice_;       //当前发送的文件片信息
    std::list<XMsg> cur_data_;          //当前发送的文件片数据
    std::list<XMsg> caches_;            //缓冲文件的缓存
    std::ifstream ifs_;                 //读本地文件
    int filesize_ = 0;                  //文件大小
    int sendsize_ = 0;                  //已经发送的文件大小
    char *slice_buf_ = 0;
};
