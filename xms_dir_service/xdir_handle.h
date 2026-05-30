#pragma once

#include "xservice_handle.h"
#include "xms_disk_client_gui.pb.h"
class XDirHandle :public XServiceHandle
{
public:
    XDirHandle();
    ~XDirHandle();

    void GetDirReq(xmsg::XMsgHead *head, XMsg *msg);
    void DeleteFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void NewDirReq(xmsg::XMsgHead *head, XMsg *msg);
    void GetDiskInfoReq(xmsg::XMsgHead *head, XMsg *msg);

    //void ChangeDirReq(xmsg::XMsgHead *head, XMsg *msg);
    
    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::DELETE_FILE_REQ, (MsgCBFunc)&XDirHandle::DeleteFileReq);
        RegCB((xmsg::MsgType)xdisk::GET_DIR_REQ, (MsgCBFunc)&XDirHandle::GetDirReq);
        RegCB((xmsg::MsgType)xdisk::NEW_DIR_REQ, (MsgCBFunc)&XDirHandle::NewDirReq);
        RegCB((xmsg::MsgType)xdisk::GET_DISK_INFO_REQ, (MsgCBFunc)&XDirHandle::GetDiskInfoReq);

        //RegCB((xmsg::MsgType)xdisk::CHANGE_DIR_REQ, (MsgCBFunc)&XDirHandle::ChangeDirReq);
    }
};

