/**
 * @file xtools.cpp
 * @brief 工具函数实现文件
 * 
 * 包含常用工具函数：字符串处理、文件操作、加密算法、时间处理等
 */
#include "xtools.h"

#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include "xmsg_com.pb.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>  // 避免protobuf冲突需要的windows头文件
#include <direct.h>
#else
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/vfs.h>
#define _access access
#define _mkdir(d) mkdir(d,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

using namespace std;
using namespace xmsg;

bool XMutex::is_debug = false;
static int mutex_index = 0;    // 互斥锁索引计数器
static int lock_count = 0;     // 加锁次数统计
static int unlock_count = 0;   // 解锁次数统计

/**
 * @brief XMutex构造函数（无消息）
 * @param mux 互斥锁指针
 * 
 * 在构造时自动加锁，支持调试模式下输出锁状态信息
 */
XMutex::XMutex(std::mutex *mux)
{
    mux_ = mux;
    mux->lock();
    mutex_index++;
    lock_count++;
    this->index_ = mutex_index;
    if (is_debug)
        cout << index_ << "|" << msg_ << ":Lock" << "L/U(" << lock_count << "/" << unlock_count << ")" << endl;
}

/**
 * @brief XMutex构造函数（带消息）
 * @param mux 互斥锁指针
 * @param msg 调试消息标识
 * 
 * 在构造时自动加锁，支持调试模式下输出锁状态信息和标识消息
 */
XMutex::XMutex(std::mutex *mux, std::string msg)
{
    mux_ = mux;
    msg_ = msg;
    mux->lock();
    mutex_index++;
    lock_count++;
    this->index_ = mutex_index;
    if (is_debug)
        cout << index_<<"|"<< msg_ << ":Lock"<<"L/U("<< lock_count <<"/"<< unlock_count <<")" << endl;
}

/**
 * @brief XMutex析构函数
 * 
 * 在析构时自动解锁，实现RAII模式的自动锁管理
 */
XMutex::~XMutex()
{
    mux_->unlock();
    unlock_count++;
    if (is_debug)
        cout << index_ << "|" << msg_ << ":UnLock" << "L/U(" << lock_count << "/" << unlock_count << ")" << endl;
}

/**
 * @brief 打印消息内容（调试用）
 * @param head 消息头指针
 * @param msg 消息体指针
 */
void PrintMsg(XMsgHead *head, XMsg *msg)
{
    // stringstream ss;
    // ss << "打印MSG:";
    // if (head)
    // {
    //     ss << head->service_name();
    // }
    // cout << "打印MSG:" << pb_head_->service_name() << " " << msg->size << " " << msg->type << endl;
    // if (msg)
    // {
    //     int32 msg_size = 1;

    //     // 消息大小
    //     MsgType msg_type = 2;

    //     // token 用户认证令牌
    //     string token = 3;

    //     // 微服务名称，用于服务间调用
    //     string service_name = 4;
    // }
}

/**
 * @brief Base64编码函数
 * @param in 输入数据指针
 * @param len 输入数据长度
 * @param out_base64 输出base64字符串缓冲区
 * @return 编码后的字符串长度
 * 
 * 使用OpenSSL的BIO接口实现Base64编码
 */
int Base64Encode(const unsigned char *in, int len, char *out_base64)
{
    if (!in || len <= 0 || !out_base64)
        return 0;
    
    // 创建内存BIO
    auto mem_bio = BIO_new(BIO_s_mem());
    if (!mem_bio) return 0;

    // 创建base64编码BIO
    auto b64_bio = BIO_new(BIO_f_base64());
    if (!b64_bio)
    {
        BIO_free(mem_bio);
        return 0;
    }

    // 拼接BIO链：b64 -> mem
    BIO_push(b64_bio, mem_bio);

    // 向b64 BIO写入数据，自动进行base64编码
    BIO_write(b64_bio, in, len);
    
    // 刷新缓冲区，确保所有数据写入内存BIO
    BIO_flush(b64_bio);

    BUF_MEM *p_data;
    
    // 获取内存BIO中的数据指针
    BIO_get_mem_ptr(b64_bio, &p_data);

    memcpy(out_base64, p_data->data, p_data->length);
    BIO_free_all(b64_bio);
    return p_data->length;
}

