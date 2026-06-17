#pragma once
#include <QDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <queue>
#include <vector>
#include "xms_disk_client_gui.pb.h"
#include "xfile_manager.h"

class ShareDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShareDialog(XFileManager *xfm, QWidget *parent = nullptr)
        : QDialog(parent), xfm_(xfm)
    {
        setWindowTitle(QString::fromUtf8("共享文件夹"));
        resize(900, 550);

        auto *main_layout = new QHBoxLayout(this);

        // left: folder list
        auto *left = new QVBoxLayout();
        left->addWidget(new QLabel(QString::fromUtf8("共享文件夹"), this));
        folder_list_ = new QListWidget(this);
        left->addWidget(folder_list_);
        auto *btn_create = new QPushButton(QString::fromUtf8("新建共享"), this);
        auto *btn_delete_folder = new QPushButton(QString::fromUtf8("删除共享"), this);
        left->addWidget(btn_create);
        left->addWidget(btn_delete_folder);
        main_layout->addLayout(left, 1);

        // right: file table
        auto *right = new QVBoxLayout();
        right->addWidget(new QLabel(QString::fromUtf8("文件列表"), this));
        file_table_ = new QTableWidget(0, 3, this);
        file_table_->setHorizontalHeaderLabels(
            {QString::fromUtf8("文件名"),
             QString::fromUtf8("大小"),
             QString::fromUtf8("时间")});
        file_table_->horizontalHeader()->setStretchLastSection(true);
        file_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        right->addWidget(file_table_);

        auto *btn_row = new QHBoxLayout();
        auto *btn_download = new QPushButton(QString::fromUtf8("下载"), this);
        auto *btn_upload   = new QPushButton(QString::fromUtf8("上传到此文件夹"), this);
        auto *btn_delete   = new QPushButton(QString::fromUtf8("删除"), this);
        auto *btn_users    = new QPushButton(QString::fromUtf8("管理成员"), this);
        btn_row->addWidget(btn_download);
        btn_row->addWidget(btn_upload);
        btn_row->addWidget(btn_delete);
        btn_row->addWidget(btn_users);
        right->addLayout(btn_row);
        main_layout->addLayout(right, 3);

        connect(btn_create,   &QPushButton::clicked, this, &ShareDialog::CreateFolder);
        connect(btn_delete_folder, &QPushButton::clicked, this, &ShareDialog::DeleteFolder);
        connect(btn_download, &QPushButton::clicked, this, &ShareDialog::DownloadFile);
        connect(btn_upload,   &QPushButton::clicked, this, &ShareDialog::UploadFile);
        connect(btn_delete,   &QPushButton::clicked, this, &ShareDialog::DeleteFile);
        connect(btn_users,    &QPushButton::clicked, this, &ShareDialog::ManageUsers);
        connect(folder_list_, &QListWidget::currentRowChanged,
                this, &ShareDialog::OnFolderSelected);

        connect(xfm_, &XFileManager::SigSharedFolders,
                this, &ShareDialog::RefreshFolders);
        connect(xfm_, &XFileManager::SigSharedDir,
                this, &ShareDialog::RefreshFiles);
        connect(xfm_, &XFileManager::SigSharedUploadAuthorized,
                this, &ShareDialog::OnUploadAuthorized);
        // Refresh the file list only after the server confirms the delete.
        connect(xfm_, &XFileManager::SigSharedDeleteDone,
                this, &ShareDialog::OnDeleteDone);

        xfm_->GetSharedFolders();
    }

public slots:
    void RefreshFolders(std::vector<xdisk::XSharedFolderInfo> folders)
    {
        folders_ = folders;
        folder_list_->clear();
        for (auto &f : folders)
        {
            QString label = QString::fromUtf8(f.name().c_str()) +
                            QString::fromUtf8(" [") +
                            QString::fromUtf8(f.owner().c_str()) +
                            QString::fromUtf8("]");
            folder_list_->addItem(label);
        }
    }
    void RefreshFiles(xdisk::XFileInfoList list)
    {
        files_.clear();
        file_table_->setRowCount(0);
        for (auto &f : list.files())
        {
            files_.push_back(f);
            int row = file_table_->rowCount();
            file_table_->insertRow(row);
            file_table_->setItem(row, 0, new QTableWidgetItem(
                QString::fromUtf8(f.filename().c_str())));
            file_table_->setItem(row, 1, new QTableWidgetItem(
                f.is_dir() ? QString::fromUtf8("-") : QString::number(f.filesize())));
            file_table_->setItem(row, 2, new QTableWidgetItem(
                QString::fromUtf8(f.filetime().c_str())));
        }
    }

