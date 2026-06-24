#pragma once

#include "xai_tool_registry.h"
#include "xms_disk_client_gui.pb.h"
#include <string>
#include <list>
#include <vector>
#include <mutex>

// 磁盘信息结构（避免直接依赖 protobuf XDiskInfo）
struct DiskInfoSnapshot {
    int64_t avail = 0;    // 可用空间
    int64_t total = 0;    // 总空间
    int64_t free_space = 0;  // 空闲空间
    int64_t dir_size = 0; // 当前目录大小
};

// 共享文件夹信息快照
struct SharedFolderSnapshot {
    int64_t     id = 0;
    std::string owner;
    std::string name;
    std::string created_at;
    int32_t     user_count = 0;
    int32_t     file_count = 0;
};

// 传输任务快照
struct TransferTaskSnapshot {
    std::string filename;
    std::string filedir;
    int64_t     filesize = 0;
    std::string tasktime;
    bool        is_complete = false;
};

// 工具执行需要的文件系统上下文接口
// 由 UI 层（XAIPanel）实现并注入，避免 xai_lib 直接依赖 XFileManager
class IFileContext {
public:
    virtual ~IFileContext() = default;

    virtual xdisk::XFileInfoList GetDirFiles(const std::string& path) = 0;
    virtual xdisk::XFileInfoList GetDirFilesSync(const std::string& path) = 0;
    virtual std::string GetCurrentDir() = 0;
    virtual xdisk::XFileInfoList GetCurrentFiles() = 0;
    virtual std::vector<TransferTaskSnapshot> GetUploadTasks() = 0;
    virtual std::vector<TransferTaskSnapshot> GetDownloadTasks() = 0;
    virtual std::vector<TransferTaskSnapshot> GetCompletedTasks() = 0;
    virtual DiskInfoSnapshot GetDiskInfo() = 0;
    virtual std::vector<SharedFolderSnapshot> GetSharedFolders() = 0;

    // 创建文件夹（path 为服务端完整路径，如 "/docs"）
    virtual bool CreateFolder(const std::string& path) = 0;

    // 移动文件（将 filename 从 src_dir 移动到 dst_dir）
    virtual bool MoveFile(const std::string& filename, const std::string& src_dir, const std::string& dst_dir) = 0;
};

// 注册所有内置工具到 registry
void RegisterBuiltinTools(XAIToolRegistry& registry, IFileContext* ctx);
