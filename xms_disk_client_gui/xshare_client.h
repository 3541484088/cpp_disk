#pragma once
#include "xservice_client.h"
#include "xms_disk_client_gui.pb.h"
#include "xmsg.h"

class XShareClient : public XServiceClient
{
public:
    static XShareClient* Get()
    {
        static XShareClient c;
        return &c;
    }
    virtual void ConnectedCB() override;

    void CreateShareFolder(const std::string &name,
                           const std::vector<xdisk::XShareUser> &users);
    void AddShareUser(int64_t folder_id,
                      const std::vector<xdisk::XShareUser> &users);
    void RemoveShareUser(int64_t folder_id,
                         const std::vector<std::string> &usernames);
    void GetSharedFolders();
    void GetSharedDir(int64_t folder_id, const std::string &path);
    void DownloadSharedFile(int64_t folder_id, const std::string &filename,
                            const std::string &filedir,
                            const std::string &local_path);
    void UploadSharedFile(int64_t folder_id, const std::string &filedir);
    void DeleteSharedFile(int64_t folder_id, const std::string &filename,
                          const std::string &filedir);

    // response callbacks
    void CreateShareFolderRes(xmsg::XMsgHead *head, XMsg *msg);
    void AddShareUserRes(xmsg::XMsgHead *head, XMsg *msg);
    void RemoveShareUserRes(xmsg::XMsgHead *head, XMsg *msg);
    void GetSharedFoldersRes(xmsg::XMsgHead *head, XMsg *msg);
    void GetSharedDirRes(xmsg::XMsgHead *head, XMsg *msg);
    void DownloadSharedFileRes(xmsg::XMsgHead *head, XMsg *msg);
    void UploadSharedFileRes(xmsg::XMsgHead *head, XMsg *msg);
    void DeleteSharedFileRes(xmsg::XMsgHead *head, XMsg *msg);

    static void RegMsgCallback()
    {
        RegCB((xmsg::MsgType)xdisk::CREATE_SHARE_FOLDER_RES,  (MsgCBFunc)&XShareClient::CreateShareFolderRes);
        RegCB((xmsg::MsgType)xdisk::ADD_SHARE_USER_RES,       (MsgCBFunc)&XShareClient::AddShareUserRes);
        RegCB((xmsg::MsgType)xdisk::REMOVE_SHARE_USER_RES,    (MsgCBFunc)&XShareClient::RemoveShareUserRes);
        RegCB((xmsg::MsgType)xdisk::GET_SHARED_FOLDERS_RES,   (MsgCBFunc)&XShareClient::GetSharedFoldersRes);
        RegCB((xmsg::MsgType)xdisk::GET_SHARED_DIR_RES,       (MsgCBFunc)&XShareClient::GetSharedDirRes);
        RegCB((xmsg::MsgType)xdisk::DOWNLOAD_SHARED_FILE_RES, (MsgCBFunc)&XShareClient::DownloadSharedFileRes);
        RegCB((xmsg::MsgType)xdisk::UPLOAD_SHARED_FILE_RES,   (MsgCBFunc)&XShareClient::UploadSharedFileRes);
        RegCB((xmsg::MsgType)xdisk::DELETE_SHARED_FILE_RES,   (MsgCBFunc)&XShareClient::DeleteSharedFileRes);
    }

private:
    XShareClient() { set_service_name(SHARE_NAME); }
};
