#pragma once
#include <iostream>
#include <string>
#include "thread_pool/task.h"
#include "thread_pool/thread_pool.h"
#include "datahandle.h"

#define G_NETWORK Network::GetInstance()

void * NetworkThreadFunc(void* args);

class Network :public Task
{
private:
    /* data */
public:
    Network();
    ~Network();
    virtual void Run(void* args);
    virtual void Clear(){}
    virtual void Debug();
    static Network* GetInstance();
    int SetNonBlock(int fd);
    void SetListenHost(std::string ip,short port);
    int Listen();
    int Connect(std::string ip,short port);
    int EpollUp();
    int StartUp();
    bool AddSocketFd();
    void DelSocketFd(int fd);
    void EpollLoop();
    void ClearDataHandle(int fd);
public:
    int AddFd(int fd);
    bool DelFd(int fd);
    bool SetEvent(int fd,int event);
    bool DelEvent(int fd,int event);
    DataHandle* GetHandle(int fd);
    bool HaveFd(int fd);
    bool NeedWrite(int fd);
    int GetEvent(int fd);
public:
    static Network*   g_self;
    bool              epoll_close;  
private:
    int               epoll_fd;
    int               listen_fd;
    std::string       _ip;
    short             _port;
    std::map<int, DataHandle*> _fd_map;
};
