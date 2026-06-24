/**
 * @file xai_panel.cpp
 * @brief AI 助手面板实现
 */
#include "xai_panel.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMetaType>
#include <QSettings>
#include <algorithm>

using namespace std;
using namespace xdisk;

XAIPanel::XAIPanel(XFileManager *fm, QWidget *parent)
    : QWidget(parent), fm_(fm)
{
    ui.setupUi(this);

    assistant_ = new XAIAssistant(this);
    assistant_->SetFileContext(this);

    qRegisterMetaType<QStringList>("QStringList");

    LoadSettings();

    // 连接 assistant 的 signals（QueuedConnection 确保跨线程安全）
    connect(assistant_, &XAIAssistant::OnAIReply,         this, &XAIPanel::OnAIReply,         Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnHighlightFiles,   this, &XAIPanel::OnHighlightFiles,   Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnNavigateDir,      this, &XAIPanel::OnNavigateDir,      Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnSuggestUploadDir, this, &XAIPanel::OnSuggestUploadDir, Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnDeleteConfirm,    this, &XAIPanel::OnDeleteConfirm,    Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnSearchFiles,      this, &XAIPanel::OnSearchFiles,      Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnRequestStart,     this, &XAIPanel::OnRequestStart,     Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnRequestEnd,       this, &XAIPanel::OnRequestEnd,       Qt::QueuedConnection);
    connect(assistant_, &XAIAssistant::OnError,            this, &XAIPanel::OnError,            Qt::QueuedConnection);

    // 连接 UI 按钮
    connect(ui.sendButton,    &QPushButton::clicked, this, &XAIPanel::OnSendClicked);
    connect(ui.clearButton,   &QPushButton::clicked, this, &XAIPanel::OnClearClicked);
    connect(ui.settingsButton,&QPushButton::clicked, this, &XAIPanel::OnSettingsClicked);

    // 连接 XFileManager 信号（缓存数据供 AI 工具使用）
    connect(fm_, &XFileManager::RefreshUploadTask,   this, &XAIPanel::OnRefreshUploadTask);
    connect(fm_, &XFileManager::RefreshDownloadTask, this, &XAIPanel::OnRefreshDownloadTask);
    connect(fm_, &XFileManager::RefreshCompleteTask, this, &XAIPanel::OnRefreshCompleteTask);
    connect(fm_, &XFileManager::RefreshDiskInfo,     this, &XAIPanel::OnRefreshDiskInfo);
    connect(fm_, &XFileManager::SigSharedFolders,    this, &XAIPanel::OnRefreshSharedFolders);

    // 连接 RefreshData 信号，用于支持 GetDirFilesSync 同步获取子目录
    connect(fm_, &XFileManager::RefreshData, this, [this](xdisk::XFileInfoList file_list, std::string cur_dir) {
        {
            std::lock_guard<std::mutex> lock(sync_dir_mutex_);
            if (!sync_dir_request_.empty() && cur_dir == sync_dir_request_) {
                sync_dir_result_ = file_list;
                sync_dir_ready_ = true;
                sync_dir_cv_.notify_one();
            }
        }
        // CreateFolder 完成后服务端会刷新目录，通知等待的 CreateFolder
        {
            std::lock_guard<std::mutex> lock(sync_create_mutex_);
            if (sync_create_pending_) {
                sync_create_done_ = true;
                sync_create_cv_.notify_one();
            }
        }
    });

    // 连接 ErrorSig 信号，用于 CreateFolder 失败通知
    connect(fm_, &XFileManager::ErrorSig, this, [this](std::string) {
        std::lock_guard<std::mutex> lock(sync_create_mutex_);
        if (sync_create_pending_) {
            sync_create_success_ = false;
            sync_create_done_ = true;
            sync_create_cv_.notify_one();
        }
    });

    // 主动请求一次共享文件夹和磁盘信息，确保 AI 工具能获取到数据
    fm_->GetSharedFolders();

    // 回车发送
    connect(ui.inputEdit, &QLineEdit::returnPressed, this, &XAIPanel::OnSendClicked);

    AppendMessage("系统", "AI 助手已就绪。你可以问我关于当前目录文件的问题。", "#888");
}

void XAIPanel::SetCurrentFiles(const XFileInfoList &files, const string &cur_dir)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    current_files_ = files;
    current_dir_   = cur_dir;
}

// IFileContext 接口实现
xdisk::XFileInfoList XAIPanel::GetDirFiles(const std::string& path)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    // 当前只能返回已缓存的当前目录文件
    // 如果 path 和当前目录一致，直接返回缓存
    if (path == current_dir_ || path.empty()) {
        return current_files_;
    }
    // 其他目录：通过 XFileManager 同步请求
    // 注意：这里需要释放锁后再请求，避免死锁
    // 但由于 XFileManager 的接口是异步的，暂时返回空
    return xdisk::XFileInfoList();
}

