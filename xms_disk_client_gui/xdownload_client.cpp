#include "xdownload_client.h"
#include"xmsg_com.pb.h"
#include "xms_disk_client_gui.pb.h"
#include "xfile_manager.h"
#include "xtools.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <vector>
#include <QString>

using namespace std;
using namespace xmsg;
using namespace xdisk;

static constexpr long long DOWNLOAD_SLICE_BYTE = 10000000;

// UTF-8 std::string → filesystem::path（Windows 用 wstring 绕过 ANSI 限制）
static std::filesystem::path utf8_to_path(const std::string &utf8)
{
#ifdef _WIN32
    return std::filesystem::path(QString::fromUtf8(utf8.c_str()).toStdWString());
#else
    return std::filesystem::path(utf8);
#endif
}

bool XDownloadClient::set_file(xdisk::XFileInfo file)
{
    this->file_ = file;

    // 修复：检查并修复重复扩展名问题
    CheckAndFixDuplicateExtension();

    filesystem::path fpath = utf8_to_path(file_.local_path());

    if (fpath.has_parent_path())
    {
        filesystem::path parent = fpath.parent_path();
        if (!filesystem::exists(parent))
        {
            error_code ec;
            if (!filesystem::create_directories(parent, ec))
            {
                string msg = string("无法创建目录: ") + parent.u8string() + " (" + ec.message() + ")";
                cout << msg << endl;
                LOGERROR(msg.c_str());
                XFileManager::Instance()->ErrorSig(msg);
                return false;
            }
        }
    }

    // ====== 断点续传：检查本地是否已有部分下载的文件 ======
    exist_size_ = 0;
    is_resume_ = false;
    if (filesystem::exists(fpath))
    {
        error_code ec;
        long long raw_size = filesystem::file_size(fpath, ec);
        if (!ec && raw_size > 0)
        {
            // 对齐到分片边界，丢弃最后一个可能不完整的分片
            long long aligned = (raw_size / DOWNLOAD_SLICE_BYTE) * DOWNLOAD_SLICE_BYTE;
            if (aligned > 0)
            {
                exist_size_ = aligned;
                is_resume_ = true;
                // 截断本地文件到对齐位置
                filesystem::resize_file(fpath, aligned, ec);
                if (ec)
                {
                    cout << "RESUME_DOWNLOAD: resize_file failed: " << ec.message() << ", starting from scratch" << endl;
                    exist_size_ = 0;
                    is_resume_ = false;
                }
                else
                {
                    cout << "RESUME_DOWNLOAD: local file aligned to " << exist_size_ << " (raw=" << raw_size << ")" << endl;
                }
            }
            // raw_size < one slice: start over, don't resume from partial slice
        }
    }

    // 以追加模式打开文件（续传时不截断已有数据）
    if (is_resume_)
    {
        ofs_.open(fpath, ios::binary | ios::app);
    }
    else
    {
        ofs_.open(fpath, ios::binary);
    }

    if (!ofs_.is_open())
    {
        string msg = string("无法打开文件: ") + file_.local_path() + " (errno=" + to_string(errno) + ": " + strerror(errno) + ")";
        cout << msg << endl;
        LOGERROR(msg.c_str());
        XFileManager::Instance()->ErrorSig(msg);
        return false;
    }
    return true;
}

// 修复：检查文件名是否有重复后缀
bool XDownloadClient::CheckAndFixDuplicateExtension()
{
    filesystem::path fpath = utf8_to_path(file_.local_path());
    string filename = fpath.filename().u8string();

    // 检查是否有重复的扩展名（如 .exe.exe）
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != string::npos && dot_pos > 0)
    {
        string extension = filename.substr(dot_pos);
        size_t prev_dot_pos = filename.find_last_of('.', dot_pos - 1);
        if (prev_dot_pos != string::npos)
        {
            string prev_extension = filename.substr(prev_dot_pos);
            if (extension == prev_extension)
            {
                // 移除重复的扩展名
                string fixed_filename = filename.substr(0, dot_pos);
                filesystem::path fixed_path = fpath.parent_path() / filesystem::path(fixed_filename);
                file_.set_local_path(fixed_path.u8string());
                cout << "Fixed duplicate extension: " << filename << " -> " << fixed_filename << endl;
                return true;
            }
        }
    }
    return false;
}

void XDownloadClient::ConnectedCB()
{
    // 发送下载请求，附带本地已有文件大小（用于断点续传）
    XMsgHead head;
    head.set_msg_type((MsgType)DOWNLOAD_FILE_REQ);
    head.set_offset(exist_size_);
    SendMsg(&head, &file_);
    cout << "XDownloadClient::Connect() offset=" << exist_size_ << endl;
}

