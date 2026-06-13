/**
 * @file xdownload_handle.cpp
 * @brief 文件下载服务消息处理实现
 * 
 * 处理文件下载相关的消息：下载文件请求、发送分片、下载开始等
 */
#include "xdownload_handle.h"
#include "xtools.h"
#include "xlog_client.h"
using namespace xmsg;
using namespace xdisk;
using namespace std;

// 平台相关的根目录定义
#ifdef _WIN32
#define DIR_ROOT "./server_root/"
#else
#define DIR_ROOT "/mnt/xms/"
#endif

// 文件信息文件前缀
#define FILE_INFO_NAME_PRE ".info_"

// 文件分片大小（100MB）
#define FILE_SLICE_BYTE 100000000 

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
    delete slice_buf_;
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
    path += head->username();
    path += "/";
    path += file_.filedir();
    path += "/";
    string filedir = path;
    path += file_.filename();

    // 读取文件信息文件
    string info_file = filedir;
    info_file += FILE_INFO_NAME_PRE;
    info_file += file_.filename();
    XFileInfo re_file;
    ifstream ifs(info_file);
    if (!ifs || !re_file.ParseFromIstream(&ifs))
    {
        LOGINFO("file info read failed");
        re_file.CopyFrom(file_);
    }
    ifs.close();

    head->set_msg_type((MsgType)DOWNLOAD_FILE_RES);
    ifs_.open(path, ios::binary);
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
    if (size > file_.filesize())
        size = file_.filesize();

    ifs_.read(slice_buf_, FILE_SLICE_BYTE);
    size = ifs_.gcount();
    cout << "SendSlice: 读取文件 size=" << size << endl;

    XFileInfo *info = new XFileInfo();
    info->CopyFrom(file_);
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
    SendSlice();
}
