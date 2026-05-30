#include "xupload_client.h"
#include"xmsg_com.pb.h"
#include "xms_disk_client_gui.pb.h"
#include "xfile_manager.h"
#include "xtools.h"
using namespace std;
using namespace xmsg;
using namespace xdisk;
//接近100M每个任务缓存 100m 任务过多时，开销大
// 每个100兆消息等待回应，至少消耗100ms
// 暂时不考虑 速度，后面要考虑做一个数据块列表
#define FILE_SLICE_BYTE 10000000 
//#define FILE_SLICE_BYTE 10000 
bool XUploadClient::set_file(xdisk::XFileInfo file)
{
    this->file_ = file;
    ifs_.open(file_.local_path(), ios::binary);
    if (!ifs_.is_open())
    {
        return false;
    }

    int filesize = 0;
    // 获取md5 每个FILE_SLICE_BYTE 生成一个片md5
    // 所有的片md5，再生成一个文件md5
    // 一开始就生成md5的目的是为了后面的秒传

    char md5_base64[17] = { 0 };
    string all_md5_base64 = "";
    while (!ifs_.eof())
    {
        ifs_.read(slice_buf_, FILE_SLICE_BYTE);
        int size = ifs_.gcount();
        filesize += size;

        string md5_base64 = XMD5_base64((unsigned char*)slice_buf_, size);
        md5_base64s_.push_back(md5_base64);
        all_md5_base64 += md5_base64;
        //cout << "[" << md5_base64 << "]" << flush;
    }
    if (filesize == 0)
    {
        ifs_.close();
        return false;
    }
    //生成文件的md5 解密后校验，普通文件一开始就生成，加密文件不考虑秒传
    string file_md5 = XMD5_base64((unsigned char*)all_md5_base64.c_str(), all_md5_base64.size());
    file_.set_md5(file_md5);

    file_.set_filesize(filesize);

    //如果是加密文件 文件大小补齐16的倍数
    if (file_.is_enc())
    {

        int dec_size = 0;
        if (filesize % 16 != 0)
        {
            dec_size = filesize  + (16 - filesize % 16);
        }
        file_.set_filesize(dec_size);
        file_.set_ori_size(filesize);
    }
    ifs_.clear();
    ifs_.seekg(0, ios_base::beg);
    cout << file_.DebugString();

    //int size = ifs_.tellg ();

    if (file_.is_enc())
    {
        auto pass = file_.password();
        if (pass.empty())
        {
            LOGERROR("please set  password");
            return false;
        }
        if (!aes_)
        {
            aes_ = XAES::Create();
        }
        aes_->SetKey(pass.c_str(), pass.size(), true);
    }
    return true;
}

void XUploadClient::ConnectedCB()
{
    auto file = file_;
    file.clear_password(); //不要发送密码
    SendMsg((MsgType)UPLOAD_FILE_REQ, &file);
    cout << "XUploadClient::Connect()" << endl;
}

void XUploadClient::SendSlice()
{
    if (ifs_.eof())
    {
        SendMsg((MsgType)UPLOAD_FILE_END_REQ, &file_);
        //文件到结尾，发送结束
        return;
    }

    long long size = FILE_SLICE_BYTE;
    if (size > file_.filesize())
        size = file_.filesize();

    //当前文件的偏移位置
    long long offset = ifs_.tellg();
    XMsgHead head;
    head.set_msg_type((MsgType)SEND_SLICE_REQ);
    head.set_offset(offset);


    ifs_.read(slice_buf_, FILE_SLICE_BYTE);
    size = ifs_.gcount();


    XMsg data;
    data.data = slice_buf_;
    data.size = size;

    //如果需要加密
    if (file_.is_enc())
    {
        long long enc_size = 0;

        if (!aes_)
        {
            LOGERROR("aes not init!");
            return;
        }
        enc_size = aes_->Encrypt((unsigned char*)slice_buf_, size, (unsigned char*)slice_buf_enc_);
        data.data = slice_buf_enc_;
        data.size = enc_size;
        string md5_base64 = XMD5_base64((unsigned char*)slice_buf_enc_, enc_size);
        head.set_md5(md5_base64);
    }
    else
    {
        //未加密
        if (!md5_base64s_.empty())
        {
            head.set_md5(md5_base64s_.front());
            md5_base64s_.pop_front();
        }
    }

    XFileInfo *info = new XFileInfo();
    info->CopyFrom(file_);
    SendMsg(&head, &data);
}

void XUploadClient::UploadFileRes(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "UploadFileRes 1 " << endl;
    //开始发送数据时，已经发送的值，要确保缓冲已经都发送成功
    //根据协议，接收到服务器的反馈，缓冲肯定已发送完毕
    begin_send_data_size_ = send_data_size();
    //开始发送文件
    SendSlice();
}

void XUploadClient::SendSliceRes(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "SendSliceRes 2 " << endl;
    SendSlice();
}

void XUploadClient::UploadFileEndRes(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "UploadFileEndRes 3" << endl;
    XFileManager::Instance()->UploadEnd(task_id);
    
    // 刷新上传文件所在的目录，而不是使用可能过期的 root_
    string filedir = file_.filedir();
    if (filedir.empty() || filedir == "/")
    {
        XFileManager::Instance()->GetDir("/");
    }
    else
    {
        XFileManager::Instance()->GetDir(filedir);
    }
    
    //任务完成刷新界面
    ClearTimer();
    Close();
    DropInMsg();
}

//通过定时器跟踪进度
void XUploadClient::TimerCB()
{
    if (begin_send_data_size_ < 0)
        return;
    auto size = BufferSize();
    
    
    //已发送的数据 不完全准确，还有确认的数据包发送
    long long sended = send_data_size() - begin_send_data_size_ - BufferSize();

    cout << sended <<":"<< file_.filesize() << endl;

    //如果数据过大，先缩小
    XFileManager::Instance()->UploadProcess(task_id, sended);
}

XUploadClient::XUploadClient()
{
    slice_buf_ = new char[FILE_SLICE_BYTE];

    //不是16的倍数要补全，所以要多预留空间
    slice_buf_enc_ = new char[FILE_SLICE_BYTE+16];
    //通过定时器跟踪进度
    set_timer_ms(100);
}


XUploadClient::~XUploadClient()
{
    delete slice_buf_;
    slice_buf_ = NULL;

    delete slice_buf_enc_;
    slice_buf_enc_ = NULL;
    if (aes_)
    {
        aes_->Drop();
        aes_ = NULL;
    }
}
