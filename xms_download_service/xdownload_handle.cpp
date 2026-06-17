/**
 * @file xdownload_handle.cpp
 * @brief 文件下载服务消息处理实现
 * 
 * 处理文件下载相关的消息：下载文件请求、发送分片、下载开始等
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _HAS_STD_BYTE 0
#include <windows.h>  // Must be before protobuf headers to avoid ERROR macro conflict
#undef ERROR  // Prevent conflict with protobuf XMessageRes::ERROR enum
#endif
#include "xdownload_handle.h"
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

// Returns the permission level (0=none,1=read,2=write,3=admin) for the given
// username on the shared folder.  Returns -1 on DB connection failure.
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

// 文件分片大小（需与客户端保持一致）
#define FILE_SLICE_BYTE 10000000

/**
 * @brief XDownloadHandle构造函数
 * 
 * 初始化定时器和分片缓冲区
 */
XDownloadHandle::XDownloadHandle()
{
    // 设定定时器用于获取传送进度
    set_timer_ms(100);
    slice_buf_ = new char[FILE_SLICE_BYTE];
};

/**
 * @brief XDownloadHandle析构函数
 */
XDownloadHandle::~XDownloadHandle()
{
    delete[] slice_buf_;
    slice_buf_ = NULL;
}

/**
 * @brief 处理下载文件请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 解析文件信息，打开文件，返回文件大小
 */
void XDownloadHandle::DownloadFileReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // 验证用户权限
    // 接收到文件请求
    if (!file_.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("DownloadFileReq ParseFromArray failed!");
        return;
    }

    // 容错没有信息的情况，由客户端判断文件是否有效
    XMessageRes res;
    string path = DIR_ROOT;

    // 去掉 filedir 首尾斜杠，避免拼接出多余 "/"
    string filedir_clean = file_.filedir();
    while (!filedir_clean.empty() &&
           (filedir_clean.front() == '/' || filedir_clean.front() == '\\'))
        filedir_clean.erase(filedir_clean.begin());
    while (!filedir_clean.empty() &&
           (filedir_clean.back() == '/' || filedir_clean.back() == '\\'))
        filedir_clean.pop_back();

    if (filedir_clean.find("_shared/") == 0 || filedir_clean == "_shared")
    {
        // Validate _shared/{digits}/ format to prevent path traversal.
        const string &fd = filedir_clean;
        size_t slash = fd.find('/', 8); // skip "_shared/"
        bool bad_path = false;
        string folder_id_str = (slash == string::npos) ? fd.substr(8) : fd.substr(8, slash - 8);
        if (folder_id_str.empty() ||
            folder_id_str.find_first_not_of("0123456789") != string::npos)
            bad_path = true;
        // Check each path component for "..".
        if (!bad_path)
        {
            string seg;
            for (char c : fd + "/")
            {
                if (c == '/' || c == '\\')
                {
                    if (seg == "..") { bad_path = true; break; }
                    seg.clear();
                }
                else { seg += c; }
            }
        }
        if (bad_path)
        {
            LOGDEBUG("DownloadFileReq: invalid shared path");
            XFileInfo err_file;
            err_file.set_filesize(0);
            head->set_msg_type((MsgType)DOWNLOAD_FILE_RES);
            SendMsg(head, &err_file);
            return;
        }
        // Verify read permission.  Guards against clients that bypass the share
        // service and send DOWNLOAD_FILE_REQ directly to this service.
        int64_t fid = atoll(folder_id_str.c_str());
        if (SharedFolderPermLevel(fid, head->username()) < 1)
        {
            LOGDEBUG("DownloadFileReq: PERMISSION_DENIED for shared folder");
            XFileInfo err_file;
            err_file.set_filesize(0);
            head->set_msg_type((MsgType)DOWNLOAD_FILE_RES);
            SendMsg(head, &err_file);
            return;
        }
        path += fd;
    }
    else
    {
        if (!filedir_clean.empty())
        {
            string seg;
            bool bad = false;
            for (char c : filedir_clean + "/")
            {
                if (c == '/' || c == '\\') { if (seg == "..") { bad = true; break; } seg.clear(); }
                else { seg += c; }
            }
            if (bad)
            {
                LOGDEBUG("DownloadFileReq: path traversal attempt");
                XFileInfo err_file;
                err_file.set_filesize(0);
                head->set_msg_type((MsgType)DOWNLOAD_FILE_RES);
                SendMsg(head, &err_file);
                return;
            }
        }
        path += head->username();
        if (!filedir_clean.empty())
        {
            path += "/";
            path += filedir_clean;
        }
    }
    path += "/";
    string filedir = path;
    path += file_.filename();

    // 读取文件信息文件
    string info_file = filedir;
    info_file += FILE_INFO_NAME_PRE;
    info_file += file_.filename();
    XFileInfo re_file;
    ifstream ifs(utf8_to_path(info_file));
    if (!ifs || !re_file.ParseFromIstream(&ifs))
    {
        LOGINFO("file info read failed");
        re_file.CopyFrom(file_);
    }
    ifs.close();

    // 读取客户端请求的断点续传偏移量
    resume_offset_ = head->offset();
    if (resume_offset_ > 0)
    {
        cout << "RESUME_DOWNLOAD: client requests resume from offset " << resume_offset_ << endl;
        LOGINFO("RESUME_DOWNLOAD: resume from offset");
    }

    head->set_msg_type((MsgType)DOWNLOAD_FILE_RES);
    ifs_.open(utf8_to_path(path), ios::binary);
    ifs_.seekg(0, ios::end);
    if (!ifs_)
    {
        // 失败返回文件大小为0
        cout << "服务端文件打开失败: " << path << endl;
        re_file.set_filesize(0);
        SendMsg(head, &re_file);
        return;
    }
    long long filesize = ifs_.tellg();
    cout << "服务端文件打开成功: " << path << ", size=" << filesize << endl;
    re_file.set_filesize(filesize);
    ifs_.seekg(0, ios::beg);
    SendMsg(head, &re_file);
}