/**
 * @brief Base64解码函数
 * @param in 输入base64字符串
 * @param len 输入字符串长度
 * @param out_data 输出解码后数据缓冲区
 * @return 解码后数据长度
 * 
 * 使用OpenSSL的BIO接口实现Base64解码
 */
int Base64Decode(const char *in, int len, unsigned char *out_data)
{
    if (!in || len <= 0 || !out_data)
        return 0;
    
    // 创建内存BIO并初始化为输入数据
    auto mem_bio = BIO_new_mem_buf(in, len + 1);
    if (!mem_bio) return 0;

    // 创建base64解码BIO
    auto b64_bio = BIO_new(BIO_f_base64());
    if (!b64_bio)
    {
        BIO_free(mem_bio);
        return 0;
    }

    // 拼接BIO链：b64 -> mem
    b64_bio = BIO_push(b64_bio, mem_bio);
    BIO_flush(b64_bio);
    
    size_t size = 0;
    BIO_read_ex(b64_bio, out_data, len, &size);
    
    BIO_free_all(mem_bio);
    return size;
}

/**
 * @brief 计算MD5哈希值
 * @param d 输入数据指针
 * @param n 输入数据长度
 * @param md 输出MD5结果缓冲区（至少16字节）
 * @return MD5结果指针（同md参数）
 * 
 * 计算128bit(16字节)的MD5哈希值
 */
unsigned char *XMD5(const unsigned char *d, unsigned long n, unsigned char *md)
{
    return MD5(d, n, md);
}

/**
 * @brief 计算MD5哈希值并转为Base64字符串
 * @param d 输入数据指针
 * @param n 输入数据长度
 * @return Base64编码的MD5结果字符串
 * 
 * 先计算128bit(16字节)的MD5哈希值，再将结果进行Base64编码
 */
std::string XMD5_base64(const unsigned char *d, unsigned long n)
{
    unsigned char buf[16] = { 0 };
    char base64[25] = { 0 };
    XMD5(d, n, buf);
    Base64Encode(buf, 16, base64);
    base64[24] = '\0';
    return base64;
}

/**
 * @brief 根据文件名获取对应的图标路径
 * @param filename 文件名
 * @param is_dir 是否为目录
 * @return 图标路径名称（如"Img", "Doc", "Video"等）
 */
XCOM_API std::string XGetIconFilename(std::string filename, bool is_dir)
{
    string iconpath = "Other";
    
    // 提取文件扩展名
    string filetype = "";
    int pos = filename.find_last_of('.');
    if (pos > 0)
    {
        filetype = filename.substr(pos + 1);
    }
    
    // 转换为小写，统一文件类型判断标准
    transform(filetype.begin(), filetype.end(), filetype.begin(), ::tolower);

    if (is_dir)
    {
        iconpath = "Folder";
    }
    else if (filetype == "jpg" || filetype == "png" || filetype == "gif")
    {
        iconpath = "Img";
    }
    else if (filetype == "doc" || filetype == "docx" || filetype == "wps")
    {
        iconpath = "Doc";
    }
    else if (filetype == "rar" || filetype == "zip" || filetype == "7z" || filetype == "gzip")
    {
        iconpath = "Rar";
    }
    else if (filetype == "ppt" || filetype == "pptx")
    {
        iconpath = "Ppt";
    }
    else if (filetype == "xls" || filetype == "xlsx")
    {
        iconpath = "Xls";
    }
    else if (filetype == "pdf")
    {
        iconpath = "Pdf";
    }
    else if (filetype == "avi" || filetype == "mp4" || filetype == "mov" || filetype == "wmv")
    {
        iconpath = "Video";
    }
    else if (filetype == "mp3" || filetype == "pcm" || filetype == "wav" || filetype == "wma")
    {
        iconpath = "Music";
    }
    else
    {
        iconpath = "Other";
    }
    return iconpath;
}
/**
 * @brief 将文件大小转换为可读字符串
 * @param size 文件大小（字节）
 * @return 格式化后的大小字符串（如"1.5GB", "256MB", "10KB", "512B"）
 */
