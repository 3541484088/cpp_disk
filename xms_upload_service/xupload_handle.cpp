/**
 * @file xupload_handle.cpp
 * @brief 文件上传服务消息处理实现
 * 
 * 处理文件上传相关的消息：上传文件请求、发送分片、上传完成等
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _HAS_STD_BYTE 0
#include <windows.h>  // Must be before protobuf headers to avoid ERROR macro conflict
#undef ERROR  // Prevent conflict with protobuf XMessageRes::ERROR enum
#endif
#include "xupload_handle.h"
#include "xtools.h"
#include "xlog_client.h"
#include "LXMysql.h"
#include <filesystem>
#include <sstream>
#ifdef _WIN32
static std::filesystem::path utf8_to_path(const std::string &utf8)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wpath[0], wlen);
    return std::filesystem::path(wpath);
}
#else
static std::filesystem::path utf8_to_path(const std::string &utf8)
{
    return std::filesystem::path(utf8);
}
#endif
using namespace xmsg;
using namespace xdisk;
using namespace std;

// 平台相关的根目录定义
#define DIR_ROOT (GetDirRoot())

// 文件信息文件前缀
#define FILE_INFO_NAME_PRE ".info_"

// Escapes a string for safe embedding in a single-quoted MySQL literal.
static string EscStr(const string &s)
{
    string out;
    out.reserve(s.size() + 4);
    for (char c : s)
    {
        switch (c)
        {
        case '\'': out += "\\'";  break;
        case '\\': out += "\\\\"; break;
        case '\0': out += "\\0";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

// Returns the write permission level (0=none,1=read,2=write,3=admin) for the
// given username on the shared folder identified by folder_id.
// Returns -1 if the DB connection cannot be established.
static int SharedFolderPermLevel(int64_t folder_id, const string &username)
{
    LX::LXMysql db;
    if (!db.Init()) return -1;
    db.SetReconnect(true);
    db.SetConnectTimeout(3);
    if (!db.InputDBConfig()) return -1;
    stringstream ss;
    ss << "SELECT permission_level FROM shared_folder_permissions WHERE folder_id="
       << folder_id << " AND username='" << EscStr(username) << "'";
    auto rows = db.GetResult(ss.str().c_str());
    if (rows.empty() || rows[0].empty()) return 0;
    string lv = rows[0][0].data ? rows[0][0].data : "";
    if (lv == "admin") return 3;
    if (lv == "write") return 2;
    if (lv == "read")  return 1;
    return 0;
}

// 文件分片大小（需与客户端保持一致，用于断点续传对齐）
#define FILE_SLICE_BYTE 10000000

/**
 * @brief 处理上传文件请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析文件信息，创建目标目录，打开文件准备接收数据
 */
