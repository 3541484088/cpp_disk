#pragma once
#include "xservice_handle.h"
#include "xms_disk_client_gui.pb.h"
#include "LXMysql.h"
#include <filesystem>

class XShareHandle : public XServiceHandle
{
public:
    XShareHandle();
    ~XShareHandle();

    void CreateShareFolderReq(xmsg::XMsgHead *head, XMsg *msg);
    void AddShareUserReq(xmsg::XMsgHead *head, XMsg *msg);
    void RemoveShareUserReq(xmsg::XMsgHead *head, XMsg *msg);
    void GetSharedFoldersReq(xmsg::XMsgHead *head, XMsg *msg);
    void GetSharedDirReq(xmsg::XMsgHead *head, XMsg *msg);
    void DownloadSharedFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void UploadSharedFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void DeleteSharedFileReq(xmsg::XMsgHead *head, XMsg *msg);
    void DeleteShareFolderReq(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::CREATE_SHARE_FOLDER_REQ,  (MsgCBFunc)&XShareHandle::CreateShareFolderReq);
        RegCB((xmsg::MsgType)xdisk::ADD_SHARE_USER_REQ,       (MsgCBFunc)&XShareHandle::AddShareUserReq);
        RegCB((xmsg::MsgType)xdisk::REMOVE_SHARE_USER_REQ,    (MsgCBFunc)&XShareHandle::RemoveShareUserReq);
        RegCB((xmsg::MsgType)xdisk::GET_SHARED_FOLDERS_REQ,   (MsgCBFunc)&XShareHandle::GetSharedFoldersReq);
        RegCB((xmsg::MsgType)xdisk::GET_SHARED_DIR_REQ,       (MsgCBFunc)&XShareHandle::GetSharedDirReq);
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_SHARED_FILE_REQ, (MsgCBFunc)&XShareHandle::DownloadSharedFileReq);
        RegCB((xmsg::MsgType)xdisk::UPLOAD_SHARED_FILE_REQ,   (MsgCBFunc)&XShareHandle::UploadSharedFileReq);
        RegCB((xmsg::MsgType)xdisk::DELETE_SHARED_FILE_REQ,   (MsgCBFunc)&XShareHandle::DeleteSharedFileReq);
        RegCB((xmsg::MsgType)xdisk::DELETE_SHARE_FOLDER_REQ,  (MsgCBFunc)&XShareHandle::DeleteShareFolderReq);
    }

private:
    // 0=none, 1=read, 2=write, 3=admin
    int GetPermLevel(int64_t folder_id, const std::string &username);
    LX::LXMysql *db_ = nullptr;
};