XCOM_API std::string XGetSizeString(long long size)
{
    string filesize_str = "";
    
    // GB级别的文件大小
    if (size > 1024 * 1024 * 1024)
    {
        double gb_size = (double)size / (double)(1024 * 1024 * 1024);
        long long tmp = gb_size * 100;

        stringstream ss;
        ss << tmp / 100;
        if (tmp % 100 > 0)
            ss << "." << tmp % 100;
        ss << "GB";
        filesize_str = ss.str();
    }
    // MB级别的文件大小
    else if (size > 1024 * 1024)
    {
        double mb_size = (double)size / (double)(1024 * 1024);
        long long tmp = mb_size * 100;

        stringstream ss;
        ss << tmp / 100;
        if (tmp % 100 > 0)
            ss << "." << tmp % 100;
        ss << "MB";
        filesize_str = ss.str();
    }
    // KB级别的文件大小
    else if (size > 1024)
    {
        float kb_size = (float)size / (float)(1024);
        long long tmp = kb_size * 100;
        stringstream ss;
        ss << tmp / 100;
        if (tmp % 100 > 0)
            ss << "." << tmp % 100;
        ss << "KB";
        filesize_str = ss.str();
    }
    // 字节级别的文件大小
    else
    {
        stringstream ss;
        ss << size;
        ss << "B";
        filesize_str = ss.str();
    }
    return filesize_str;
}

/**
 * @brief 将时间戳转换为指定格式的字符串
 * @param timestamp 时间戳（秒），如果为0则使用当前时间
 * @param fmt 格式化字符串（如"%Y-%m-%d %H:%M:%S"）
 * @return 格式化后的时间字符串
 */
XCOM_API std::string XGetTime(int timestamp, std::string fmt)
{
    char time_buf[128] = { 0 };
    time_t tm = 0;
    
    // 如果timestamp为0，使用当前时间
    if (timestamp > 0)
        tm = timestamp;
    else
        tm = time(0);
    
    strftime(time_buf, sizeof(time_buf), fmt.c_str(), localtime(&tm));
    return time_buf;
}

/**
 * @brief 字符串分割函数
 * @param vec 输出的分割结果向量
 * @param str 待分割的字符串
 * @param find 分隔符
 */
XCOM_API void XStringSplit(std::vector<string> &vec, std::string str, std::string find)
{
    int pos1 = 0;
    int pos2 = 0;
    vec.clear();
    
    // 循环查找分隔符并分割字符串
    while ((pos2 = str.find(find, pos1)) != (int)string::npos)
    {
        vec.push_back(str.substr(pos1, pos2 - pos1));
        pos1 = pos2 + find.length();
    }
    
    // 添加最后一段字符串
    string strTemp = str.substr(pos1);
    if ((int)strTemp.size() > 0)
    {
        vec.push_back(str.substr(pos1));
    }
}

/**
 * @brief 字符串替换函数
 * @param str 原始字符串
 * @param str_find 要查找的子串
 * @param str_replace 替换字符串
 * @param sreturn 输出的替换结果
 */
XCOM_API void XStrReplace(string str, string str_find, string str_replace, string &sreturn)
{
    sreturn = "";
    int str_size = str.size();
    int rep_size = str_find.size();
    
    // 逐个字符遍历，查找匹配的子串
    for (int i = 0; i < str_size - (rep_size - 1); i++)
    {
        bool is_find = true;
        for (int j = 0; j < rep_size; j++)
        {
            if (str[i + j] != str_find[j])
            {
                is_find = false;
                break;
            }
        }
        
        if (is_find)
        {
            sreturn += str_replace;
            i += (rep_size - 1);  // 跳过已匹配的字符
        }
        else
        {
            sreturn += str[i];
        }
    }
}
/**
 * @brief 格式化目录路径
 * @param dir 原始目录路径
 * @return 格式化后的目录路径（统一使用'/'分隔符，去除连续分隔符）
 * 
 * 将目录路径中的'\\'转换为'/'，并去除连续的分隔符
 */
