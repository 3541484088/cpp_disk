#include "xupload_client.h"
#include"xmsg_com.pb.h"
#include "xms_disk_client_gui.pb.h"
#include "xfile_manager.h"
#include "xtools.h"
#include <filesystem>
#include <QString>
using namespace std;
using namespace xmsg;
using namespace xdisk;

static std::filesystem::path utf8_to_path(const std::string &utf8)
{
#ifdef _WIN32
    return std::filesystem::path(QString::fromUtf8(utf8.c_str()).toStdWString());
#else
    return std::filesystem::path(utf8);
#endif
}

//接近100M每个任务缓存 100m 任务过多时，开销大
// 每个100兆消息等待回应，至少消耗100ms
// 暂时不考虑 速度，后面要考虑做一个数据块列表
#define FILE_SLICE_BYTE 10000000
//#define FILE_SLICE_BYTE 10000
bool XUploadClient::set_file(xdisk::XFileInfo file)
{
    this->file_ = file;
    ifs_.open(utf8_to_path(file_.local_path()), ios::binary);
    if (!ifs_.is_open())
    {
        return false;
    }

    long long filesize = 0;
    // 获取md5 每个FILE_SLICE_BYTE 生成一个片md5
    // 所有的片md5，再生成一个文件md5
    // 一开始就生成md5的目的是为了后面的秒传

    string all_md5_base64 = "";
    while (!ifs_.eof())
    {
        ifs_.read(slice_buf_, FILE_SLICE_BYTE);
        long long size = ifs_.gcount();
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
        long long dec_size = filesize;
        if (filesize % 16 != 0)
        {
            dec_size = filesize + (16 - filesize % 16);
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

    //当前文件的偏移位置
    long long offset = ifs_.tellg();
    XMsgHead head;
    head.set_msg_type((MsgType)SEND_SLICE_REQ);
    head.set_offset(offset);


    ifs_.read(slice_buf_, FILE_SLICE_BYTE);
    long long size = ifs_.gcount();


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

    SendMsg(&head, &data);
}

void XUploadClient::UploadFileRes(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "UploadFileRes 1 " << endl;
    
    // 检查是否为秒传响应
    xmsg::XMessageRes res;
    if (!res.ParseFromArray(msg->data, msg->size))
    {
        // 解析失败说明收到了非预期的消息，不能继续发送
        cout << "UploadFileRes: ParseFromArray failed, aborting" << endl;
        XFileManager::Instance()->ErrorSig("Upload failed: unexpected server response");
        ClearTimer();
        Close();
        DropInMsg();
        return;
    }

    if (res.msg() == "SEC_UPLOAD")
    {
        cout << "SEC_UPLOAD detected! Skipping file transfer." << endl;
        is_sec_upload_ = true;
        XFileManager::Instance()->InfoSig("秒传：文件已存在，跳过上传");
        XFileManager::Instance()->UploadEnd(task_id);
        string filedir = file_.filedir();
        if (filedir.empty() || filedir == "/")
            XFileManager::Instance()->GetDir("/");
        else
            XFileManager::Instance()->GetDir(filedir);
        ClearTimer();
        Close();
        DropInMsg();
        return;
    }

    // 检查错误响应
    if (res.return_() != XMessageRes::OK)
    {
        cout << "UploadFileRes error: " << res.msg() << endl;
        XFileManager::Instance()->ErrorSig(
            string("Upload failed: ") + res.msg());
        ClearTimer();
        Close();
        DropInMsg();
        return;
    }

    // ====== 断点续传：检查服务端返回的偏移量 ======
    long long resume_offset = head->offset();
    if (resume_offset > 0)
    {
        resume_offset_ = resume_offset;
        cout << "RESUME_UPLOAD: resuming from offset " << resume_offset << endl;

        // 如果续传偏移已达到文件大小，说明文件已完整上传，直接发送结束
        if (resume_offset >= file_.filesize())
        {
            cout << "RESUME_UPLOAD: file already complete, skip to end" << endl;
            SendMsg((MsgType)UPLOAD_FILE_END_REQ, &file_);
            return;
        }

        // 将本地文件指针定位到续传位置
        // 加密文件：resume_offset 是服务端加密文件的字节偏移，需折算回明文偏移
        // 每片加密后大小 = ceil(FILE_SLICE_BYTE / 16) * 16，但 FILE_SLICE_BYTE 本身已是16的倍数
        // 所以加密前后每片大小相同，分片数 = resume_offset / FILE_SLICE_BYTE 对两者都成立
        long long seek_offset = resume_offset;
        if (file_.is_enc())
        {
            long long complete_slices_enc = resume_offset / FILE_SLICE_BYTE;
            seek_offset = complete_slices_enc * FILE_SLICE_BYTE;
            // 明文总大小以 ori_size 为准，seek 不能超出
            if (seek_offset > file_.ori_size())
                seek_offset = file_.ori_size();
        }
        ifs_.seekg(seek_offset, ios_base::beg);
        if (ifs_.fail())
        {
            cout << "RESUME_UPLOAD: seekg failed!" << endl;
            XFileManager::Instance()->ErrorSig("Upload resume failed: seek error");
            ClearTimer();
            Close();
            DropInMsg();
            return;
        }

        // 计算已完整发送的分片数，弹出对应的 MD5
        long long complete_slices = resume_offset / FILE_SLICE_BYTE;
        for (long long i = 0; i < complete_slices && !md5_base64s_.empty(); i++)
        {
            md5_base64s_.pop_front();
        }
        cout << "RESUME_UPLOAD: skipped " << complete_slices << " slices, "
             << md5_base64s_.size() << " slices remaining" << endl;
    }

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
    // 续传时包含已发送的起始偏移量；clamp 到 0 防止 BufferSize 大于已发数据时出现负值
    long long sended = send_data_size() - begin_send_data_size_ - BufferSize() + resume_offset_;
    if (sended < 0) sended = 0;

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
    delete[] slice_buf_;
    slice_buf_ = NULL;

    delete[] slice_buf_enc_;
    slice_buf_enc_ = NULL;
    if (aes_)
    {
        aes_->Drop();
        aes_ = NULL;
    }
}
