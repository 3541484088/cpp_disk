#include "xshare_client.h"
#include "xfile_manager.h"
#include "xlog_client.h"
#include <iostream>
using namespace std;
using namespace xdisk;
using namespace xmsg;

void XShareClient::ConnectedCB()
{
    // persistent connection ready; no auto-request on connect
}

void XShareClient::CreateShareFolder(const string &name,
                                     const vector<XShareUser> &users)
{
    XCreateShareFolderReq req;
    req.set_name(name);
    for (auto &u : users)
    {
        auto *nu = req.add_users();
        nu->CopyFrom(u);
    }
    SendMsg((MsgType)CREATE_SHARE_FOLDER_REQ, &req);
}

void XShareClient::AddShareUser(int64_t folder_id,
                                const vector<XShareUser> &users)
{
    XAddShareUserReq req;
    req.set_folder_id(folder_id);
    for (auto &u : users)
    {
        auto *nu = req.add_users();
        nu->CopyFrom(u);
    }
    SendMsg((MsgType)ADD_SHARE_USER_REQ, &req);
}

void XShareClient::RemoveShareUser(int64_t folder_id,
                                   const vector<string> &usernames)
{
    XRemoveShareUserReq req;
    req.set_folder_id(folder_id);
    for (auto &u : usernames)
        req.add_usernames(u);
    SendMsg((MsgType)REMOVE_SHARE_USER_REQ, &req);
}

void XShareClient::GetSharedFolders()
{
    XGetSharedFoldersReq req;
    // username field intentionally left empty; server uses the token-verified
    // head->username() and ignores this field.
    SendMsg((MsgType)GET_SHARED_FOLDERS_REQ, &req);
}

void XShareClient::GetSharedDir(int64_t folder_id, const string &path)
{
    XGetSharedDirReq req;
    req.set_folder_id(folder_id);
    req.set_path(path);
    SendMsg((MsgType)GET_SHARED_DIR_REQ, &req);
}

void XShareClient::DownloadSharedFile(int64_t folder_id,
                                      const string &filename,
                                      const string &filedir,
                                      const string &local_path)
{
    XDownloadSharedFileReq req;
    req.set_folder_id(folder_id);
    req.set_filename(filename);
    req.set_filedir(filedir);
    req.set_local_path(local_path);
    SendMsg((MsgType)DOWNLOAD_SHARED_FILE_REQ, &req);
}

void XShareClient::UploadSharedFile(int64_t folder_id, const string &filedir)
{
    XUploadSharedFileReq req;
    req.set_folder_id(folder_id);
    req.set_filedir(filedir);
    SendMsg((MsgType)UPLOAD_SHARED_FILE_REQ, &req);
}

void XShareClient::DeleteSharedFile(int64_t folder_id,
                                    const string &filename,
                                    const string &filedir)
{
    XDeleteSharedFileReq req;
    req.set_folder_id(folder_id);
    req.set_filename(filename);
    req.set_filedir(filedir);
    SendMsg((MsgType)DELETE_SHARED_FILE_REQ, &req);
}

void XShareClient::DeleteShareFolder(int64_t folder_id)
{
    XDeleteShareFolderReq req;
    req.set_folder_id(folder_id);
    SendMsg((MsgType)DELETE_SHARE_FOLDER_REQ, &req);
}

// ===== response callbacks =====

void XShareClient::CreateShareFolderRes(XMsgHead *head, XMsg *msg)
{
    XCreateShareFolderRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.code() != 0)
    {
        XFileManager::Instance()->ErrorSig(string("创建共享文件夹失败: ") + res.msg());
        return;
    }
    GetSharedFolders();  // refresh list
}

void XShareClient::AddShareUserRes(XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.return_() != XMessageRes::OK)
        XFileManager::Instance()->ErrorSig(string("添加用户失败: ") + res.msg());
}

void XShareClient::RemoveShareUserRes(XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.return_() != XMessageRes::OK)
        XFileManager::Instance()->ErrorSig(string("移除用户失败: ") + res.msg());
}

void XShareClient::GetSharedFoldersRes(XMsgHead *head, XMsg *msg)
{
    XGetSharedFoldersRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    vector<XSharedFolderInfo> folders(res.folders().begin(), res.folders().end());
    XFileManager::Instance()->SigSharedFolders(folders);
}

void XShareClient::GetSharedDirRes(XMsgHead *head, XMsg *msg)
{
    XGetSharedDirRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.code() != 0)
    {
        XFileManager::Instance()->ErrorSig(string("获取共享目录失败: ") + res.msg());
        return;
    }
    XFileInfoList list;
    for (auto &f : res.files())
        list.add_files()->CopyFrom(f);
    XFileManager::Instance()->SigSharedDir(list);
}

void XShareClient::DownloadSharedFileRes(XMsgHead *head, XMsg *msg)
{
    XDownloadSharedFileRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.code() != 0)
    {
        XFileManager::Instance()->ErrorSig(string("权限验证失败: ") + res.msg());
        return;
    }
    XFileInfo fi = res.file_info();
    fi.set_filedir(res.authorized_path());
    XFileManager::Instance()->DownloadFile(fi);
}

void XShareClient::UploadSharedFileRes(XMsgHead *head, XMsg *msg)
{
    XUploadSharedFileRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.code() != 0)
    {
        XFileManager::Instance()->ErrorSig(string("权限验证失败: ") + res.msg());
        return;
    }
    XFileManager::Instance()->SigSharedUploadAuthorized(res.authorized_filedir());
}

void XShareClient::DeleteSharedFileRes(XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    bool ok = (res.return_() == XMessageRes::OK);
    if (!ok)
        XFileManager::Instance()->ErrorSig(string("删除失败: ") + res.msg());
    // Emit signal so the dialog can refresh after the delete actually completes.
    XFileManager::Instance()->SigSharedDeleteDone(ok, res.msg());
}

void XShareClient::DeleteShareFolderRes(XMsgHead *head, XMsg *msg)
{
    XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size)) return;
    if (res.return_() != XMessageRes::OK)
    {
        XFileManager::Instance()->ErrorSig(string("删除共享文件夹失败: ") + res.msg());
        return;
    }
    // Refresh the folder list after successful deletion.
    GetSharedFolders();
}
