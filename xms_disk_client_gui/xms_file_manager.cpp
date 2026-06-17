#include "xms_file_manager.h"
#include "xget_dir_client.h"
#include "xupload_client.h"
#include "xdownload_client.h"
#include "xshare_client.h"
#include "xtools.h"
#include <fstream>
#include <filesystem>
#include <QString>
using namespace xdisk;
using namespace std;

static std::filesystem::path utf8_to_path(const std::string &utf8)
{
#ifdef _WIN32
    return std::filesystem::path(QString::fromUtf8(utf8.c_str()).toStdWString());
#else
    return std::filesystem::path(utf8);
#endif
}

XMSFileManager::XMSFileManager()
{
    instance_ = this;
}


XMSFileManager::~XMSFileManager()
{

}

void XMSFileManager::set_login(xmsg::XLoginRes login)
{
    XGetDirClient::Get()->set_login(&login);
    XShareClient::Get()->set_login(&login);
    XFileManager::set_login(login);
}

void XMSFileManager::DeleteFile(xdisk::XFileInfo file)
{
    XGetDirClient::Get()->DeleteFileReq(file);
}

void XMSFileManager::NewDir(std::string path)
{
    XGetDirClient::Get()->NewDirReq(path);
}

void XMSFileManager::GetDir(std::string root)
{
    // Route shared folder paths to the share service instead of the dir service.
    // Format: "_shared/{folder_id}" or "_shared/{folder_id}/{sub_path}"
    if (root.rfind("_shared/", 0) == 0)
    {
        string rest = root.substr(8); // after "_shared/"
        size_t slash = rest.find('/');
        string id_str = (slash == string::npos) ? rest : rest.substr(0, slash);
        string sub    = (slash == string::npos) ? "" : rest.substr(slash + 1);
        if (!id_str.empty() &&
            id_str.find_first_not_of("0123456789") == string::npos)
        {
            int64_t folder_id = atoll(id_str.c_str());
            root_ = root;
            XShareClient::Get()->GetSharedDir(folder_id, sub);
            return;
        }
    }
    XGetDirReq req;
    req.set_root(root);
    root_ = root;
    XGetDirClient::Get()->GetDirReq(req);
}

void XMSFileManager::InitFileManager(std::string server_ip, int server_port)
{
    XGetDirClient::RegMsgCallback();
    XUploadClient::RegMsgCallback();
    XDownloadClient::RegMsgCallback();
    XShareClient::RegMsgCallback();

    XGetDirClient::Get()->set_server_ip(server_ip.c_str());
    XGetDirClient::Get()->set_server_port(server_port);
    XGetDirClient::Get()->StartConnect();

    XShareClient::Get()->set_server_ip(server_ip.c_str());
    XShareClient::Get()->set_server_port(server_port);
    XShareClient::Get()->StartConnect();
}

void XMSFileManager::CreateShareFolder(const string &name,
                                       const vector<xdisk::XShareUser> &users)
{
    XShareClient::Get()->CreateShareFolder(name, users);
}

void XMSFileManager::GetSharedFolders()
{
    XShareClient::Get()->GetSharedFolders();
}

void XMSFileManager::GetSharedDir(int64_t folder_id, const string &path)
{
    XShareClient::Get()->GetSharedDir(folder_id, path);
}

void XMSFileManager::UploadToSharedFolder(int64_t folder_id,
                                          const string &sub_dir)
{
    XShareClient::Get()->UploadSharedFile(folder_id, sub_dir);
}

void XMSFileManager::AddSharedUser(int64_t folder_id,
                                   const vector<xdisk::XShareUser> &users)
{
    XShareClient::Get()->AddShareUser(folder_id, users);
}

void XMSFileManager::RemoveSharedUser(int64_t folder_id,
                                      const vector<string> &usernames)
{
    XShareClient::Get()->RemoveShareUser(folder_id, usernames);
}

void XMSFileManager::DownloadFromSharedFolder(int64_t folder_id,
                                              const string &filename,
                                              const string &filedir,
                                              const string &local_path)
{
    XShareClient::Get()->DownloadSharedFile(folder_id, filename, filedir, local_path);
}

