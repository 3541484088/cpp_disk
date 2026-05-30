#include "task_list_gui.h"
#include "taskitemgui.h"
#include <QDebug>
#include <map>
#include <set>
#include <string>
#include <sstream>
using namespace std;
static map<int, TaskItemGUI*> task_items;
void TaskListGUI::RefreshUploadTask(std::list<xdisk::XFileTask> file_list)
{
    stringstream ss;
    ss << "("<< file_list.size() << ")";
    ui.uplabel->setText(ss.str().c_str());

    upload_list = file_list;
    if (ui.upButton->isChecked())
        RefreshTask(file_list);
}

void TaskListGUI::RefreshDownloadTask(std::list<xdisk::XFileTask> file_list)
{
    stringstream ss;
    ss << "(" << file_list.size() << ")";
    ui.downlabel->setText(ss.str().c_str());

    download_list = file_list;
    if (ui.downButton->isChecked())
        RefreshTask(file_list);
}

void TaskListGUI::RefreshCompleteTask(std::list<xdisk::XFileTask> file_list)
{
    stringstream ss;
    ss << "(" << file_list.size() << ")";
    ui.oklabel->setText(ss.str().c_str());

    ok_list = file_list;
    if (ui.okButton->isChecked())
        RefreshTask(file_list);
}

void TaskListGUI::RefreshTask(std::list<xdisk::XFileTask> file_list)
{
    auto tab = ui.taskableWidget;
    
    // 创建当前任务ID集合
    set<int> current_task_ids;
    for(auto task: file_list)
    {
        current_task_ids.insert(task.index());
    }
    
    // 移除不再存在的任务
    for (auto it = task_items.begin(); it != task_items.end(); )
    {
        if (current_task_ids.find(it->first) == current_task_ids.end())
        {
            // 找到需要删除的行
            for (int i = 0; i < tab->rowCount(); i++)
            {
                QWidget* widget = tab->cellWidget(i, 0);
                if (widget == it->second)
                {
                    tab->removeRow(i);
                    break;
                }
            }
            delete it->second;
            it = task_items.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    // 添加或更新当前任务
    for(auto task: file_list)
    {
        if (task_items.find(task.index()) == task_items.end())
        {
            tab->insertRow(0);
            auto item = new TaskItemGUI();
            item->SetTask(task);
            tab->setCellWidget(0, 0, item);
            tab->setRowHeight(0, 51);
            task_items[task.index()] = item;
        }
        else
        {
            task_items[task.index()]->SetTask(task);
        }
    }
}


TaskListGUI::TaskListGUI(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    auto tab = ui.taskableWidget;
    while (tab->rowCount() > 0)
    {
        tab->removeRow(0);
    }
    tab->setIconSize(QSize(30, 30));
    tab->setColumnWidth(0, 100);     //checkall
    tab->setColumnWidth(1, 500);    //filename

    tab->setSelectionBehavior(QAbstractItemView::SelectRows);//����ѡ��ģʽΪѡ����
    tab->setSelectionMode(QAbstractItemView::SingleSelection);//����ѡ�е���
    // 强制显示垂直滚动条
    tab->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui.uplabel->setText("");
    ui.downlabel->setText("");
    ui.oklabel->setText("");
    

}

void TaskListGUI::OkTask()
{
    task_items.clear();
    auto tab = ui.taskableWidget;
    while (tab->rowCount() > 0)
    {
        tab->removeRow(0);
    }
    RefreshTask(ok_list);
}
void TaskListGUI::UpTask()
{
    auto tab = ui.taskableWidget;
    task_items.clear();
    while (tab->rowCount() > 0)
    {
        tab->removeRow(0);
    }
    RefreshTask(upload_list);
}

void TaskListGUI::DownTask()
{
    task_items.clear();
    auto tab = ui.taskableWidget;
    while (tab->rowCount() > 0)
    {
        tab->removeRow(0);
    }
    RefreshTask(download_list);
}

void TaskListGUI::Select(QModelIndex index)
{

}

TaskListGUI::~TaskListGUI()
{
}

void TaskListGUI::Hide()
{
    this->hide();
}
void TaskListGUI::Show()
{
    this->show();
    QWidget *p = (QWidget *)this->parent();
    int w = p->width();
    auto tab_pos = ui.taskableWidget->pos();
    auto size = ui.taskableWidget->size();
    
    size.setHeight(p->height() - pos().y());
    size.setWidth(w- tab_pos.x());
    ui.taskableWidget->resize(size);
}