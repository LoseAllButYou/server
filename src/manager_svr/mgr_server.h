#pragma once

#include <iostream>
#include "thread_pool/thread_pool.h"
#include <map>
#include <string>
#include <vector>
#include "network.h"

#define G_MGR_SVR Mgr_Svr::GetInstance()

class SvrData
{
public:
    SvrData(int id,int type,int fd,std::string version,std::vector<int> link_svr);
    ~SvrData();
    int                 _ID;
    int                 _type;
    int                 _fd;
    std::string         _version;
    std::vector<int>    _link_svr;
};

class Mgr_Svr : public Task
{
public:
    Mgr_Svr();
    ~Mgr_Svr();
    bool GetSvrInfo(std::string conf_path = "");
    int AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr);
    bool DelSvrData(int id);
    bool UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr );
    void DealCmd(int svr_id,int cmd,DataHandle* pHandle, const std::string & context);
    SvrData* GetSvrData(int id);
    virtual void Run(void* args);
    virtual void Clear(){}
    virtual void Debug();
    static Mgr_Svr* GetInstance();

private:
    int                          _ID;
    int                          _type;
    std::string                  _version;
    std::vector<int>             _link_svr;
    std::map<int ,SvrData*>     _svr_map;
    static Mgr_Svr*              g_self;
};