void XUploadHandle::UploadFileReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // 验证用户权限
    // 接收到文件请求
    if (!cur_file_.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("UploadFileReq ParseFromArray failed!");
        return;
    }

    XMessageRes res;
    string path = DIR_ROOT;

    // 去掉 filedir 首尾斜杠，避免拼接出 "//" 或 "///"
    string filedir = cur_file_.filedir();
    while (!filedir.empty() && (filedir.front() == '/' || filedir.front() == '\\'))
        filedir.erase(filedir.begin());
    while (!filedir.empty() && (filedir.back() == '/' || filedir.back() == '\\'))
        filedir.pop_back();

    if (filedir.find("_shared/") == 0 || filedir == "_shared" ||
        cur_file_.filedir().find("_shared/") == 0)
    {
        // Shared path: use the trimmed filedir directly (already validated by share service).
        const string &fd = filedir.empty() ? cur_file_.filedir() : filedir;
        // Validate that the path is "_shared/{digits}" (optionally with sub-path).
        size_t slash = fd.find('/', 8); // skip "_shared/"
        string folder_id_str = (slash == string::npos) ? fd.substr(8) : fd.substr(8, slash - 8);
        if (folder_id_str.empty() ||
            folder_id_str.find_first_not_of("0123456789") != string::npos)
        {
            LOGDEBUG("UploadFileReq: malformed shared path (no folder_id)");
            res.set_return_(XMessageRes::ERROR);
            res.set_msg("INVALID_PATH");
            head->set_msg_type((MsgType)UPLOAD_FILE_RES);
            SendMsg(head, &res);
            return;
        }
        // Reject any ".." components in the sub-path after the folder_id.
        string sub = (slash == string::npos) ? "" : fd.substr(slash + 1);
        string seg;
        bool bad = false;
        for (char c : sub + "/")
        {
            if (c == '/' || c == '\\')
            {
                if (seg == "..") { bad = true; break; }
                seg.clear();
            }
            else { seg += c; }
        }
        if (bad)
        {
            LOGDEBUG("UploadFileReq: path traversal attempt in shared sub-path");
            res.set_return_(XMessageRes::ERROR);
            res.set_msg("INVALID_PATH");
            head->set_msg_type((MsgType)UPLOAD_FILE_RES);
            SendMsg(head, &res);
            return;
        }
        // Verify the requesting user actually has write (or admin) permission on
        // this shared folder.  This check guards against clients that bypass the
        // share service and send UPLOAD_FILE_REQ directly to this service.
        int64_t fid = atoll(folder_id_str.c_str());
        int perm = SharedFolderPermLevel(fid, head->username());
        if (perm < 2)
        {
            LOGDEBUG("UploadFileReq: PERMISSION_DENIED for shared folder");
            res.set_return_(XMessageRes::ERROR);
            res.set_msg("PERMISSION_DENIED");
            head->set_msg_type((MsgType)UPLOAD_FILE_RES);
            SendMsg(head, &res);
            return;
        }
        path += fd;
    }
    else
    {
        if (!filedir.empty())
        {
            string seg;
            bool bad = false;
            for (char c : filedir + "/")
            {
                if (c == '/' || c == '\\') { if (seg == "..") { bad = true; break; } seg.clear(); }
                else { seg += c; }
            }
            if (bad)
            {
                LOGDEBUG("UploadFileReq: path traversal attempt");
                res.set_return_(XMessageRes::ERROR);
                res.set_msg("INVALID_PATH");
                head->set_msg_type((MsgType)UPLOAD_FILE_RES);
                SendMsg(head, &res);
                return;
            }
        }
        path += head->username();
        if (!filedir.empty())
        {
            path += "/";
            path += filedir;
        }
    }
    save_dir_ = path;
    path += "/";
    cout << "UploadFileReq path = " << path << endl;
    
    // 创建目录
    XNewDir(path);

    string file_path = path + cur_file_.filename();
    string info_path = path + FILE_INFO_NAME_PRE + cur_file_.filename();

    // ====== 秒传检测：检查目标文件是否已存在且MD5匹配 ======
    is_sec_upload_ = false;
    exist_size_ = 0;
    if (!cur_file_.md5().empty())
    {
        // 检查目标文件和元信息文件是否都存在
        ifstream ifs_file(utf8_to_path(file_path), ios::binary);
        if (ifs_file.is_open())
        {
            ifs_file.close();
            ifstream ifs_info(utf8_to_path(info_path), ios::binary);
            if (ifs_info.is_open())
            {
                xdisk::XFileInfo exist_info;
                if (exist_info.ParseFromIstream(&ifs_info))
                {
                    ifs_info.close();
                    if (exist_info.md5() == cur_file_.md5() &&
                        exist_info.is_enc() == cur_file_.is_enc())
                    {
                        // MD5完全一致，秒传！
                        is_sec_upload_ = true;
                        cout << "SEC_UPLOAD: file already exists, md5 matched! path=" << file_path << endl;
                        LOGINFO("SEC_UPLOAD: file already exists, md5 matched!");
                        
                        res.set_return_(XMessageRes::OK);
                        res.set_msg("SEC_UPLOAD");
                        head->set_msg_type((MsgType)UPLOAD_FILE_RES);
                        SendMsg(head, &res);
                        return;
                    }
                }
                ifs_info.close();
            }

            // ====== 断点续传检测：文件已部分上传，返回已接收大小 ======
            ifstream ifs_size(utf8_to_path(file_path), ios::binary | ios::ate);
            if (ifs_size.is_open())
            {
                exist_size_ = ifs_size.tellg();
                ifs_size.close();

                // 对齐到分片边界，丢弃最后一个可能不完整的分片
                long long safe_offset = (exist_size_ / FILE_SLICE_BYTE) * FILE_SLICE_BYTE;
                if (safe_offset > 0)
                {
                    cout << "RESUME_UPLOAD: file partially exists, exist_size=" << exist_size_
                         << ", safe_offset=" << safe_offset << " path=" << file_path << endl;
                    LOGINFO("RESUME_UPLOAD: file partially exists");

                    // 截断文件到分片对齐位置
                    error_code ec;
                    filesystem::resize_file(utf8_to_path(file_path), safe_offset, ec);
                    if (ec)
                    {
                        cout << "RESUME_UPLOAD: resize_file failed! " << ec.message() << endl;
                        LOGERROR("RESUME_UPLOAD: resize_file failed!");
                        exist_size_ = 0;
                    }
                    else
                    {
                        exist_size_ = safe_offset;
                    }
                }
                else
                {
                    // 文件存在但不足一个分片，从头开始
                    exist_size_ = 0;
                }
            }
        }
    }

    // 以追加模式打开文件（续传时不截断）
    if (exist_size_ > 0)
    {
        ofs_.open(utf8_to_path(file_path), ios::binary | ios::app);
    }
    else
    {
        ofs_.open(utf8_to_path(file_path), ios::binary);
    }

    // 需要校验权限
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    if (!ofs_.is_open())
    {
        stringstream ss;
        ss << "UploadFileReq open file failed!" << file_path;
        res.set_return_(XMessageRes::ERROR);
        res.set_msg(ss.str());
        LOGINFO(ss.str().c_str())
    }

    // 目录修改位实际目录，需要添加公共配置上传目录
    head->set_offset(exist_size_);
    head->set_msg_type((MsgType)UPLOAD_FILE_RES);
    SendMsg(head, &res);
}

