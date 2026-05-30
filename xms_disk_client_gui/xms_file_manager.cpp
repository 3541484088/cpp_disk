#include "xms_file_manager.h"
#include "xget_dir_client.h"
#include "xupload_client.h"
#include "xdownload_client.h"
#include "xtools.h"
#include <fstream>
using namespace xdisk;
using namespace std;

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


    XGetDirClient::Get()->set_server_ip(server_ip.c_str());
    XGetDirClient::Get()->set_server_port(server_port);
    //XGetDirClient::Get()->set_timer_ms(100);
    

    XGetDirClient::Get()->StartConnect();


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
    client->set_auto_delete(false);
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



    ifstream ifs(file.local_path(), ios::ate);
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

    auto pass = password();
    if (!pass.empty())
    {
        file.set_is_enc(true);
        file.set_password(pass);
    }
    cout << file.DebugString();

    auto client = new XUploadClient();

    
    client->set_auto_connect(false);
    client->set_auto_delete(false);
    client->set_server_ip(ip.c_str());
    client->set_server_port(port);
    auto user = login();
    client->set_login(&user);
    if (!client->set_file(file))
    {
        cout << "client->LoadFile failed!" << endl;
        
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