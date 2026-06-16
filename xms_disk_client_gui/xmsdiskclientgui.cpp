/**
 * @file xmsdiskclientgui.cpp
 * @brief 磁盘客户端GUI实现
 * 
 * 提供文件管理界面，包括文件列表显示、上传下载等功能
 */
#include "xmsdiskclientgui.h"
#include <QMouseEvent>
#include <QMenu>
#include <QHBoxLayout>
#include <sstream>
#include <list>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include "filepassword.h"
#include "task_list_gui.h"
#include "share_dialog.h"
#include "xtools.h"
using namespace std;
using namespace xdisk;

static list<XFileInfo> file_list;
static list<QCheckBox *> check_list;
static TaskListGUI *task_gui = 0;

#define FILE_ICON_PATH ":/XMSDiskClientGui/Resources/img/FileType/Small/"

XMSDiskClientGui::~XMSDiskClientGui()
{
    
}

void XMSDiskClientGui::ErrorSlot(std::string err)
{
    QMessageBox::information(this, "XMS ERROR", QString::fromUtf8(err.c_str()));
}

void XMSDiskClientGui::InfoSlot(std::string msg)
{
    QMessageBox::information(this, QString::fromUtf8("提示"), QString::fromUtf8(msg.c_str()));
}

XMSDiskClientGui::XMSDiskClientGui(XFileManager *xfm, QWidget *parent)
    : QWidget(parent)
{    
    ui.setupUi(this);
    set_xfm(xfm);

    // 去掉原窗口边框
    setWindowFlags(Qt::FramelessWindowHint);

    // 设置背景透明，实现圆角
    setAttribute(Qt::WA_TranslucentBackground);

    setMouseTracking(true);
    auto head = ui.filetableWidget->horizontalHeader();
    head->setDefaultAlignment(Qt::AlignLeft);
    auto tab = ui.filetableWidget;
    tab->setColumnWidth(0, 40);     // checkall
    tab->setColumnWidth(1, 500);    // filename
    tab->setColumnWidth(2, 150);    // time
    tab->setColumnWidth(3, 100);    // size
    auto hitem = tab->horizontalHeaderItem(0);
    qRegisterMetaType<std::list<XFileInfo>>("std::list<FileInfo>");
    qRegisterMetaType< std::string>("std::string");
    qRegisterMetaType<xdisk::XFileInfoList>("xdisk::XFileInfoList");
    

    
    //xfm_ = XFileManager::Instance();
    //connect(FileManager::Get(), SIGNAL(RefreshData(xdisk::XFileInfoList)), this, SLOT(RefreshData(xdisk::XFileInfoList)));
    connect(this->xfm_, SIGNAL(RefreshData(xdisk::XFileInfoList, std::string)), this, SLOT(RefreshData(xdisk::XFileInfoList, std::string)));
    //this->xfm_->GetDir("/");
    while (tab->rowCount() > 0)
    {
        tab->removeRow(0);
    }

    task_gui = new TaskListGUI(this);
    task_gui->hide();

    qRegisterMetaType<std::list<xdisk::XFileTask>>("std::list<xdisk::XFileTask>");
    connect(this->xfm_, SIGNAL(RefreshUploadTask(std::list<xdisk::XFileTask>)), task_gui, SLOT(RefreshUploadTask(std::list<xdisk::XFileTask>)));
    connect(this->xfm_, SIGNAL(RefreshDownloadTask(std::list<xdisk::XFileTask>)), task_gui, SLOT(RefreshDownloadTask(std::list<xdisk::XFileTask>)));
    connect(this->xfm_, SIGNAL(RefreshCompleteTask(std::list<xdisk::XFileTask>)), task_gui, SLOT(RefreshCompleteTask(std::list<xdisk::XFileTask>)));

    qRegisterMetaType<xdisk::XDiskInfo>("xdisk::XDiskInfo");
    connect(this->xfm_, SIGNAL(RefreshDiskInfo(xdisk::XDiskInfo)), this, SLOT(RefreshDiskInfo(xdisk::XDiskInfo)));
    
    connect(this->xfm_, SIGNAL(ErrorSig(std::string)), this, SLOT(ErrorSlot(std::string)));
    connect(this->xfm_, SIGNAL(InfoSig(std::string)),  this, SLOT(InfoSlot(std::string)));

    qRegisterMetaType<std::vector<xdisk::XSharedFolderInfo>>("std::vector<xdisk::XSharedFolderInfo>");

    //FileManager::Get()->GetDir("/");

    Refresh();

    // 显示用户名
    ui.username_label->setText(xfm_->login().username().c_str());
    
    //ui.username_label->set_text(xfm_->login()->username().c_str());
    
    //TaskTab();
    return;

}