/**
 * @brief 发送文件分片
 * 
 * 读取文件数据并发送给客户端
 */
void XDownloadHandle::SendSlice()
{
    if (ifs_.eof())
    {
        // 文件到结尾，发送结束
        cout << "SendSlice: 文件已到尾，发送结束" << endl;
        return;
    }

    long long size = FILE_SLICE_BYTE;
    long long remaining = file_.filesize() - resume_offset_;
    if (size > remaining)
        size = remaining;

    ifs_.read(slice_buf_, FILE_SLICE_BYTE);
    size = ifs_.gcount();
    cout << "SendSlice: 读取文件 size=" << size << endl;

    XMsgHead head;
    head.set_msg_type((MsgType)DOWNLOAD_SLICE_REQ);
    XMsg data;
    data.data = slice_buf_;
    data.size = size;
    SendMsg(&head, &data);
}

/**
 * @brief 处理下载分片响应
 * @param head 消息头
 * @param msg 消息体
 * 
 * 客户端确认接收后，继续发送下一个分片
 */
void XDownloadHandle::DownloadSliceRes(xmsg::XMsgHead *head, XMsg *msg)
{
    // 校验MD5后继续发送下一个分片
    SendSlice();
}

/**
 * @brief 处理下载开始请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 开始发送文件分片
 */
void XDownloadHandle::DownloadFileBegin(xmsg::XMsgHead *head, XMsg *msg)
{
    // 断点续传：定位到指定偏移位置
    if (resume_offset_ > 0)
    {
        ifs_.seekg(resume_offset_, ios::beg);
        if (ifs_.fail())
        {
            cout << "RESUME_DOWNLOAD: seekg to " << resume_offset_ << " failed, starting from beginning" << endl;
            LOGERROR("RESUME_DOWNLOAD: seekg failed");
            ifs_.clear();
            ifs_.seekg(0, ios::beg);
            resume_offset_ = 0;
        }
        else
        {
            cout << "RESUME_DOWNLOAD: seeked to " << resume_offset_ << ", starting send from there" << endl;
        }
    }
    SendSlice();
}
