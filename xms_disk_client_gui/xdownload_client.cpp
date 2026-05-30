#include "xdownload_client.h"
#include"xmsg_com.pb.h"
#include "xms_disk_client_gui.pb.h"
#include "xfile_manager.h"
#include "xtools.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>

using namespace std;
using namespace xmsg;
using namespace xdisk;

bool XDownloadClient::set_file(xdisk::XFileInfo file)
{
    this->file_ = file;
    filesystem::path fpath(file.local_path());
    string path = fpath.string();
    cout << "set_file path: " << path << endl;

    if (fpath.has_parent_path())
    {
        filesystem::path parent = fpath.parent_path();
        if (!filesystem::exists(parent))
        {
            error_code ec;
            if (!filesystem::create_directories(parent, ec))
            {
                string msg = string("无法创建目录: ") + parent.string() + " (" + ec.message() + ")";
                cout << msg << endl;
                LOGERROR(msg.c_str());
                XFileManager::Instance()->ErrorSig(msg);
                return false;
            }
        }
    }

    ofs_.open(path, ios::binary);
    if (!ofs_.is_open())
    {
        string msg = string("无法打开文件: ") + path + " (errno=" + to_string(errno) + ": " + strerror(errno) + ")";
        cout << msg << endl;
        LOGERROR(msg.c_str());
        XFileManager::Instance()->ErrorSig(msg);
        return false;
    }
    return true;
}

void XDownloadClient::ConnectedCB()
{
    //XMsgHead head;
    //head.set_msg_type((MsgType)DOWNLOAD_FILE_REQ);
    //head.set_username("root");// 临时测试用，后面改为登陆信息
    //SendMsg(&head, &file_);
    SendMsg((MsgType)DOWNLOAD_FILE_REQ, &file_);
    cout << "XDownloadClient::Connect()" << endl;
}

//确认文件信息
void XDownloadClient::DownloadFileRes(xmsg::XMsgHead *head, XMsg *msg)
{/*
    XFileInfo res;*/
    if (!file_.ParseFromArray(msg->data, msg->size))
    {
        cout << "XDownloadClient::DownloadFileRes ParseFromArray failed!" << endl;
        return;
    }

    cout << "服务端返回文件信息: " << file_.DebugString() << endl;

    if (file_.filesize() == 0)
    {
        string msg = string("服务端文件不存在或为空: ") + file_.filename();
        LOGERROR(msg.c_str());
        XFileManager::Instance()->ErrorSig(msg);
        ofs_.close();
        ClearTimer();
        Close();
        return;
    }

    //文件加密，需要有秘钥
    if (file_.is_enc())
    {
        auto pass = XFileManager::Instance()->password();
        if (pass.empty())
        {
            LOGERROR("please set password");
            //具体的提示的语言，可以根据字符串替换为不同的语言
            XFileManager::Instance()->ErrorSig("NO PASSWORD");
            return;
        }
        aes_ = XAES::Create();
        aes_->SetKey(pass.c_str(),pass.size(),false);
    }

    //如果是加密文件需要验证加密
    int task_id = XFileManager::Instance()->AddDownloadTask(file_);
    task_id_ = task_id;

    //XMsgHead h;
    //h.set_msg_type((MsgType)DOWNLOAD_FILE_BEGTIN);
    //h.set_username("root");// 临时测试用，后面改为登陆信息
    //SendMsg(&h, &file_);
    SendMsg((MsgType)DOWNLOAD_FILE_BEGTIN, &file_);

    begin_recv_data_size_ = recv_data_size();
    XFileManager::Instance()->DownloadProcess(task_id, 0);

}

void XDownloadClient::DownloadSliceReq(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "DownloadSliceReq: msg->size=" << msg->size << ", file_.filesize()=" << file_.filesize() << ", file_.net_size()=" << file_.net_size() << endl;
    long long recved = file_.net_size() + msg->size;
    file_.set_net_size(recved);
    const char *data = msg->data;
    long long size = msg->size;
    if (file_.is_enc())
    {
        char *dec_data = new char[msg->size];
        size = aes_->Decrypt((unsigned char*)msg->data, msg->size, (unsigned char*)dec_data);
        if (size <= 0)
        {
            LOGERROR("aes_->Decrypt failed!");
            delete dec_data;
            return;
        }
        if (recved > file_.ori_size())
        {
            size = size -(recved - file_.ori_size());
        }
        data = dec_data;
    }

    string md5_base64 = XMD5_base64((unsigned char*)data, size);
    // md5_base64s_.push_back(md5_base64);
    all_md5_base64_ += md5_base64;

    ofs_.write(data, size);
    if (ofs_.bad())
    {
        string msg = string("写入文件失败: ") + file_.local_path() + " ，请检查磁盘空间和路径权限";
        LOGERROR(msg.c_str());
        XFileManager::Instance()->ErrorSig(msg);
        if (file_.is_enc())
        {
            delete data;
        }
        ofs_.close();
        ClearTimer();
        Close();
        return;
    }
    if (file_.is_enc())
    {
        delete data;
    }
    //XMsgHead h;
    //h.set_msg_type((MsgType)DOWNLOAD_SLICE_RES);
    //h.set_username("root");// 临时测试用，后面改为登陆信息
    //SendMsg(&h, &file_);
    SendMsg((MsgType)DOWNLOAD_SLICE_RES, &file_);
    //文件接收结束
    if (file_.filesize() == file_.net_size())
    {
        ofs_.flush();

        cout << "下载完成验证: filesize=" << file_.filesize() << ", net_size=" << file_.net_size() << ", all_md5_base64_.size()=" << all_md5_base64_.size() << endl;

        ofs_.close();

        cout << "下载完成: " << file_.local_path() << " (size=" << file_.filesize() << ")" << endl;

        XFileManager::Instance()->DownloadEnd(task_id_);
        //校验整个文件的md5
        string file_md5 = XMD5_base64((unsigned char*)all_md5_base64_.data(), all_md5_base64_.size());
        if (file_.md5() != file_md5)
        {
            cerr << "file is not complete" << endl;
        }

        ClearTimer();
        Close();
    }
}
//通过定时器跟踪进度
void XDownloadClient::TimerCB()
{
    if (begin_recv_data_size_ < 0)
        return;
    auto size = BufferSize();


    //已发送的数据
    long long recved = recv_data_size() - begin_recv_data_size_ ;

    cout << recved << ":" << file_.filesize() << endl;
    XFileManager::Instance()->DownloadProcess(task_id_, recved);
}

void XDownloadClient::Close()
{
    if (ofs_.is_open())
    {
        cout << "下载连接关闭，关闭文件: " << file_.local_path() << endl;
        ofs_.close();
    }
    XServiceClient::Close();
}

XDownloadClient::XDownloadClient()
{
    set_timer_ms(100);
}


XDownloadClient::~XDownloadClient()
{
    if (ofs_.is_open())
    {
        ofs_.close();
    }
    if (aes_)
    {
        aes_->Drop();
        aes_ = NULL;
    }
}