XCOM_API std::string XFormatDir(const std::string &dir)
{
    std::string re = "";
    bool is_sep = false; // 标记是否刚遇到分隔符 '/' 或 '\'
    
    for (int i = 0; i < dir.size(); i++)
    {
        if (dir[i] == '/' || dir[i] == '\\')
        {
            // 如果已经是分隔符状态，跳过当前分隔符（避免连续分隔符）
            if (is_sep)
            {
                continue;
            }
            re += '/';
            is_sep = true;
            continue;
        }
        is_sep = false;
        re += dir[i];
    }
    return re;
}

/**
 * @brief 去除字符串首尾空白字符
 * @param s 原始字符串
 * @return 去除首尾空白后的字符串
 */
XCOM_API string XTrim(const string& s)
{
    if (s.length() == 0)
        return s;
    
    // 查找第一个非空白字符位置
    int beg = s.find_first_not_of(" \a\b\f\n\r\t\v");
    // 查找最后一个非空白字符位置
    int end = s.find_last_not_of(" \a\b\f\n\r\t\v");
    
    if (beg == (int)std::string::npos) // 字符串全是空白字符
        return "";
    
    return string(s, beg, end - beg + 1);
}

/**
 * @brief 检查文件是否存在
 * @param s 文件路径
 * @return 文件存在返回true，否则返回false
 */
XCOM_API bool XFileExist(const std::string& s)
{
    if (_access(s.c_str(), 0) == -1)
    {
        return false;
    }
    return true;
}

/**
 * @brief 创建目录（支持多级目录）
 * @param path 要创建的目录路径
 * 
 * 递归创建所有不存在的父目录
 */
XCOM_API void XNewDir(std::string path)
{
    string tmp = XFormatDir(path);

    vector<string> paths;
    XStringSplit(paths, tmp, "/");

    string tmpstr = "";
    for (auto s : paths)
    {
        tmpstr += s + "/";
        // 如果目录不存在则创建
        if (_access(tmpstr.c_str(), 0) == -1)
        {
            _mkdir(tmpstr.c_str());
        }
    }
}

/**
 * @brief 删除文件
 * @param path 要删除的文件路径
 * 
 * Windows平台使用DeleteFileA，Linux平台使用remove
 */
XCOM_API void XDelFile(std::string path)
{
#ifdef _WIN32
    DeleteFileA(path.c_str());
#else
    remove(path.c_str());
#endif
}

/**
 * @brief 获取目录列表
 * @param path 目录路径
 * @return 文件信息列表，包含文件名、大小、是否目录、修改时间等
 * 
 * 返回格式：文件名|文件大小(byte)|是否目录(0/1)|修改时间(如2020-01-22 19:30:13)
 */
