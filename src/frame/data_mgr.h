#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "datahandle.h"

class Data_Mgr
{
private:
    std::map<int, DataHandle*> _fd_map;
public:
    Data_Mgr(/* args */);
    ~Data_Mgr();
    int AddNewFd(int fd);
    bool DelFd(int fd);
    bool SetEvent(int fd,int event);
    bool DelEvent(int fd,int event);
    DataHandle* GetHandle(int fd);
    bool HaveFd(int fd);
    bool NeedWrite(int fd);
    int GetEvent(int fd);
};