void XMSDiskClientGui::RefreshDiskInfo(xdisk::XDiskInfo info)
{
    
    string size_str = XGetSizeString(info.dir_size());
    size_str += "/";
    size_str += XGetSizeString(info.total());
    ui.disk_info_text->setText(size_str.c_str());
    ui.disk_info_bar->setMaximum(info.total());
    ui.disk_info_bar->setValue(info.dir_size());
}

// 文件加密开关
void XMSDiskClientGui::FileEnc()
{
    if (ui.file_enc->isChecked())
    {
        FilePassword pass_dia;
        if (pass_dia.exec() == QDialog::Accepted)
        {
            this->xfm_->set_password(pass_dia.password);
        }
    }
    else
    {
        this->xfm_->set_password("");
    }
}

void XMSDiskClientGui::MyTab()
{
    if (!task_gui) return;
    ui.filelistwidget->show();
    task_gui->hide();
}

void XMSDiskClientGui::TaskTab()
{
    task_gui->move(ui.filelistwidget->pos().x(), ui.filelistwidget->pos().y());
    task_gui->resize(size());
    //task_gui->
    ui.filelistwidget->hide();

    task_gui->Show();
}

void XMSDiskClientGui::DoubleClicked(int row, int col)
{
    // 双击进入目录或下载文件
    auto item = ui.filetableWidget->item(row, 1);
    QString dir = item->text();
    string filename(dir.toUtf8().constData());

    // 检查是否为目录
    for (auto file : file_list)
    {
        if (file.filename() == filename)
        {
            if (file.is_dir())
            {
                // Build next path without duplicate slashes.
                string next = remote_dir_;
                while (!next.empty() && next.back() == '/') next.pop_back();
                next += "/" + filename;
                this->xfm_->GetDir(next);
            }
            else
            {
                QString localpath = QFileDialog::getExistingDirectory(this, QString::fromUtf8("请选择保存路径"));
                if (localpath.isEmpty())
                    return;

                XFileInfo task;
                QString filename = item->text();
                task.set_filename(filename.toUtf8().constData());
                task.set_filedir(remote_dir_);
                QString rawpath = QDir(localpath).filePath(filename);
                QString filepath = QDir::toNativeSeparators(rawpath);
                task.set_local_path(filepath.toUtf8().constData());

                // 检查是否为加密文件，弹窗输入密码
                if (file.is_enc())
                {
                    FilePassword pass_dia;
                    pass_dia.setWindowTitle("Download encrypted file");
                    if (pass_dia.exec() == QDialog::Accepted)
                    {
                        this->xfm_->set_password(pass_dia.password);
                    }
                    else
                    {
                        return; // 用户取消输入密码，取消下载
                    }
                }

                xfm_->DownloadFile(task);
                
                TaskTab();
            }
            break;
        }
    }
    qDebug() << item;
}

void XMSDiskClientGui::NewDir()
{
    QDialog dialog;

    // 去掉原窗口边框
    dialog.setWindowFlags(Qt::FramelessWindowHint);

    // 设置背景透明，实现圆角
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.resize(400, 50);
    QLineEdit edit(&dialog);
    edit.resize(300, 40);
    QPushButton ok(&dialog);
    ok.move(305, 0);
    ok.setText(QString::fromUtf8("确定"));
    QPushButton cancel(&dialog);
    cancel.move(305, 22);
    cancel.setText(QString::fromUtf8("取消"));
    connect(&cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));
    connect(&ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
    auto re = dialog.exec();
    if (re == QDialog::Rejected)
    {
        return;
    }
    if (edit.text().isEmpty()) return;

    string dir = edit.text().toUtf8().constData(); // 输入新建目录名称
    xfm_->NewDir(remote_dir_+"/"+dir);
}

void XMSDiskClientGui::Root()
{
    xfm_->GetDir("/");
}

// 目录后退
void XMSDiskClientGui::Back()
{
    if (remote_dir_.empty() || remote_dir_ == "/")
        return;
    string tmp = remote_dir_;
    // Strip trailing slash
    while (!tmp.empty() && tmp.back() == '/')
        tmp.pop_back();
    auto pos = tmp.find_last_of('/');
    if (pos == string::npos)
        remote_dir_ = "/";   // no slash found → already at top level
    else
        remote_dir_ = tmp.substr(0, pos);
    if (remote_dir_.empty())
        remote_dir_ = "/";
    xfm_->GetDir(remote_dir_);
}

