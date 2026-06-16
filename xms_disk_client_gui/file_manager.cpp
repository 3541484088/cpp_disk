#include "file_manager.h"
#include <thread>
#include "xtools.h"
#include<fstream>
using namespace xdisk;
using namespace std;


void FileManager::AddTask(xdisk::XFileMsgType type, const google::protobuf::Message *msg_in)
{
    XMutex mutex(&mutex_);
    auto msg_map = msg_map_.mutable_msg_map();// [GET_DIR_REQ].add_task();
    auto task = (*msg_map)[type].add_tasks();
    task->set_type(type); // ����ʡ��
    task->set_msg_ser(msg_in->SerializeAsString());
    cout << msg_map_.DebugString();
}

bool FileManager::GetTask(xdisk::XFileMsgType type, google::protobuf::Message *msg_out)
{
    if (is_exit_)
        return false;
    if (!msg_out)
        return false;
    XMutex mutex(&mutex_);
    auto msg_map = msg_map_.mutable_msg_map();// [GET_DIR_REQ].add_task();
    auto tasks = (*msg_map)[type].mutable_tasks();
    auto t = tasks->begin();
    if (t == tasks->end())
    {
        return false;
    }
    string msg_ser = t->msg_ser();
    bool re = msg_out->ParseFromString(msg_ser);
    tasks->erase(tasks->begin());
    return re;
}

void FileManager::GetDir(std::string root)
{
   
    XGetDirReq req;
    //if (root.empty())
    //    root = "/";
    req.set_root(root);
    root_ = root;
    AddTask(xdisk::GET_DIR_REQ, &req);
    //msg_map_.
    //msg_map_[xdisk::GET_DIR_REQ];
}

void FileManager::UploadFile(std::string filename, std::string file_local_path, std::string remote_dir)
{
    cout << file_local_path << filename << endl;

    ifstream ifs(file_local_path, ios::ate);
    if (!ifs)
    {
        cout << "UploadFile failed!" << file_local_path << endl;
        return;
    }
    long long filesize = ifs.tellg();

    static int task_id = 0;
    task_id++;
    XFileInfo file;
    //file.set_file
    //file_real_path.find_last_of("\\")
    file.set_filename(filename);
    file.set_filedir(remote_dir);
    file.set_local_path(file_local_path);
    file.set_filesize(filesize);
    cout << file.DebugString();
    AddTask(xdisk::UPLOAD_FILE_REQ, &file);
    XFileInfo* f = upload_tasks_.add_files();
    f->CopyFrom(file);
}

//���ȴ�0~1000
void FileManager::UploadProcess(int task_id, long long process)
{

}
//void FileManager::StartThread()
//{
//    std::thread th(&FileManager::Main, this);
//    th.detach();
//}

FileManager::FileManager()
{
}
