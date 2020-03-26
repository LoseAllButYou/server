#pragma once

#include <iostream>
#include "thread_pool/thread_pool.h"
#include <map>
#include <string>
#include <vector>

#define G_CONSLOE Consloe::GetInstance()

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

class Consloe : public Task
{
public:
    Consloe();
    ~Consloe();
    bool GetSvrInfo(std::string conf_path= "");
    int AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr);
    bool DelSvrData(int id);
    bool UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr );
    SvrData* GetSvrData(int id);
    int LinkConfSvr(std::string  key,std::string conf_path= "");
    int Start(std::string conf_path= "");
    void Cmd();
    static Consloe* GetInstance();
    virtual void Run(void* args);
    virtual void Debug();
    virtual void Clear(){}
private:
    bool                        _chat_res;
    int                         _ID;
    int                         _type;
    int                         _mgr_svr_id;
    std::string                 _version;
    std::vector<int>                _link_svr;
    std::map<int ,SvrData*>     _svr_map;
    static Consloe*   g_self;
};