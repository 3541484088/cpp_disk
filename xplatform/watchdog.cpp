/**
 * @file watchdog.cpp
 * @brief 进程看门狗程序
 * 
 * 监控指定进程的运行状态，当进程意外退出时自动重启
 */
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
using namespace std;

int main(int argc, char *argv[])
{
    // 默认进程退出后的重启等待时间（秒）
    int timeval = 3;
    
    // 检查命令行参数：watchdog 重启间隔(秒) 目标程序 [参数...]
    if (argc < 3)
    {
        cout << "Usage: ./watchdog <restart_interval> <program> [args...]" << endl;
        cout << "Example: ./watchdog 3 ./register_server 20011" << endl;
        return -1;
    }

    // 创建子进程，父进程退出，子进程变为守护进程
    int pid = fork();
    if (pid > 0) exit(0);  // 父进程退出，让1号进程接管子进程
    
    setsid();   // 创建新的会话，成为会话组长
    umask(0);   // 设置文件权限掩码为0

    // 获取重启等待时间参数
    timeval = atoi(argv[1]);

    // 拼接要执行的命令（从第2个参数开始）
    string cmd = "";
    for (int i = 2; i < argc; i++)
    {
        cmd += " ";
        cmd += argv[i];
    }
    cout << "Watchdog monitoring: " << cmd << endl;

    // 无限循环：启动进程并等待其退出
    for (;;)
    {
        // 启动目标进程，等待其退出
        int ret = system(cmd.c_str());
        cout << "Process exited, restarting in " << timeval << " seconds..." << endl;
        sleep(timeval);
    }

    return 0;
}