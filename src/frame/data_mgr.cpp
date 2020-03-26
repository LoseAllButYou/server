#include "data_mgr.h"
#include "thread_pool/task.h"
#include "thread_pool/thread_pool.h"
#include "loger.h"
#include <map>

Data_Mgr::Data_Mgr(/* args */)
{
}

Data_Mgr::~Data_Mgr()
{
    std::map<int,DataHandle*>::iterator it;
    for (it=_fd_map.begin();it!=_fd_map.end();)
    {
        _fd_map.erase(it);
    }
}

DataHandle* Data_Mgr::GetHandle(int fd)
{
    if(_fd_map.find(fd)!=_fd_map.end())
    {
        return _fd_map[fd];
    }
    return NULL;
}


int Data_Mgr::AddNewFd(int fd)
{
    if(_fd_map.find(fd)!=_fd_map.end())
    {
        _fd_map[fd]->Clear();
        //或者处理业务逻辑
    }
    _fd_map[fd]= new DataHandle(fd);
    if(_fd_map[fd]==NULL)
    {
        return -1;
    }
    _fd_map[fd]->SetEvent(DATA_CONNECT);
    G_Thread_pool->PushTask(_fd_map[fd]);
    return 0;
}

bool Data_Mgr::DelFd(int fd)
{
    std::map<int,DataHandle*>::iterator it= _fd_map.find(fd);
    if (it!=_fd_map.end())
    {
        _fd_map.erase(it);
    }
    return true;
}

bool Data_Mgr::SetEvent(int fd,int event)
{
    if(_fd_map.find(fd)==_fd_map.end())
    {
        return false;
    }
    _fd_map[fd]->SetEvent(event);
    G_Thread_pool->PushTask(_fd_map[fd]);
    return true;
}

bool Data_Mgr::DelEvent(int fd,int event)
{
    if(_fd_map.find(fd)==_fd_map.end())
    {
        return false;
    }
    _fd_map[fd]->DelEvent(event);
    G_Thread_pool->PushTask(_fd_map[fd]);
    return true;
}

bool Data_Mgr::HaveFd(int fd)
{
    return _fd_map.find(fd)!=_fd_map.end();
}

bool Data_Mgr::NeedWrite(int fd)
{
    if(_fd_map.find(fd)==_fd_map.end())
    {
        return false;
    }
    return _fd_map[fd]->NeedWrite();
}

int Data_Mgr::GetEvent(int fd)
{
    if(_fd_map.find(fd)==_fd_map.end())
    {
        return -1;
    }
    return _fd_map[fd]->Event();
}

