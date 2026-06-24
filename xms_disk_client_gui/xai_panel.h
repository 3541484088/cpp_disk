#pragma once

#include <QWidget>
#include <QStringList>
#include <mutex>
#include <condition_variable>
#include "ui_xai_panel.h"
#include "xai_assistant.h"
#include "xai_tools.h"
#include "xfile_manager.h"

/**
 * @brief AI 助手面板 Widget
 *
 * 嵌入主界面右侧，提供聊天式交互。
 * 接收来自 XAIAssistant 的 signals，驱动文件列表高亮等操作。
 */
class XAIPanel : public QWidget, public IFileContext
{
    Q_OBJECT

public:
    explicit XAIPanel(XFileManager *fm, QWidget *parent = nullptr);

    /// 外部（主界面）每次目录刷新时调用，更新文件列表上下文
    void SetCurrentFiles(const xdisk::XFileInfoList &files, const std::string &cur_dir);

    // IFileContext 接口实现
    xdisk::XFileInfoList GetDirFiles(const std::string& path) override;
    xdisk::XFileInfoList GetDirFilesSync(const std::string& path) override;
    std::string GetCurrentDir() override;
    xdisk::XFileInfoList GetCurrentFiles() override;
    std::vector<TransferTaskSnapshot> GetUploadTasks() override;
    std::vector<TransferTaskSnapshot> GetDownloadTasks() override;
    std::vector<TransferTaskSnapshot> GetCompletedTasks() override;
    DiskInfoSnapshot GetDiskInfo() override;
    std::vector<SharedFolderSnapshot> GetSharedFolders() override;
    bool CreateFolder(const std::string& path) override;
    bool MoveFile(const std::string& filename, const std::string& src_dir, const std::string& dst_dir) override;

signals:
    /// 需要高亮文件列表中这些文件名的行
    void HighlightFiles(QStringList file_names);

    /// 需要导航到某目录
    void NavigateDir(QString dir_path);

private slots:
    void OnSendClicked();
    void OnClearClicked();
    void OnSettingsClicked();

    // 来自 XAIAssistant 的槽
    void OnAIReply(QString text);
    void OnHighlightFiles(QStringList file_names);
    void OnNavigateDir(QString dir_path);
    void OnSuggestUploadDir(QString dir_path);
    void OnDeleteConfirm(QStringList file_names);
    void OnSearchFiles(const XAIAssistant::SearchCriteria& criteria);
    void OnRequestStart();
    void OnRequestEnd();
    void OnError(QString error_msg);

    // 来自 XFileManager 的槽（缓存数据供 AI 工具使用）
    void OnRefreshUploadTask(std::list<xdisk::XFileTask> file_list);
    void OnRefreshDownloadTask(std::list<xdisk::XFileTask> file_list);
    void OnRefreshCompleteTask(std::list<xdisk::XFileTask> file_list);
    void OnRefreshDiskInfo(xdisk::XDiskInfo info);
    void OnRefreshSharedFolders(std::vector<xdisk::XSharedFolderInfo> folders);

private:
    void AppendMessage(const QString &who, const QString &text, const QString &color);
    void LoadSettings();
    void SaveSettings();

    Ui::XAIPanelClass    ui;
    XAIAssistant        *assistant_ = nullptr;
    XFileManager        *fm_        = nullptr;

    mutable std::mutex   data_mutex_;  // 保护以下缓存数据（主线程写、子线程读）
    xdisk::XFileInfoList current_files_;
    std::string          current_dir_;

    // 用于 GetDirFilesSync：子线程请求子目录文件，主线程填充结果
    std::mutex                sync_dir_mutex_;
    std::condition_variable   sync_dir_cv_;
    std::string               sync_dir_request_;   // 请求的目录路径
    xdisk::XFileInfoList      sync_dir_result_;    // 返回的文件列表
    bool                      sync_dir_ready_ = false;  // 结果是否就绪

    // 用于 CreateFolder 同步等待：子线程发起创建，主线程信号通知结果
    std::mutex                sync_create_mutex_;
    std::condition_variable   sync_create_cv_;
    bool                      sync_create_pending_ = false;  // 是否正在等待
    bool                      sync_create_done_    = false;  // 是否完成
    bool                      sync_create_success_ = true;   // 是否成功

    // 缓存磁盘信息和共享文件夹（由信号更新）
    DiskInfoSnapshot              cached_disk_info_;
    std::vector<SharedFolderSnapshot> cached_shared_folders_;
    std::vector<TransferTaskSnapshot> cached_uploads_;
    std::vector<TransferTaskSnapshot> cached_downloads_;
    std::vector<TransferTaskSnapshot> cached_completes_;
};
