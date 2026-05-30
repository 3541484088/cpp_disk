#pragma once
#include "xservice_handle.h"
#include "xms_disk_client_gui.pb.h"
#include <list>
#include <fstream>
#include "xtools.h"
//每个请求只处理一个文件
class XUploadHandle:public XServiceHandle
{
public:
    XUploadHandle() 
    {
        //设定定时器用于获取传输进度
        set_timer_ms(100);
    };
    virtual ~XUploadHandle()
    {
        if (aes_)
            delete aes_;
        aes_ = 0;
    }

    void UploadFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void SendSliceReq(xmsg::XMsgHead *head, XMsg *msg);
    void UploadFileEndReq(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::UPLOAD_FILE_REQ, (MsgCBFunc)&XUploadHandle::UploadFileReq);
        RegCB((xmsg::MsgType)xdisk::SEND_SLICE_REQ, (MsgCBFunc)&XUploadHandle::SendSliceReq);
        
        RegCB((xmsg::MsgType)xdisk::UPLOAD_FILE_END_REQ, (MsgCBFunc)&XUploadHandle::UploadFileEndReq);
    }
private:
    std::string save_dir_ = "";
    xdisk::XFileInfo cur_file_;         //当前接收的文件
    xdisk::XFileSlice cur_slice_;       //当前接收的文件片信息
    std::list<XMsg> cur_data_;          //当前接收的文件片数据
    std::list<XMsg> caches_;            //缓冲文件的缓存
    std::ofstream ofs_;                 //写本地文件
    int filesize_ = 0;                  //文件大小
    int sendsize_ = 0;                  //已经发送的文件大小
    XAES *aes_ = 0; //加密文件
};
