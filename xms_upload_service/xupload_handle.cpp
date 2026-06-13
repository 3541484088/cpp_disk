/**
 * @file xupload_handle.cpp
 * @brief 文件上传服务消息处理实现
 * 
 * 处理文件上传相关的消息：上传文件请求、发送分片、上传完成等
 */
#include "xupload_handle.h"
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
    path += head->username();
    path += "/";
    path += cur_file_.filedir();
    save_dir_ = path;
    path += "/";
    cout << "UploadFileReq path = " << path << endl;
    
    // 创建目录
    XNewDir(path);

    string file_path = path + cur_file_.filename();
    string info_path = path + FILE_INFO_NAME_PRE + cur_file_.filename();

    // ====== 秒传检测：检查目标文件是否已存在且MD5匹配 ======
    is_sec_upload_ = false;
    if (!cur_file_.md5().empty())
    {
        // 检查目标文件和元信息文件是否都存在
        ifstream ifs_file(file_path, ios::binary);
        if (ifs_file.is_open())
        {
            ifs_file.close();
            ifstream ifs_info(info_path, ios::binary);
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
        }
    }

    // 秒传检测未通过时，检查同名文件是否已存在
    if (XFileExist(file_path))
    {
        // 文件已存在但 MD5 不同或加密状态不同，拒绝覆盖
        res.set_return_(XMessageRes::ERROR);
        res.set_msg("File already exists");
        head->set_msg_type((MsgType)UPLOAD_FILE_RES);
        SendMsg(head, &res);
        return;
    }

    ofs_.open(file_path, ios::binary);

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
        return;
    }

    // 写入文件
    ofs_.write(msg->data, msg->size);

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
    ofs.open(info_path, ios::binary);
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