xdisk::XFileInfoList XAIPanel::GetDirFilesSync(const std::string& path)
{
    // 如果请求的是当前目录，直接返回缓存
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (path == current_dir_ || path.empty()) {
            return current_files_;
        }
    }

    // 准备同步请求
    {
        std::lock_guard<std::mutex> lock(sync_dir_mutex_);
        sync_dir_request_ = path;
        sync_dir_ready_ = false;
        sync_dir_result_ = xdisk::XFileInfoList();
    }

    // 在主线程调用 fm_->GetDir(path) 发起异步请求
    QMetaObject::invokeMethod(this, [this, path]() {
        fm_->GetDir(path);
    }, Qt::QueuedConnection);

    // 等待结果（超时 5 秒，避免永远阻塞）
    {
        std::unique_lock<std::mutex> lock(sync_dir_mutex_);
        bool got = sync_dir_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
            return sync_dir_ready_;
        });

        xdisk::XFileInfoList result;
        if (got) {
            result = sync_dir_result_;
        }

        // 清理请求状态
        sync_dir_request_.clear();
        sync_dir_ready_ = false;

        // 恢复原目录（在主线程）
        std::string orig_dir;
        {
            std::lock_guard<std::mutex> dlock(data_mutex_);
            orig_dir = current_dir_;
        }
        if (orig_dir != path) {
            QMetaObject::invokeMethod(this, [this, orig_dir]() {
                fm_->GetDir(orig_dir);
            }, Qt::QueuedConnection);
        }

        return result;
    }
}

std::string XAIPanel::GetCurrentDir()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_dir_;
}

xdisk::XFileInfoList XAIPanel::GetCurrentFiles()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_files_;
}

std::vector<TransferTaskSnapshot> XAIPanel::GetUploadTasks()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_uploads_;
}

std::vector<TransferTaskSnapshot> XAIPanel::GetDownloadTasks()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_downloads_;
}

std::vector<TransferTaskSnapshot> XAIPanel::GetCompletedTasks()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_completes_;
}

DiskInfoSnapshot XAIPanel::GetDiskInfo()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_disk_info_;
}

std::vector<SharedFolderSnapshot> XAIPanel::GetSharedFolders()
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    return cached_shared_folders_;
}

bool XAIPanel::CreateFolder(const std::string& path)
{
    if (path.empty()) return false;

    // 准备同步等待状态
    {
        std::lock_guard<std::mutex> lock(sync_create_mutex_);
        sync_create_pending_ = true;
        sync_create_done_ = false;
        sync_create_success_ = true;
    }

    // 在主线程调用 fm_->NewDir(path) 发起异步请求
    QMetaObject::invokeMethod(this, [this, path]() {
        fm_->NewDir(path);
    }, Qt::QueuedConnection);

    // 等待结果（超时 5 秒）
    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(sync_create_mutex_);
        ok = sync_create_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
            return sync_create_done_;
        });
    }

    // 清理状态
    {
        std::lock_guard<std::mutex> lock(sync_create_mutex_);
        sync_create_pending_ = false;
        sync_create_done_ = false;
    }

    // 超时视为失败
    if (!ok) return false;
    return sync_create_success_;
}

bool XAIPanel::MoveFile(const std::string& filename, const std::string& src_dir, const std::string& dst_dir)
{
    (void)filename;
    (void)src_dir;
    (void)dst_dir;
    return false;  // 服务端暂不支持移动文件
}

// ─────────────────────────────────────────────
// UI 槽
// ─────────────────────────────────────────────
void XAIPanel::OnSendClicked()
{
    QString text = ui.inputEdit->text().trimmed();
    if (text.isEmpty()) return;

    AppendMessage("你", text, "#2e5bff");
    ui.inputEdit->clear();
    // sendButton 由 OnRequestStart/OnRequestEnd 统一管理，无需在此单独禁用

    assistant_->SendUserMessage(text, current_files_, current_dir_);
}

void XAIPanel::OnClearClicked()
{
    ui.chatDisplay->clear();
    assistant_->ClearHistory();
    AppendMessage("系统", "对话已清除。", "#888");
}

