#pragma once

#include <QWidget>
#include "ui_task_list_gui.h"
#include "xms_disk_client_gui.pb.h"
#include <list>
class TaskListGUI : public QWidget
{
    Q_OBJECT

public:
    TaskListGUI(QWidget *parent = Q_NULLPTR);
    ~TaskListGUI();
    void Show();
    void Hide();
public slots:
    void Select(QModelIndex index);
    void RefreshUploadTask(std::list<xdisk::XFileTask> file_list);
    void RefreshDownloadTask(std::list<xdisk::XFileTask> file_list);
    void RefreshCompleteTask(std::list<xdisk::XFileTask> file_list);
    void RefreshTask(std::list<xdisk::XFileTask> file_list);
    void OkTask();
    void UpTask();
    void DownTask();
private:
    Ui::TaskListGUI ui;
    std::list<xdisk::XFileTask> upload_list;
    std::list<xdisk::XFileTask> download_list;
    std::list<xdisk::XFileTask> ok_list;
};