void XMSFileManager::DeleteFromSharedFolder(int64_t folder_id,
                                            const string &filename,
                                            const string &filedir)
{
    XShareClient::Get()->DeleteSharedFile(folder_id, filename, filedir);
}

void XMSFileManager::DeleteShareFolder(int64_t folder_id)
{
    XShareClient::Get()->DeleteShareFolder(folder_id);
}
void XMSFileManager::DownloadFile(xdisk::XFileInfo file)
{
    string ip = "127.0.0.1";
    int port = DOWNLOAD_PORT;
    cout << file.DebugString() << endl;
    auto servers = download_servers();
    if (servers.service().size() > 0)
    {
        static int index = 0;
        
        index = index % servers.service().size();
        ip = servers.service().at(index).ip();
        port = servers.service().at(index).port();
        index++;
    }
    stringstream ss;
    ss << "download server " << ip << ":" << port;
    LOGINFO(ss.str().c_str());

    auto client = new XDownloadClient();

    client->set_auto_connect(false);
    client->set_auto_delete(true);
    client->set_server_ip(ip.c_str());
    client->set_server_port(port);
    if (!client->set_file(file))
    {
        string msg = string("无法打开文件 ") + file.local_path() + " ，请检查路径权限";
        XFileManager::Instance()->ErrorSig(msg);
        delete client;
        return;
    }
    auto user = login();
    client->set_login(&user);
    client->StartConnect();



}


void XMSFileManager::UploadFile(xdisk::XFileInfo file)
{
    string ip = "127.0.0.1";
    int port = UPLOAD_PORT;
    cout << file.DebugString() << endl;
    //XFileInfo file = task.file();
    
   
    auto servers = upload_servers();
    if (servers.service().size() > 0)
    {
        static int index = 0;
        
        index = index % servers.service().size();
        ip = servers.service().at(index).ip();
        port = servers.service().at(index).port();
        index++;
    }
    stringstream ss;
    ss << "upload server " << ip << ":" << port;
    LOGINFO(ss.str().c_str());



    ifstream ifs(utf8_to_path(file.local_path()), ios::ate);
    if (!ifs)
    {
        cout << "UploadFile failed!" << file.local_path() << endl;
        return;
    }
    long long filesize = ifs.tellg();
    ifs.close();

    //XFileInfo file;
    //file.set_filename(filename);
    //file.set_filedir(remote_dir);
    //file.set_local_path(file_local_path);
    file.set_filesize(filesize);

    // Shared-folder uploads must not be encrypted: other members don't have the
    // uploader's password and would receive garbled data on download.
    bool is_shared = file.filedir().rfind("_shared/", 0) == 0;
    auto pass = password();
    if (!pass.empty() && !is_shared)
    {
        file.set_is_enc(true);
        file.set_password(pass);
    }
    cout << file.DebugString();

    auto client = new XUploadClient();


    client->set_auto_connect(false);
    client->set_auto_delete(true);
    client->set_server_ip(ip.c_str());
    client->set_server_port(port);
    auto user = login();
    client->set_login(&user);
    if (!client->set_file(file))
    {
        string errmsg;
        if (file.filesize() == 0)
            errmsg = string("上传失败：文件为空 ") + file.local_path();
        else
            errmsg = string("上传失败：无法打开文件 ") + file.local_path() + " ，请检查路径权限";
        XFileManager::Instance()->ErrorSig(errmsg);
        delete client;
        return;
    }
    int task_id = AddUploadTask(file);
    client->task_id = task_id;
    client->StartConnect();

    //XFileTask task;
    //auto filetask = new XFileInfo();
    //filetask->CopyFrom(file);
    //task.set_allocated_file(filetask);
    //static int task_id = 0;
    //task_id++;
    //task.set_index(task_id);
    //client->task_id = task_id;
    //task.set_tasktime(XGetTime());
    //uploads_.push_back(task);
    //AddTask(xdisk::UPLOAD_FILE_REQ, &file);
    //XFileInfo* f = upload_tasks_.add_files();
    //f->CopyFrom(file);
}