void XAIPanel::OnSettingsClicked()
{
    QStringList types = {"openai (兼容大多数第三方API)", "ollama (本地，免费)", "claude"};
    bool ok = false;

    QString type = QInputDialog::getItem(this, "AI 设置", "API 类型：", types, 0, false, &ok);
    if (!ok) return;

    QString api_type = type.section(' ', 0, 0);

    QString key;
    if (api_type != "ollama")
    {
        key = QInputDialog::getText(this, "AI 设置", "API Key：",
                                    QLineEdit::Password, "", &ok);
        if (!ok) return;
        assistant_->set_api_key(key.toStdString());
    }

    QString url = QInputDialog::getText(this, "AI 设置",
        "API URL（留空使用默认，第三方代理填基础地址如 https://xxx.com）：",
        QLineEdit::Normal, "", &ok);
    if (!ok) return;

    QString model = QInputDialog::getText(this, "AI 设置",
        "模型名称（留空使用默认）：",
        QLineEdit::Normal, "", &ok);
    if (!ok) return;

    assistant_->set_api_type(api_type.toStdString());
    assistant_->set_api_url(url.isEmpty() ? "" : url.toStdString());
    if (!model.isEmpty())
        assistant_->set_model(model.toStdString());

    SaveSettings();
    AppendMessage("系统", "设置已更新，API 类型：" + api_type, "#888");
}

// ─────────────────────────────────────────────
// XAIAssistant 回调槽
// ─────────────────────────────────────────────
void XAIPanel::OnAIReply(QString text)
{
    AppendMessage("AI", text, "#333");
}

void XAIPanel::OnHighlightFiles(QStringList file_names)
{
    emit HighlightFiles(file_names);
}

void XAIPanel::OnNavigateDir(QString dir_path)
{
    emit NavigateDir(dir_path);
}

void XAIPanel::OnSuggestUploadDir(QString dir_path)
{
    // 弹出建议气泡，用户确认后导航
    auto ret = QMessageBox::question(this, "AI 建议",
                                     "AI 建议将文件上传到：" + dir_path + "\n是否导航到该目录？");
    if (ret == QMessageBox::Yes)
        emit NavigateDir(dir_path);
}

void XAIPanel::OnDeleteConfirm(QStringList file_names)
{
    QString msg = "AI 建议删除以下文件：\n" + file_names.join("\n") + "\n\n确认删除？";
    auto ret = QMessageBox::warning(this, "确认删除", msg,
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // 通过 XFileManager 执行批量删除（与主界面行为一致）
    fm_->BeginBatchDelete(file_names.size());
    for (const auto &name : file_names)
    {
        std::string name_str = name.toUtf8().constData();
        xdisk::XFileInfo fi;
        fi.set_filename(name_str);
        fi.set_filedir(current_dir_);
        // 从当前文件列表查找 is_dir
        for (int i = 0; i < current_files_.files_size(); ++i)
        {
            if (current_files_.files(i).filename() == name_str)
            {
                fi.set_is_dir(current_files_.files(i).is_dir());
                break;
            }
        }
        fm_->DeleteFile(fi);
    }
}

void XAIPanel::OnRequestStart()
{
    ui.statusLabel->setText("AI 思考中...");
    ui.sendButton->setEnabled(false);
}

void XAIPanel::OnRequestEnd()
{
    ui.statusLabel->clear();
    ui.sendButton->setEnabled(true);
}

void XAIPanel::OnError(QString error_msg)
{
    AppendMessage("错误", error_msg, "#d32f2f");
}

// ─────────────────────────────────────────────
// 辅助：追加一条消息到对话框
// ─────────────────────────────────────────────
void XAIPanel::AppendMessage(const QString &who, const QString &text, const QString &color)
{
    QString html = QString("<p><span style='color:%1;font-weight:bold;'>%2：</span>"
                           "<span style='color:#333;'>%3</span></p>")
                   .arg(color)
                   .arg(who.toHtmlEscaped())
                   .arg(text.toHtmlEscaped().replace("\n", "<br>"));
    ui.chatDisplay->append(html);
}

void XAIPanel::LoadSettings()
{
    QSettings settings;
    settings.beginGroup("AI");
    QString api_type = settings.value("api_type", "openai").toString();
    QString api_key  = settings.value("api_key", "").toString();
    QString api_url  = settings.value("api_url",
        "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions").toString();
    QString model    = settings.value("model", "qwen-turbo").toString();
    settings.endGroup();

    assistant_->set_api_type(api_type.toStdString());
    assistant_->set_api_key(api_key.toStdString());
    assistant_->set_api_url(api_url.toStdString());
    assistant_->set_model(model.toStdString());

    if (api_key.isEmpty()) {
        AppendMessage("系统",
            "尚未配置 API Key，请点击右上角「设置」按钮输入你的 DashScope API Key。",
            "#e65100");
    }
}

void XAIPanel::SaveSettings()
{
    QSettings settings;
    settings.beginGroup("AI");
    settings.setValue("api_type", QString::fromStdString(assistant_->api_type()));
    settings.setValue("api_key",  QString::fromStdString(assistant_->api_key()));
    settings.setValue("api_url",  QString::fromStdString(assistant_->api_url()));
    settings.setValue("model",    QString::fromStdString(assistant_->model()));
    settings.endGroup();
}

// ─────────────────────────────────────────────
// 搜索文件功能实现
// ─────────────────────────────────────────────
void XAIPanel::OnSearchFiles(const XAIAssistant::SearchCriteria& criteria)
{
    QStringList matched_files;
    
    for (int i = 0; i < current_files_.files_size(); ++i) {
        const auto& file = current_files_.files(i);
        std::string filename = file.filename();
        bool matched = true;
        
        // 检查文件名关键词
        if (!criteria.keyword.empty()) {
            std::string keyword_lower = criteria.keyword;
            std::string filename_lower = filename;
            std::transform(keyword_lower.begin(), keyword_lower.end(), keyword_lower.begin(), ::tolower);
            std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(), ::tolower);
            if (filename_lower.find(keyword_lower) == std::string::npos) {
                matched = false;
            }
        }
        
        // 检查文件类型
        if (matched && !criteria.file_type.empty()) {
            std::string ext = filename.substr(filename.find_last_of(".") + 1);
            std::string type_lower = criteria.file_type;
            std::string ext_lower = ext;
            std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
            std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
            if (ext_lower != type_lower && filename.find(criteria.file_type) == std::string::npos) {
                matched = false;
            }
        }
        
        // 检查文件大小
        if (matched) {
            int64_t file_size = file.filesize();
            if (criteria.min_size >= 0 && file_size < criteria.min_size) {
                matched = false;
            }
            if (criteria.max_size >= 0 && file_size > criteria.max_size) {
                matched = false;
            }
        }
        
        // 检查日期范围（简化实现：比较修改时间）
        if (matched && (!criteria.date_from.empty() || !criteria.date_to.empty())) {
            // 实际项目中需要解析日期并比较，这里简化处理
            // 可以根据实际需求扩展
        }
        
        if (matched) {
            matched_files << QString::fromUtf8(filename.c_str());
        }
    }
    
    if (matched_files.isEmpty()) {
        AppendMessage("系统", "未找到符合条件的文件", "#888");
    } else {
        // 高亮匹配的文件
        emit HighlightFiles(matched_files);

        QString result = QString("找到 %1 个符合条件的文件：\n").arg(matched_files.size());
        result += matched_files.join("\n");
        AppendMessage("系统", result, "#2e7d32");
    }
}