void XMSDiskClientGui::Delete()
{
    auto tab = ui.filetableWidget;

    // 检查是否有选中项
    bool has_checked = false;
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
        if (check && check->isChecked()) { has_checked = true; break; }
    }
    if (!has_checked)
    {
        QMessageBox::information(this, "", QString::fromUtf8("未选择删除文件"));
        return;
    }

    auto re = QMessageBox::information(this, "", QString::fromUtf8("确认删除文件吗"), QMessageBox::Ok | QMessageBox::Cancel);
    if (re == QMessageBox::Cancel)
        return;

    // Count how many files are selected before issuing any deletes, so
    // the directory is only refreshed once after the last response arrives.
    int delete_count = 0;
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
        if (check && check->isChecked()) delete_count++;
    }
    if (delete_count == 0) return;
    this->xfm_->BeginBatchDelete(delete_count);

    // 遍历所有选中项逐个删除
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
        if (!check || !check->isChecked()) continue;

        auto item = tab->item(i, 1);
        if (!item) continue;

        string filename(item->text().toUtf8().constData());

        // 检查该文件是否为目录
        bool is_dir = false;
        for (auto &f : ::file_list)
        {
            if (f.filename() == filename)
            {
                is_dir = f.is_dir();
                break;
            }
        }

        XFileInfo file;
        file.set_filename(filename);
        file.set_filedir(remote_dir_);
        file.set_is_dir(is_dir);
        this->xfm_->DeleteFile(file);
    }
}

void XMSDiskClientGui::Download()
{
   // int row = ui.filetableWidget->currentRow();

    auto tab = ui.filetableWidget;
    int row = -1;
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
        //auto check = (QCheckBox*)tab->cellWidget(i, 0);
        if (!check) continue;
        if (check->isChecked())
        {
            row = i;
            break;
        }
    }
    
    if (row < 0)
    {
        QMessageBox::information(this, "", QString::fromUtf8("未选择下载文件"));
        return;
    }
    // 获取选择的文件名
    QString filename = ui.filetableWidget->item(row, 1)->text();
    // 获取保存路径
    QString localpath = QFileDialog::getExistingDirectory(this, QString::fromUtf8("请选择保存路径"));
    if (localpath.isEmpty())
        return;
    XFileInfo task;
    task.set_filename(filename.toUtf8().constData());
    task.set_filedir(remote_dir_);
    QString rawpath = QDir(localpath).filePath(filename);
    QString filepath = QDir::toNativeSeparators(rawpath);
    task.set_local_path(filepath.toUtf8().constData());

    // 检查是否为加密文件，弹窗输入密码
    for (auto &f : ::file_list)
    {
        if (f.filename() == filename && f.is_enc())
        {
            FilePassword pass_dia;
            pass_dia.setWindowTitle("Download encrypted file");
            if (pass_dia.exec() == QDialog::Accepted)
            {
                this->xfm_->set_password(pass_dia.password);
            }
            else
            {
                return; // 用户取消输入密码，取消下载
            }
            break;
        }
    }

    xfm_->DownloadFile(task);
}

void XMSDiskClientGui::Checkall()
{
    static int count = 0;
    count++;
    qDebug() << count << "Checkall()" << ui.checkallBox->isChecked();
    auto tab = ui.filetableWidget;
    //for (auto check : check_list)
    //{
    //    check->setChecked(true);
    //}
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
       //auto check = (QCheckBox*)tab->cellWidget(i, 0);
        if (!check) continue;
        check->setChecked(ui.checkallBox->isChecked());
    }
    
}

void XMSDiskClientGui::SelectFile(QModelIndex index)
{
    auto tab = ui.filetableWidget;
    for (int i = 0; i < tab->rowCount(); i++)
    {
        auto w = tab->cellWidget(i, 0);
        if (!w) continue;
        auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
        if (!check) continue;
        check->setChecked(false);
    }
    auto w = tab->cellWidget(index.row(), 0);
    if (!w) return;
    auto check = (QCheckBox*)w->layout()->itemAt(0)->widget();
    //auto check = (QCheckBox*)tab->cellWidget(i, 0);
    if (!check) return;
    check->setChecked(true);
}