XCOM_API std::list< XToolFileInfo > GetDirList(std::string path)
{
    std::list< XToolFileInfo > file_list;

#ifdef _WIN32
    // 使用Windows API遍历目录
    _finddata_t file;
    string dirpath = path + "/*.*";
    
    // 打开目录查找
    intptr_t dir = _findfirst(dirpath.c_str(), &file);
    if (dir < 0)
        return file_list;
    
    char time_buf[128] = { 0 };
    do
    {
        XToolFileInfo file_info;
        if (file.attrib & _A_SUBDIR)
        {
            file_info.is_dir = true;
        }
        file_info.filename = file.name;
        file_info.filesize = file.size;
        file_info.time_write = file.time_write;
        
        // 格式化时间为 "YYYY-MM-DD HH:MM:SS" 格式
        time_t tm = file_info.time_write;
        strftime(time_buf, sizeof(time_buf), "%F %T", localtime(&tm));
        file_info.time_str = time_buf;
        file_list.push_back(file_info);
    } while (_findnext(dir, &file) == 0);
    _findclose(dir);
#else
    // 使用POSIX API遍历目录
    const char *dir = path.c_str();
    DIR *dp = 0;
    struct dirent *entry = 0;
    struct stat statbuf;
    
    dp = opendir(dir);
    if (dp == NULL)
        return file_list;
    
    chdir(dir);
    while ((entry = readdir(dp)) != NULL)
    {
        XToolFileInfo file_info;
        lstat(entry->d_name, &statbuf);
        
        if (S_ISDIR(statbuf.st_mode))
        {
            file_info.is_dir = true;
        }
        file_info.filename = entry->d_name;
        file_info.filesize = statbuf.st_size;
        file_info.time_write = statbuf.st_mtime;
        
        // 格式化时间为 "YYYY-MM-DD HH:MM:SS" 格式
        time_t tm = file_info.time_write;
        char time_buf[32] = {0};
        strftime(time_buf, sizeof(time_buf), "%F %T", localtime(&tm));
        file_info.time_str = time_buf;
        file_list.push_back(file_info);
    }
    closedir(dp);
#endif

    return file_list;
}
/**
 * @brief 获取目录中文件数据（文件名和大小）
 * @param path 目录路径
 * @return 文件数据字符串，格式为 "文件名1,大小1;文件名2,大小2;..."
 * 
 * 仅获取文件（不包含目录），返回格式用于简单的数据传输
 */
XCOM_API std::string GetDirData(std::string path)
{
    string data = "";
#ifdef _WIN32
    _finddata_t file;
    string dirpath = path + "/*.*";
    
    intptr_t dir = _findfirst(dirpath.c_str(), &file);
    if (dir < 0)
        return data;
    
    do
    {
        // 跳过目录，只处理文件
        if (file.attrib & _A_SUBDIR) continue;
        
        char buf[1024] = { 0 };
        sprintf(buf, "%s,%u;", file.name, file.size);
        data += buf;
    } while (_findnext(dir, &file) == 0);
    _findclose(dir);
#else
    const char *dir = path.c_str();
    DIR *dp = 0;
    struct dirent *entry = 0;
    struct stat statbuf;
    
    dp = opendir(dir);
    if(dp == NULL)
        return data;
    
    chdir(dir);
    char buf[1024] = {0};
    while((entry = readdir(dp))!=NULL)
    {
        lstat(entry->d_name, &statbuf);
        // 跳过目录，只处理文件
        if(S_ISDIR(statbuf.st_mode)) continue;
        
        sprintf(buf, "%s,%ld;", entry->d_name, statbuf.st_size);
        data += buf;
    }
    closedir(dp);
#endif
    
    // 去除末尾的分号
    if (!data.empty())
    {
        data = data.substr(0, data.size() - 1);
    }
    return data;
}

/**
 * @brief 递归计算目录大小
 * @param path 目录路径
 * @return 目录总大小（字节）
 * 
 * 递归遍历目录下所有文件和子目录，累加计算总大小
 */
long long GetDirSize(const char * path)
{
    if (!path) return 0;
    
    long long dir_size = 0;
    string dir_new = path;
    string name = "";

#ifdef _WIN32
    _finddata_t file;
    dir_new += "\\*.*";

    intptr_t dir = _findfirst(dir_new.c_str(), &file);
    if (dir < 0)
        return 0;
    
    do
    {
        // 判断是否为目录，排除"."和".."
        if (file.attrib & _A_SUBDIR)
        {
            name = file.name;
            if (name == "." || name == "..")
                continue;
            
            // 递归计算子目录大小
            dir_new = path;
            dir_new += "/";
            dir_new += name;
            dir_size += GetDirSize(dir_new.c_str());
        }
        else
        {
            // 累加文件大小
            dir_size += file.size;
        }
    } while (_findnext(dir, &file) == 0);
    _findclose(dir);
#else
    // Linux/Unix平台使用POSIX API
    DIR *dp = 0;
    struct dirent *entry = 0;
    struct stat statbuf;
    
    dp = opendir(dir_new.c_str());
    if (dp == NULL)
        return 0;
    
    chdir(dir_new.c_str());
    while ((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name, &statbuf);
        
        if (S_ISDIR(statbuf.st_mode))
        {
            name = entry->d_name;
            if (name == "." || name == "..")
                continue;
            
            // 递归计算子目录大小
            dir_new = path;
            dir_new += "/";
            dir_new += entry->d_name;
            dir_size += GetDirSize(dir_new.c_str());
        }
        else
        {
            // 累加文件大小
            dir_size += statbuf.st_size;
        }
    }
    closedir(dp);
#endif
    return dir_size;
}