// ─────────────────────────────────────────────
// XFileManager 数据缓存槽
// ─────────────────────────────────────────────
static std::vector<TransferTaskSnapshot> ConvertTaskList(const std::list<xdisk::XFileTask>& tasks)
{
    std::vector<TransferTaskSnapshot> result;
    for (const auto& t : tasks) {
        TransferTaskSnapshot snap;
        snap.filename    = t.file().filename();
        snap.filedir     = t.file().filedir();
        snap.filesize    = t.file().filesize();
        snap.tasktime    = t.tasktime();
        snap.is_complete = t.is_complete();
        result.push_back(snap);
    }
    return result;
}

void XAIPanel::OnRefreshUploadTask(std::list<xdisk::XFileTask> file_list)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    cached_uploads_ = ConvertTaskList(file_list);
}

void XAIPanel::OnRefreshDownloadTask(std::list<xdisk::XFileTask> file_list)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    cached_downloads_ = ConvertTaskList(file_list);
}

void XAIPanel::OnRefreshCompleteTask(std::list<xdisk::XFileTask> file_list)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    cached_completes_ = ConvertTaskList(file_list);
}

void XAIPanel::OnRefreshDiskInfo(xdisk::XDiskInfo info)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    cached_disk_info_.avail      = info.avail();
    cached_disk_info_.total      = info.total();
    cached_disk_info_.free_space = info.free();
    cached_disk_info_.dir_size   = info.dir_size();
}

void XAIPanel::OnRefreshSharedFolders(std::vector<xdisk::XSharedFolderInfo> folders)
{
    std::lock_guard<std::mutex> lock(data_mutex_);
    cached_shared_folders_.clear();
    for (const auto& f : folders) {
        SharedFolderSnapshot snap;
        snap.id         = f.id();
        snap.owner      = f.owner();
        snap.name       = f.name();
        snap.created_at = f.created_at();
        snap.user_count = f.user_count();
        snap.file_count = f.file_count();
        cached_shared_folders_.push_back(snap);
    }
}
