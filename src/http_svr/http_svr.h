#pragma once

#include <iostream>
#include "thread_pool/thread_pool.h"
#include <map>
#include <string>
#include <vector>

#include "room.h"
#include "timer.h"
#include "datahandle.h"
#include "poker.pb.h"

#define G_HTTP HttpSvr::GetInstance()

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

class HttpSvr : public Task
{
public:
    HttpSvr();
    ~HttpSvr();
    bool GetSvrInfo(std::string conf_path= "");
    int AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr);
    bool DelSvrData(int id);
    bool UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr );
    SvrData* GetSvrData(int id);
    int LinkConfSvr(std::string  key,std::string conf_path= "");
    int Start(std::string conf_path= "");
    static HttpSvr* GetInstance();
    void OnTimerOver(TimerOver* timer_over);
    void OnSvrRequest(DataHandle * phandle);
    bool IsSvrSocket(int fd);
    virtual void Run(void* args);
    virtual void Clear(){}
    virtual void Debug();
//业务相关：
public:
    void OnClientRequest(DataHandle * phandle);
    void OnWebSocket(DataHandle * phandle);
    void HandleRequest(DataHandle * phandle);
    int LoadResoures(std::string& path,std::string & out);
    std::string GetSrcType(std::string& type);
public:
    //room 
    int OnCreatRoom(PokerGame::msg_package & msg_package,DataHandle* handle);
    int OnJoinRoom(PokerGame::msg_package & msg_package,DataHandle* handle);
    bool HaveRoom(int room_id){return _room_map.find(room_id)!=_room_map.end();}
    int RandRoomID();
private:
    int GetTypeConfig(std::string conf_path= "");
private:
    const char*                             _root_path;  
    const char*                             _url;  
    const char*                             _main_page;  
    bool                                    _chat_res;
    int                                     _ID;
    int                                     _type;
    int                                     _mgr_svr_id;
    std::string                             _version;
    std::string                             _main_data;
    std::vector<int>                        _link_svr;
    std::map<int ,SvrData*>                 _svr_map;
    std::map<std::string,std::string>       _src_map;
    std::map<std::string ,std::string >     _header_map;
    std::map<std::string ,std::string >     _type_map;
    std::map<int ,Room* >                   _room_map;
    static HttpSvr*                         g_self;
};