private slots:
    void OnFolderSelected(int row)
    {
        if (row < 0 || row >= (int)folders_.size()) return;
        cur_folder_id_ = folders_[row].id();
        cur_shared_path_ = "";
        xfm_->GetSharedDir(cur_folder_id_, cur_shared_path_);
    }
    void CreateFolder()
    {
        bool ok;
        QString name = QInputDialog::getText(
            this, QString::fromUtf8("新建共享文件夹"),
            QString::fromUtf8("文件夹名称:"), QLineEdit::Normal, "", &ok);
        if (!ok || name.isEmpty()) return;
        xfm_->CreateShareFolder(name.toUtf8().constData(), {});
    }
    void DeleteFolder()
    {
        if (cur_folder_id_ <= 0)
        {
            QMessageBox::information(this, "", QString::fromUtf8("请先选择要删除的共享文件夹"));
            return;
        }
        // Find the folder name for the confirmation dialog.
        QString folder_name;
        for (auto &f : folders_)
        {
            if (f.id() == cur_folder_id_)
            {
                folder_name = QString::fromUtf8(f.name().c_str());
                break;
            }
        }
        auto ans = QMessageBox::question(
            this, QString::fromUtf8("确认"),
            QString::fromUtf8("确定删除共享文件夹 \"") + folder_name +
            QString::fromUtf8("\"？\n此操作不可恢复！"));
        if (ans != QMessageBox::Yes) return;
        xfm_->DeleteShareFolder(cur_folder_id_);
        cur_folder_id_ = -1;
    }
    void DownloadFile()
    {
        int row = file_table_->currentRow();
        if (row < 0 || row >= (int)files_.size() || cur_folder_id_ <= 0) return;
        auto &fi = files_[row];
        if (fi.is_dir()) return;
        QString local = QFileDialog::getSaveFileName(
            this, QString::fromUtf8("保存到"),
            QString::fromUtf8(fi.filename().c_str()));
        if (local.isEmpty()) return;
        xfm_->DownloadFromSharedFolder(cur_folder_id_, fi.filename(),
                                        fi.filedir(), local.toUtf8().constData());
    }
    void UploadFile()
    {
        if (cur_folder_id_ <= 0)
        {
            QMessageBox::information(this, "", QString::fromUtf8("请先选择共享文件夹"));
            return;
        }
        QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择上传文件"));
        if (path.isEmpty()) return;
        QFileInfo qfi(path);

        // Enqueue the file info; the actual upload starts in OnUploadAuthorized
        // after the share service validates write permission.  Using a queue
        // handles the case where the user queues multiple uploads before the
        // first authorization response arrives.
        xdisk::XFileInfo fi;
        fi.set_filename(qfi.fileName().toUtf8().constData());
        fi.set_local_path(path.toUtf8().constData());
        pending_uploads_.push(fi);

        xfm_->UploadToSharedFolder(cur_folder_id_, "");
    }
    void OnUploadAuthorized(std::string authorized_filedir)
    {
        if (pending_uploads_.empty()) return;
        auto fi = pending_uploads_.front();
        pending_uploads_.pop();
        fi.set_filedir(authorized_filedir);
        xfm_->UploadFile(fi);
        // Do NOT refresh here — wait for the upload to complete via the normal
        // UploadFileEndRes path, which already calls GetDir on the current path.
    }
    void DeleteFile()
    {
        int row = file_table_->currentRow();
        if (row < 0 || row >= (int)files_.size() || cur_folder_id_ <= 0) return;
        auto &fi = files_[row];
        if (fi.is_dir())
        {
            QMessageBox::information(this, "", QString::fromUtf8("不支持删除目录"));
            return;
        }
        auto ans = QMessageBox::question(
            this, QString::fromUtf8("确认"),
            QString::fromUtf8("删除 ") + QString::fromUtf8(fi.filename().c_str()) + "?");
        if (ans != QMessageBox::Yes) return;
        xfm_->DeleteFromSharedFolder(cur_folder_id_, fi.filename(), fi.filedir());
        // Do NOT call GetSharedDir here — wait for OnDeleteDone to avoid a race
        // where the listing arrives before the delete completes on the server.
    }
    void OnDeleteDone(bool success, std::string /*msg*/)
    {
        if (success && cur_folder_id_ > 0)
            xfm_->GetSharedDir(cur_folder_id_, "");
    }
    void ManageUsers()
    {
        if (cur_folder_id_ <= 0) return;
        QStringList options;
        options << QString::fromUtf8("添加成员")
                << QString::fromUtf8("移除成员");
        bool ok;
        QString choice = QInputDialog::getItem(
            this, QString::fromUtf8("管理成员"),
            QString::fromUtf8("请选择操作:"),
            options, 0, false, &ok);
        if (!ok) return;
        if (choice == QString::fromUtf8("添加成员"))
        {
            QString input = QInputDialog::getText(
                this, QString::fromUtf8("添加成员"),
                QString::fromUtf8("格式: 用户名,权限(read/write/admin)"),
                QLineEdit::Normal, "", &ok);
            if (!ok || input.isEmpty()) return;
            auto parts = input.split(",");
            if (parts.size() < 2) return;
            xdisk::XShareUser u;
            u.set_username(parts[0].trimmed().toUtf8().constData());
            u.set_permission_level(parts[1].trimmed().toUtf8().constData());
            xfm_->AddSharedUser(cur_folder_id_, {u});
        }
        else
        {
            QString input = QInputDialog::getText(
                this, QString::fromUtf8("移除成员"),
                QString::fromUtf8("请输入要移除的用户名:"),
                QLineEdit::Normal, "", &ok);
            if (!ok || input.isEmpty()) return;
            std::vector<std::string> names;
            names.push_back(input.trimmed().toUtf8().constData());
            xfm_->RemoveSharedUser(cur_folder_id_, names);
        }
    }

private:
    XFileManager *xfm_;
    QListWidget  *folder_list_;
    QTableWidget *file_table_;
    std::vector<xdisk::XSharedFolderInfo> folders_;
    std::vector<xdisk::XFileInfo>         files_;
    int64_t cur_folder_id_ = -1;
    std::string cur_shared_path_;
    // Queue of pending uploads awaiting authorization from the share service.
    std::queue<xdisk::XFileInfo> pending_uploads_;
};
