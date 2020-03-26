#include <iostream>
#include<sys/types.h>
#include<sys/stat.h>
#include<signal.h>
#include<unistd.h>
#include<stdlib.h>
#include "daemon.h"

void Daemon()                 //只fork()一次
{
    umask(0);                   //用umask将文件模式创建为屏蔽字为0
    if(fork() == 0)             //fork子进程，关闭父进程
    {}
    else
    {
        exit(0);
    }
    setsid();                   //让子进程成为一个守护进程
    chdir("/");                 //更改当前目录为根目录
    close(0);                   //关闭默认的文件描述符表
    close(1);
    close(2);
    signal(SIGCHLD, SIG_DFL);   //忽略SIGCHLD信号
}