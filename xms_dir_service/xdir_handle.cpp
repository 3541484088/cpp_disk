/**
 * @file xdir_handle.cpp
 * @brief 目录服务消息处理实现
 * 
 * 处理文件目录相关的消息：获取磁盘信息、创建目录、获取目录列表、删除文件等
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _HAS_STD_BYTE 0
#include <windows.h>  // Must be before protobuf headers to avoid ERROR macro conflict
#undef ERROR  // Prevent conflict with protobuf XMessageRes::ERROR enum
#endif
#include "xdir_handle.h"
#include "xtools.h"
#include "xlog_client.h"
#include <filesystem>
#include <fstream>

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

// Rejects paths that contain ".." components to prevent directory traversal.
static bool IsPathSafe(const std::string &path)
{
    if (path.size() > 512) return false;
    std::string seg;
    for (char c : path + "/")
    {
        if (c == '/' || c == '\\')
        {
            if (seg == "..") return false;
            seg.clear();
        }
        else { seg += c; }
    }
    return true;
}

// 平台相关的根目录定义
#define DIR_ROOT (GetDirRoot())

// 文件信息文件前缀
#define FILE_INFO_NAME_PRE ".info_"

// 用户空间大小（1GB）
#define USER_SPACE 1073741824

using namespace xdisk;
using namespace std;
using namespace xmsg;

/**
 * @brief 获取用户路径
 * @param head 消息头
 * @return 用户目录路径
 */
static string GetUserPath(const xmsg::XMsgHead *head)
{
    if (!head)
        return "";
    string path = DIR_ROOT;
    path += head->username();
    path += "/";
    return path;
}

/**
 * @brief 处理获取磁盘信息请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 返回磁盘空间信息，root用户返回系统磁盘信息，普通用户返回用户配额信息
 */
void XDirHandle::GetDiskInfoReq(xmsg::XMsgHead *head, XMsg *msg)
{
    string path = GetUserPath(head);
    XDiskInfo res;
    unsigned long long avail = 0;
    unsigned long long total = 0;
    unsigned long long free = 0;

    // 此步骤存在资源浪费，需要优化
    long long dir_size = GetDirSize(path.c_str());
    res.set_dir_size(dir_size);

    // 如果是root用户，返回全部磁盘空间
    if (head->username() == "root")
    {
        GetDiskSize(path.c_str(), &avail, &total, &free);
        res.set_avail(avail);
        res.set_free(free);
        res.set_total(total);
    }
    else
    {
        // 用户空间，从配置文件读取，或者从用户信息读取，默认10G
        long long user_size = USER_SPACE;
        res.set_avail(user_size - dir_size);
        res.set_free(user_size - dir_size);
        res.set_total(user_size);
    }

    head->set_msg_type((xmsg::MsgType)xdisk::GET_DISK_INFO_RES);
    SendMsg(head, &res);
}

/**
 * @brief 处理创建目录请求
 * @param head 消息头
 * @param msg 消息体
 */
void XDirHandle::NewDirReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XGetDirReq req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("XDirHandle::NewDirReq failed!");
        return;
    }

    string path = GetUserPath(head);
    if (!IsPathSafe(req.root()))
    {
        XMessageRes res;
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)NEW_DIR_RES);
        SendMsg(head, &res);
        return;
    }
    path += req.root();
    path += "/";
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(utf8_to_path(path), ec);
    XMessageRes res;
    if (ec)
    {
        res.set_return_(XMessageRes::ERROR);
        res.set_msg(string("Create dir failed: ") + ec.message());
        head->set_msg_type((MsgType)NEW_DIR_RES);
        SendMsg(head, &res);
        return;
    }
    // 无论是新建还是已存在，只要没有错误就返回 OK
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    head->set_msg_type((MsgType)NEW_DIR_RES);
    SendMsg(head, &res);
}

/**
 * @brief 处理获取目录列表请求
 * @param head 消息头
 * @param msg 消息体
 * 
 * 返回目录下的文件列表
 */
void XDirHandle::GetDirReq(xmsg::XMsgHead *head, XMsg *msg)
{
    // 根目录 + 用户名 + 请求目录
    XGetDirReq req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("XDirHandle::GetDirReq failed!");
        return;
    }
    cout << req.DebugString();
    string path = GetUserPath(head);
    if (!IsPathSafe(req.root()))
    {
        XFileInfoList empty;
        head->set_msg_type((xmsg::MsgType)xdisk::GET_DIR_RES);
        SendMsg(head, &empty);
        return;
    }
    path += req.root();
    // Normalize: strip trailing slashes so GetDirList never receives "//" or "///"
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    cout << "GetDirReq path = " << path << endl;
    
    // 自动创建用户目录（如果不存在）
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(utf8_to_path(path + "/"), ec);
    
    auto files = GetDirList(path);
    cout << "GetDirList returned " << files.size() << " entries for path: " << path << endl;
    XFileInfoList file_list;

    for (auto file : files)
    {
        if (file.filename == "." || file.filename == "..")
            continue;
        // 文件信息文件
        string pre = file.filename.substr(0, strlen(FILE_INFO_NAME_PRE));
        if (pre == FILE_INFO_NAME_PRE)
            continue;
        auto info = file_list.add_files();
        info->set_filename(file.filename);
        info->set_filesize(file.filesize);
        info->set_filetime(file.time_str);
        info->set_is_dir(file.is_dir);

        // 读取 .info_ 元数据文件，填充加密相关字段
        if (!file.is_dir)
        {
            string info_path = path + "/" + FILE_INFO_NAME_PRE + file.filename;
            ifstream ifs_info(utf8_to_path(info_path));
            if (ifs_info)
            {
                xdisk::XFileInfo file_info;
                if (file_info.ParseFromIstream(&ifs_info))
                {
                    info->set_is_enc(file_info.is_enc());
                    info->set_md5(file_info.md5());
                    info->set_ori_size(file_info.ori_size());
                }
                ifs_info.close();
            }
        }
    }
    head->set_msg_type((xmsg::MsgType)xdisk::GET_DIR_RES);
    cout << "GetDirReq: sending " << file_list.files_size() << " files in response" << endl;
    SendMsg(head, &file_list);
}

void XDirHandle::DeleteFileReq(xmsg::XMsgHead *head, XMsg *msg)
{
    XFileInfo req;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        LOGDEBUG("DeleteFileReq ParseFromArray failed!");
        return;
    }
    string path = GetUserPath(head);
    if (!IsPathSafe(req.filedir()) || !IsPathSafe(req.filename()))
    {
        XMessageRes res;
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)DELETE_FILE_RES);
        SendMsg(head, &res);
        return;
    }
    path += req.filedir();
    path += "/";

    string info_path = path;

    path += req.filename();
    info_path += FILE_INFO_NAME_PRE;
    info_path += req.filename();
    
    // 删除文件（如果是目录需要递归删除）
    if (req.is_dir())
    {
        // 递归删除目录及其内容
        XDelDir(path);
    }
    else
    {
        // 删除实际文件
        XDelFile(path);
        // 删除信息文件（目录没有信息文件）
        XDelFile(info_path);
    }
    
    // 判断是否删除成功，检查是否存在文件
    head->set_msg_type((xmsg::MsgType)xdisk::DELETE_FILE_RES);
    XMessageRes res;
    res.set_return_(XMessageRes::OK);
    res.set_msg("OK");
    SendMsg(head, &res);
}

/**
 * @brief XDirHandle构造函数
 */
XDirHandle::XDirHandle()
{
}

/**
 * @brief XDirHandle析构函数
 */
XDirHandle::~XDirHandle()
{
}
