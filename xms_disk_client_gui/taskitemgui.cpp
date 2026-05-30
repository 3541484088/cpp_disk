#include "taskitemgui.h"
#include "xtools.h"
#include <string>
using namespace std;
#define FILE_MID_ICON_PATH ":/XMSDiskClientGui/Resources/img/FileType/Middle/"
TaskItemGUI::TaskItemGUI(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    
    //setWindowFlags(Qt::FramelessWindowHint);

    //setAttribute(Qt::WA_TranslucentBackground);
    //ui.filename->setAttribute(Qt::WA_TranslucentBackground);
    //ui.filesize->setAttribute(Qt::WA_TranslucentBackground);
    ////ui.filetype->setAttribute(Qt::WA_TranslucentBackground);
    //ui.filetime->setAttribute(Qt::WA_TranslucentBackground);
    
}
void TaskItemGUI::SetTask(xdisk::XFileTask task)
{
    auto file = task.file();

    
    ui.filetime->setText(task.tasktime().c_str());

    
    QString filename = QString::fromLocal8Bit(task.file().filename().c_str());
    ui.filename->setText(filename);

    
    stringstream ss;
    if (file.filesize() == 0)
    {
        ss << "0B" << endl;
        
    }
    else
    {
        ss << XGetSizeString(file.net_size()) << "/" << XGetSizeString(file.filesize());
    }
        

    ui.filesize->setText(ss.str().c_str());

    
    ui.progressBar->setMinimum(0);
    long long filesize = file.filesize();
    long long netsize = file.net_size();
    if (filesize > 1024 * 1024 * 10)
    {
        filesize /= 1000;
        netsize /= 1000;
    }
    ui.progressBar->setMaximum(filesize);
    ui.progressBar->setValue(netsize);
    
    
    string iconpath = FILE_MID_ICON_PATH;
    iconpath += XGetIconFilename(task.file().filename(), file.is_dir());
    iconpath += "Type.png";
    QString sty = "background-color: rgba(0, 0, 0,0);\n";
    sty += "background-image: url(";
    sty += iconpath.c_str();
    sty += ");";
    ui.filetype->setStyleSheet(sty);


}

TaskItemGUI::~TaskItemGUI()
{
}
