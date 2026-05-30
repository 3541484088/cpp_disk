/**
 * @file xms_add_user.cpp
 * @brief 用户添加工具主程序
 * 
 * 命令行工具，用于向认证服务器添加新用户
 */
#include <iostream>
#include <string>
#include "xauth_client.h"
#include <thread>
#include <chrono>
using namespace std;

/**
 * @brief 用户添加工具主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码
 * 
 * 交互式输入用户名、角色名和密码，向认证服务器发送添加用户请求
 */
int main(int argc, char *argv[])
{
    string username = "";
    string rolename = "";
    string password = "";
    
    // 交互式输入用户信息
    cout << "Username:";
    cin >> username;
    cout << "Rolename:";
    cin >> rolename;
    cout << "Password:";
    cin >> password;
    cout << username << "/" << password << endl;
    
    // 初始化认证客户端
    XAuthClient::RegMsgCallback();
    XAuthClient::Get()->set_server_ip("127.0.0.1");
    XAuthClient::Get()->set_server_port(AUTH_PORT);
    XAuthClient::Get()->StartConnect();
    
    // 等待连接建立
    while (!XAuthClient::Get()->is_connected())
    {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    // 构造添加用户请求
    xmsg::XAddUserReq adduser;
    adduser.set_username(username);
    adduser.set_password(password);
    adduser.set_rolename(rolename);
    
    // 发送请求
    XAuthClient::Get()->AddUserReq(&adduser);
    this_thread::sleep_for(chrono::milliseconds(500));
    return 0;
}
