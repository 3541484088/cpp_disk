#pragma once

#include <QtWidgets/QWidget>
#include "ui_xmsdiskclientgui.h"
#include <list>
//#include "file_manager.h"
#include "xfile_manager.h"
#include "xai_panel.h"

class XMSDiskClientGui : public QWidget
{
    Q_OBJECT

public:
    XMSDiskClientGui(XFileManager *xfm,QWidget *parent = Q_NULLPTR);
    ~XMSDiskClientGui();
    void mouseMoveEvent(QMouseEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void contextMenuEvent(QContextMenuEvent *event);
public slots:
    void Refresh();
    void Checkall();
    void Upload();
    void Download();
    void Delete();
    //目录操作
    void Back();
    void Root();
    void NewDir();
    void RefreshData(xdisk::XFileInfoList file_list, std::string);
    void RefreshDiskInfo(xdisk::XDiskInfo info);
    void DoubleClicked(int row, int col);

    void SelectFile(QModelIndex index);
    void TaskTab();
    void MyTab();
    void ErrorSlot(std::string err);
    void InfoSlot(std::string msg);
    void FileEnc();
    void ShowSharePanel();
    void ChangePassword();
    void set_xfm(XFileManager *fm) { xfm_ = fm; }

    // AI 面板相关
    void HighlightFiles(QStringList file_names);
    void AINavigateDir(QString dir_path);
    void ToggleAIPanel();

private:
    Ui::XMSDiskClientGuiClass ui;
    std::string remote_dir_ = "";//远程路径
    XFileManager *xfm_ = 0;
    XAIPanel     *ai_panel_ = nullptr;
};