//确认文件信息
void XDownloadClient::DownloadFileRes(xmsg::XMsgHead *head, XMsg *msg)
{
    // 保存本地路径，ParseFromArray 会清空整个 proto（包括 local_path）
    string saved_local_path = file_.local_path();
    if (!file_.ParseFromArray(msg->data, msg->size))
    {
        cout << "XDownloadClient::DownloadFileRes ParseFromArray failed!" << endl;
        return;
    }
    file_.set_local_path(saved_local_path);

    cout << "服务端返回文件信息: " << file_.DebugString() << endl;

    if (file_.filesize() == 0)
    {
        string msg = string("服务端文件不存在或为空: ") + file_.filename();
        LOGERROR(msg.c_str());
        XFileManager::Instance()->ErrorSig(msg);
        ofs_.close();
        ClearTimer();
        Close();
        DropInMsg();
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
            ofs_.close();
            ClearTimer();
            Close();
            DropInMsg();
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
    SendMsg((MsgType)DOWNLOAD_FILE_BEGIN, &file_);

    begin_recv_data_size_ = recv_data_size();
    // 断点续传时，初始化 net_size 为已存在文件的大小
    if (is_resume_)
    {
        file_.set_net_size(exist_size_);
        cout << "RESUME_DOWNLOAD: init net_size=" << exist_size_ << endl;
    }
    XFileManager::Instance()->DownloadProcess(task_id, is_resume_ ? exist_size_ : 0LL);

}

void XDownloadClient::DownloadSliceReq(xmsg::XMsgHead *head, XMsg *msg)
{
    cout << "DownloadSliceReq: msg->size=" << msg->size << ", file_.filesize()=" << file_.filesize() << ", file_.net_size()=" << file_.net_size() << endl;
    long long recved = file_.net_size() + msg->size;
    file_.set_net_size(recved);
    const char *data = msg->data;
    long long size = msg->size;
    char *dec_data = nullptr;
    if (file_.is_enc())
    {
        dec_data = new char[msg->size];
        size = aes_->Decrypt((unsigned char*)msg->data, msg->size, (unsigned char*)dec_data);
        if (size <= 0)
        {
            LOGERROR("aes_->Decrypt failed!");
            delete[] dec_data;
            dec_data = nullptr;
            ofs_.close();
            std::error_code ec;
            std::filesystem::remove(utf8_to_path(file_.local_path()), ec);
            XFileManager::Instance()->ErrorSig("Decryption failed: wrong password or corrupted data");
            ClearTimer();
            Close();
            DropInMsg();
            return;
        }


        if (recved > file_.ori_size())
        {
            size = size - (recved - file_.ori_size());
        }
        data = dec_data;
    }

    string md5_base64 = XMD5_base64((unsigned char*)data, size);
    all_md5_base64_ += md5_base64;

    ofs_.write(data, size);
    bool write_failed = ofs_.bad();
    if (dec_data)
    {
        delete[] dec_data;
        dec_data = nullptr;
        data = nullptr;
    }
    if (write_failed)
    {
        string errmsg = string("写入文件失败: ") + file_.local_path() + " ，请检查磁盘空间和路径权限";
        LOGERROR(errmsg.c_str());
        XFileManager::Instance()->ErrorSig(errmsg);
        ofs_.close();
        ClearTimer();
        Close();
        DropInMsg();
        return;
    }

    SendMsg((MsgType)DOWNLOAD_SLICE_RES, &file_);

    // 文件接收结束
    if (file_.filesize() == file_.net_size())
    {
        ofs_.flush();
        ofs_.close();

        cout << "下载完成验证: filesize=" << file_.filesize() << ", net_size=" << file_.net_size() << endl;

        // 校验整个文件的 MD5（断点续传模式下从磁盘读取校验）
        bool md5_ok = true;
        if (!file_.md5().empty())
        {
            if (!is_resume_)
            {
                // 非续传模式：使用累积的分片 MD5 字符串校验
                string file_md5 = XMD5_base64((unsigned char*)all_md5_base64_.data(), all_md5_base64_.size());
                if (file_.md5() != file_md5)
                {
                    md5_ok = false;
                }
            }
            else
            {
                // 续传模式：从磁盘读取完整文件，重新计算各分片 MD5 后校验
                std::ifstream verify_ifs(utf8_to_path(file_.local_path()), std::ios::binary);
                if (verify_ifs)
                {
                    std::string all_md5;
                    std::vector<char> vbuf(DOWNLOAD_SLICE_BYTE);
                    while (verify_ifs.read(vbuf.data(), DOWNLOAD_SLICE_BYTE) || verify_ifs.gcount() > 0)
                    {
                        long long n = verify_ifs.gcount();
                        all_md5 += XMD5_base64((unsigned char*)vbuf.data(), n);
                    }
                    verify_ifs.close();
                    string file_md5 = XMD5_base64((unsigned char*)all_md5.data(), all_md5.size());
                    if (file_.md5() != file_md5)
                    {
                        md5_ok = false;
                    }
                }
            }

            if (!md5_ok)
            {
                // 密码错误或文件损坏，删除已写入的垃圾文件
                std::error_code ec;
                std::filesystem::remove(utf8_to_path(file_.local_path()), ec);
                XFileManager::Instance()->ErrorSig(
                    "Decryption failed: wrong password or file corrupted");
            }
        }

        if (md5_ok)
        {
            cout << "下载完成: " << file_.local_path() << " (size=" << file_.filesize() << ")" << endl;
            XFileManager::Instance()->DownloadEnd(task_id_);
        }
        // md5 失败时不调用 DownloadEnd，任务不标记完成

        ClearTimer();
        Close();
        DropInMsg();
    }
}
//通过定时器跟踪进度
void XDownloadClient::TimerCB()
{
    if (begin_recv_data_size_ < 0)
        return;
    auto size = BufferSize();


    //已发送的数据（续传时包含已存在的部分文件大小）
    long long recved = recv_data_size() - begin_recv_data_size_ + exist_size_;

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
