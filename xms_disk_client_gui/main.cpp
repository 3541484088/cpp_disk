#include "xmsdiskclientgui.h"
#include <QtWidgets/QApplication>
//#include "file_manager.h"
#include "xget_dir_client.h"
#include "xupload_proxy.h"
#include "xms_file_manager.h"
#include "xauth_client.h"
#include "logingui.h"
#include "xtools.h"

int main(int argc, char *argv[])
{

    XMSFileManager xsm_fm;
    QApplication a(argc, argv);
    /*
    FileManager::Get()->StartThread();*/
    ////设置服务器地址 
    //XGetDirClient::Get()->set_server_ip("127.0.0.1");
    //XGetDirClient::Get()->set_server_port(API_GATEWAY_PORT);
    ////XGetDirClient::Get()->set_timer_ms(100);
    //XGetDirClient::RegMsgCallback();

    //XGetDirClient::Get()->StartConnect();
    string gateway_ip = XGetHostByName(API_GATEWAY_SERVER_NAME);

    XAuthClient::Get()->set_server_ip(gateway_ip.c_str());
    XAuthClient::Get()->set_server_port(API_GATEWAY_PORT);
    XAuthClient::Get()->StartConnect();

    //XUploadProxy::Get()->Start();
    LoginGUI login_gui;
    if (login_gui.exec() == QDialog::Rejected)
    {
        return -1;
    }
    xsm_fm.InitFileManager(gateway_ip, API_GATEWAY_PORT);
    //auto res = XAuthClient::Get()->GetLogin();
    //xsm_fm.set_login(XAuthClient::Get()->GetLogin());
    xsm_fm.set_login(XAuthClient::Get()->GetLogin());
    XMSDiskClientGui w(&xsm_fm);
    w.show();
    return a.exec();
}

