/**
 * @file main.cpp
 * @brief 配置管理GUI主程序入口
 * 
 * 启动Qt应用程序，显示配置管理窗口
 */
#include "config_gui.h"
#include "xconfig_client.h"
#include "config_edit.h"
#include <QtWidgets/QApplication>

/**
 * @brief 配置管理GUI主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ConfigGui w;
    w.show();
    return a.exec();
}
