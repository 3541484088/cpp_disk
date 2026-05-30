#pragma once

#include <QWidget>
#include "ui_taskitemgui.h"
#include "xms_disk_client_gui.pb.h"
class TaskItemGUI : public QWidget
{
    Q_OBJECT

public:
    TaskItemGUI(QWidget *parent = Q_NULLPTR);
    ~TaskItemGUI();
    void SetTask(xdisk::XFileTask task);
private:
    xdisk::XFileTask task_;
    Ui::TaskItemGUI ui;
};