/**
 * @brief 获取磁盘空间信息
 * @param dir 磁盘路径
 * @param avail 可用空间（输出参数）
 * @param total 总空间（输出参数）
 * @param free 空闲空间（输出参数）
 * @return 获取成功返回true，失败返回false
 */
bool GetDiskSize(const char *dir, unsigned long long *avail, unsigned long long *total, unsigned long long *free)
{
#ifdef _WIN32
    return GetDiskFreeSpaceExA(dir, (ULARGE_INTEGER *)avail, (ULARGE_INTEGER *)total, (ULARGE_INTEGER *)free);
#else
    struct statfs diskInfo;
    statfs(dir, &diskInfo);
    *total = diskInfo.f_blocks * diskInfo.f_bsize;
    *free = diskInfo.f_bfree * diskInfo.f_bsize;
    *avail = diskInfo.f_bavail * diskInfo.f_bsize;
    return true;
#endif
}

/**
 * @brief 通过域名获取IP地址
 * @param host_name 域名
 * @return IP地址字符串，如果解析失败返回"127.0.0.1"
 * 
 * 使用gethostbyname进行域名解析，只返回第一个IP地址
 */
std::string XGetHostByName(std::string host_name)
{
#ifdef _WIN32
    // Windows平台需要初始化Winsock
    static bool is_init = false;
    if (!is_init)
    {
        WORD sockVersion = MAKEWORD(2, 2);
        WSADATA wsaData;
        if (WSAStartup(sockVersion, &wsaData) != 0)
        {
            return "";
        }
        is_init = true;
    }
#endif
    
    auto host = gethostbyname(host_name.c_str());
    if (!host || !host->h_addr_list || !*host->h_addr_list)
        return "127.0.0.1";
    
    return inet_ntoa(*(struct in_addr*)*host->h_addr_list);
}


/**
 * @class CXAES
 * @brief AES加密算法实现类
 * 
 * 基于OpenSSL实现AES加密/解密，支持ECB模式
 * 支持128位(16字节)、192位(24字节)、256位(32字节)密钥长度
 */