void XMSDiskClientGui::RefreshData(xdisk::XFileInfoList file_list, std::string dir)
{
    remote_dir_ = dir;
    QString view_dir = "";
    QString dir_str = QString::fromUtf8(dir.c_str());
    auto dir_list = dir_str.split("/");
    for (auto d : dir_list)
    {
        auto dir = d.trimmed();
        if (dir.isEmpty())
        {
            continue;
        }
        view_dir += dir;
        view_dir += "> ";
    }
    ui.dir_label->setText(view_dir);

    // 清空并更新全局文件列表
    ::file_list.clear();
    for (auto file : file_list.files())
    {
        ::file_list.push_back(file);
    }


    auto tab = ui.filetableWidget;
    while (tab->rowCount() > 0) tab->removeRow(0);
    check_list.clear();
    for (auto file : file_list.files())
    {
        // 文件名
        string filename = file.filename();
        if (filename.empty()) continue;
        

        // 文件扩展名
        //string filetype = "";
        //int pos = filename.find_last_of('.');
        //if (pos > 0)
        //{
        //    filetype = filename.substr(pos + 1);
        //}
        // 转换为小写，用于匹配图标
        //transform(filetype.begin(), filetype.end(), filetype.begin(), ::tolower);


        // 文件对应图标
        string iconpath = FILE_ICON_PATH;
        //map<string, string> icons;
        //icons["jpg"] = "Img";
        //icons["png"] = "Img";
        //icons["gif"] = "Img";

        iconpath += XGetIconFilename(filename, file.is_dir());
        iconpath += "Type.png";
        // 插入表格
        int newRow = tab->rowCount();
        tab->insertRow(newRow);

        // 添加一个选择框，居中显示
        QCheckBox *ckb = new QCheckBox(tab);
        check_list.push_back(ckb);
        auto hLayout = new QHBoxLayout();
        auto widget = new QWidget(tab);
        hLayout->addWidget(ckb);
        hLayout->setContentsMargins(0, 0, 0, 0);  // 设置边距，让CheckBox居中显示
        hLayout->setAlignment(ckb, Qt::AlignCenter);
        widget->setLayout(hLayout);
        tab->setCellWidget(newRow, 0, widget);

        // 设置文件图标
        QString qfilename;
        qfilename = QString::fromUtf8(filename.c_str());
        tab->setItem(newRow, 1, new QTableWidgetItem(QIcon(iconpath.c_str()), qfilename));
        
        // 文件时间
        tab->setItem(newRow, 2, new QTableWidgetItem(file.filetime().c_str()));

        // 文件大小 B KB MB GB
        string filesize_str = "";
        if (!file.is_dir())
        {
            filesize_str = XGetSizeString(file.filesize());
        }
        
        tab->setItem(newRow, 3, new QTableWidgetItem(filesize_str.c_str()));
    }

    // 文件数量
    stringstream ss;
    ss << tab->rowCount();
    ui.file_count->setText(ss.str().c_str());
}

void XMSDiskClientGui::Upload()
{
    // 用户选择一个文件
    QString filepath = QFileDialog::getOpenFileName(this, QString::fromUtf8("请选择上传文件"));
    if (filepath.isEmpty())
        return;
    qDebug() << "filepath:" << filepath;

    QFileInfo fileinfo;
    fileinfo = QFileInfo(filepath);
    qDebug() << fileinfo.filePath();
    qDebug() << fileinfo.fileName();
    qDebug() << fileinfo.canonicalFilePath();
    qDebug() << fileinfo.absoluteFilePath();
    string file_real_path(filepath.toUtf8().constData());
    string filename(fileinfo.fileName().toUtf8().constData());/*
    string filedir = file_real_path.substr(0, file_real_path.size() - filename.size());*/
    XFileInfo task;
    task.set_filename(filename);
    task.set_filedir(remote_dir_);
    task.set_local_path(file_real_path);
    xfm_->UploadFile(task);
}

void XMSDiskClientGui::Refresh()
{
    // 如果远程目录为空，默认使用根目录 "/"
    std::string dir = remote_dir_.empty() ? "/" : remote_dir_;
    xfm_->GetDir(dir);
}

void XMSDiskClientGui::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu Context;
    Context.addAction(ui.action_new_dir);
    Context.addAction(ui.upaction);
    Context.addAction(ui.downaction);
    Context.addAction(ui.refreshaction);
    QAction share_action(QString::fromUtf8("共享文件夹"), &Context);
    Context.addSeparator();
    Context.addAction(&share_action);
    connect(&share_action, &QAction::triggered, this, &XMSDiskClientGui::ShowSharePanel);
    Context.exec(QCursor::pos());
}

void XMSDiskClientGui::ShowSharePanel()
{
    ShareDialog dlg(xfm_, this);
    dlg.exec();
}

static bool mouse_press = false;
static QPoint mouse_point;

void XMSDiskClientGui::mouseMoveEvent(QMouseEvent *ev)
{
    // 没有按下，处理原事件
    if (!mouse_press)
    {
        QWidget::mouseMoveEvent(ev);
        return;
    }
    auto cur_pos = ev->globalPos();
    this->move(cur_pos - mouse_point);
}

void XMSDiskClientGui::mousePressEvent(QMouseEvent *ev)
{
    // 鼠标左键按下记录位置
    if (ev->button() == Qt::LeftButton)
    {
        mouse_press = true;
        mouse_point = ev->pos();
    }
}

void XMSDiskClientGui::mouseReleaseEvent(QMouseEvent *ev)
{
    mouse_press = false;
}