/**
 * @brief 处理发送分片请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 接收文件分片数据，校验MD5，写入文件
 */
void XUploadHandle::SendSliceReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // 秒传跳过切片接收
    if (is_sec_upload_)
    {
        XMessageRes res;
        res.set_return_(XMessageRes::OK);
        res.set_msg("SEC_UPLOAD");
        head->set_msg_type((MsgType)SEND_SLICE_RES);
        SendMsg(head, &res);
        return;
    }

    head->set_msg_type((MsgType)SEND_SLICE_RES);
    XMessageRes res;
    if (head->md5().empty())
    {
        cout << "XUploadHandle::SendSliceReq failed! md5 is empty!";
        // 需要校验权限
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("md5 is empty");
        SendMsg(head, &res);
        return;
    }
    
    // 校验MD5
    string md5 = XMD5_base64((unsigned char *)msg->data, msg->size);
    if (head->md5() != md5)
    {
        cout << "XUploadHandle::SendSliceReq failed! md5 is error!";
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("md5 is error");
        SendMsg(head, &res);
        return;
    }

    // 写入文件
    ofs_.write(msg->data, msg->size);
    if (ofs_.bad())
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("Write failed: disk full or I/O error");
        head->set_msg_type((MsgType)SEND_SLICE_RES);
        SendMsg(head, &res);
        return;
    }

    // 需要校验权限
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    SendMsg(head, &res);
}

/**
 * @brief 处理上传完成请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 关闭文件，保存文件信息，验证文件完整性
 */
void XUploadHandle::UploadFileEndReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // 秒传跳过文件结束处理
    if (is_sec_upload_)
    {
        head->set_msg_type((MsgType)UPLOAD_FILE_END_RES);
        XMessageRes res;
        res.set_return_(XMessageRes::OK);
        res.set_msg("SEC_UPLOAD");
        SendMsg(head, &res);
        return;
    }

    ofs_.close();
    
    // 文件信息存储 .filename.info
    string info_path = save_dir_;
    info_path += "/";
    info_path += FILE_INFO_NAME_PRE;
    info_path += cur_file_.filename();
    ofstream ofs;
    ofs.open(utf8_to_path(info_path), ios::binary);
    if (ofs)
    {
        cur_file_.SerializeToOstream(&ofs);
        ofs.close();
    }

    // 验证文件MD5，验证是否正确
    head->set_msg_type((MsgType)UPLOAD_FILE_END_RES);
    XMessageRes res;
    // 需要校验权限
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    SendMsg(head, &res);
}