class CXAES : public XAES
{
    /**
     * @brief 设置AES密钥
     * @param key 密钥数据
     * @param key_size 密钥长度（字节），支持16/24/32字节
     * @param is_enc true表示加密模式，false表示解密模式
     * @return 设置成功返回true，失败返回false
     * 
     * 根据密钥长度自动选择128/192/256位加密
     * 密钥长度超过32字节或小于等于0会返回失败
     */
    virtual bool SetKey(const char *key, int key_size, bool is_enc) override
    {
        if (key_size > 32 || key_size <= 0)
        {
            cerr << "AES key size error(>32 && <=0 )! key_size= " << key_size << endl;
            return false;
        }
        unsigned char aes_key[32] = { 0 };
        memcpy(aes_key, key, key_size);
        int bit_size = 0;
        if (key_size > 24)
        {
            bit_size = 32 * 8;
        }
        else if (key_size > 16)
        {
            bit_size = 24 * 8;
        }
        else
        {
            bit_size = 16 * 8;
        }

        /*
            if (bits != 128 && bits != 192 && bits != 256)
             return -2;
        */
        if (is_enc)
        {
            is_set_encode = true;
            if (AES_set_encrypt_key(aes_key, bit_size, &aes_) < 0)
            {
                return false;
            }
            return true;
        }
        is_set_decode = true;
        if (AES_set_decrypt_key(aes_key, bit_size, &aes_) < 0)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief 释放对象资源
     */
    virtual void Drop() override
    {
        delete this;
    }

    /**
     * @brief AES加密函数
     * @param in 输入数据
     * @param in_size 输入数据大小
     * @param out 输出缓冲区（需确保足够空间，为16字节的整数倍）
     * @return 加密后数据长度，失败返回0
     * 
     * 使用ECB模式进行加密，每块16字节
     * 如果数据长度不是16的倍数，会自动补零
     */
    virtual long long Encrypt(const unsigned char *in, long long in_size, unsigned char *out) override
    {
        if (!in || in_size <= 0 || !out)
        {
            cerr << "Encrypt input data error" << endl;
            return 0;
        }

        if (!is_set_encode)
        {
            cerr << "Encrypt password not set" << endl;
            return 0;
        }
        
        long long enc_byte = 0;
        unsigned char *p_in = 0;
        unsigned char *p_out = 0;
        unsigned char data[16] = { 0 };
        
        // 按16字节块进行加密
        for (int i = 0; i < in_size; i += 16)
        {
            p_in = (unsigned char *)in + i;
            p_out = out + i;
            
            // 如果剩余数据不足16字节，进行零填充
            if (in_size - i < 16)
            {
                memset(data, 0, sizeof(data));
                memcpy(data, p_in, in_size - i);
                p_in = data;
            }
            enc_byte += 16;
            AES_encrypt(p_in, p_out, &aes_);
        }
        return enc_byte;
    }

    /**
     * @brief AES解密函数
     * @param in 输入数据（加密后的数据）
     * @param in_size 输入数据大小（必须是16的倍数）
     * @param out 输出缓冲区
     * @return 解密后数据长度，失败返回0
     * 
     * 使用ECB模式进行解密，输入数据长度必须是16字节的整数倍
     */
    virtual long long Decrypt(const unsigned char *in, long long in_size, unsigned char *out) override
    {
        if (!in || in_size <= 0 || !out || in_size % 16 != 0)
        {
            cerr << "Decrypt input data error" << endl;
            return 0;
        }

        if (!is_set_decode)
        {
            cerr << "Decrypt password not set" << endl;
            return 0;
        }

        long long enc_byte = 0;
        unsigned char *p_in = 0;
        unsigned char *p_out = 0;
        
        // 按16字节块进行解密
        for (int i = 0; i < in_size; i += 16)
        {
            p_in = (unsigned char *)in + i;
            p_out = out + i;
            enc_byte += 16;
            AES_decrypt(p_in, p_out, &aes_);
        }
        return enc_byte;
    }

private:
    AES_KEY aes_;           // AES密钥结构
    bool is_set_decode = false;  // 解密密钥是否已设置
    bool is_set_encode = false;  // 加密密钥是否已设置
};

/**
 * @brief 创建AES加密对象
 * @return AES对象指针
 */
XAES* XAES::Create()
{
    return new CXAES();
}

/**
 * @brief GBK编码转换为UTF-8编码
 * @param gbk_str GBK编码的字符串
 * @return UTF-8编码的字符串
 * 
 * 用于处理Windows API返回的GBK编码错误信息，转换为UTF-8以便正确显示中文
 */
XCOM_API std::string XGBKToUTF8(const std::string& gbk_str)
{
    std::string result = "";
#ifdef _WIN32
    // GBK转Unicode
    int len = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, NULL, 0);
    if (len <= 0)
        return result;
    
    std::wstring unicode_str(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, &unicode_str[0], len);
    
    // Unicode转UTF-8
    len = WideCharToMultiByte(CP_UTF8, 0, unicode_str.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        return result;
    
    result.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, unicode_str.c_str(), -1, &result[0], len, NULL, NULL);
#else
    // Linux平台使用iconv
    result = gbk_str;
#endif
    return result;
}