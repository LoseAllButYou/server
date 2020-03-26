#include <bits/stdc++.h>
#include <stdio.h>
#include <fstream>
#include <stdlib.h>

#include "http_svr.h"
#include "network.h"
#include "redis_cli.h"
#include "config.h"
#include "loger.h"
#include "mgrsvr_protocol.pb.h"
#include "define.h"
#include "string.h"
#include <stdlib.h>
#include <time.h> 

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_room_mutex = PTHREAD_MUTEX_INITIALIZER;
HttpSvr* HttpSvr:: g_self = NULL;

SvrData::SvrData(int id,int type,int fd,std::string version,std::vector<int> link_svr)
    :_ID(id),_type(type),_fd(fd),_version(version),_link_svr(link_svr)
 {
 }

SvrData::~SvrData()
{
    std::string().swap(_version);
    std::vector<int>().swap(_link_svr);
}

HttpSvr::HttpSvr()
    :Task(TASK_DEFAULT,"consloe server")
    ,_root_path("web/"),_main_page("index.html"),_url("http://192.168.0.111:8080/")
    ,_chat_res(false),_ID(-1),_type(-1),_mgr_svr_id(-1),_version("")
{
    pthread_mutex_init(&m_mutex,NULL);
    pthread_mutex_init(&m_room_mutex,NULL);
}

HttpSvr::~HttpSvr()
{
    std::string().swap(_version);
    pthread_mutex_destroy(&m_mutex);
    pthread_mutex_destroy(&m_room_mutex);
}

bool HttpSvr::GetSvrInfo(std::string conf_path)
{
    Config conf;

    if(conf_path!="")
    {
        conf.SetPath(conf_path);
    }
    conf.ReadConfValue();
    std::string tmp ="";
    conf.GetValue("LINK_SVR",tmp);
    if( 0>conf.GetValue("TYPE",_type)||
        conf.GetValue("ID",_ID)<0||
        conf.GetValue("VERSION",_version)<0||
        conf.GetValue("LINK_SVR",_link_svr)<0)
    {
        LOG_ERROR("server conf : %d %d %s %s",_ID,_type,_version.c_str(),tmp.c_str());
        return false;
    }
    
    LOG_DBG("server conf : %d %d %s %s",_ID,_type,_version.c_str(),tmp.c_str());
    //暂写死http headers
    {
        _header_map["Content-Language: "] = "en,zh";
        _header_map["Content-Type: "    ] = "text/html; charset=utf-8";
        _header_map["Server: "          ] = "PPH_http_sver 1.0";
        _header_map["Content-Length: "  ] = "";
        _header_map["Location: "        ] = "";
        _header_map["Accept-Encoding: " ] = "gzip, deflate";
    }

    std::string ip ;
    int port;
    conf.GetValue("IP",ip);
    conf.GetValue("PORT",port);
    LOG_DBG("server host : ip: %s port:%d",ip.c_str(),port);
    if(ip.length()>0 && port>0)
    {
        G_NETWORK-> SetListenHost(ip,port);
    }
    else
    {
        LOG_ERROR("server conf : ip: %s port:%d",ip.c_str(),port);
        return false;
    }
    GetTypeConfig("type.ini");
    return true;
}

int HttpSvr:: GetTypeConfig(std::string conf_path)
{
    Config conf;
    if(conf_path == "")
    {
        conf_path = "./type.ini";
    }

    conf.SetPath(conf_path);
    conf.ReadConfValue();
    conf.GetAll(_type_map);
    return 0;
}


int HttpSvr::LinkConfSvr(std::string key,std::string conf_path)
{
    Config conf;
    if(conf_path!="")
    {
        conf.SetPath(conf_path);
    }
    conf.ReadConfValue();
    std::vector<std::string> out_vec;
    conf.IncompleteMatch(key,out_vec);
    if(out_vec.size()==0)
    {
        LOG_DBG("don't have key : %d",key.c_str());
        return -1;
    }
    for(int i = 0;i< out_vec.size();++i)
    {
        int type = -1, id = -1,port = -1;
        std::string version("");
        std::vector<int> vec; 
        std::string ip;
        conf.GetValue(out_vec[i]+"_TYPE",type);
        conf.GetValue(out_vec[i]+"_ID",id);
        conf.GetValue(out_vec[i]+"_VERSION",version);
        conf.GetValue(out_vec[i]+"_LINK_SVR",vec);
        conf.GetValue(out_vec[i]+"_IP",ip);
        conf.GetValue(out_vec[i]+"_PORT",port);
        if(ip == ""||port == -1)
        {
            LOG_DBG("error ip or port! %s",out_vec[i].c_str());
            continue;
        }
        else
        {
            pthread_mutex_lock(&m_mutex);
            int fd = G_NETWORK->Connect(ip,port);
            if(fd>0)
            {
                if(0>AddSvrData(id,fd,type,version,vec))
                {
                    G_NETWORK->DelSocketFd(fd);
                    pthread_mutex_unlock(&m_mutex);
                    return -2;
                }
                else
                {
                    if(G_NETWORK->GetHandle(fd))
                    {
                        G_NETWORK->GetHandle(fd)->SetProtocolType(TCP);
                        pthread_mutex_unlock(&m_mutex);                         
                    }
                    else
                    {
                        G_NETWORK->DelSocketFd(fd);
                        pthread_mutex_unlock(&m_mutex);                         
                        return -3;
                    }
                    
                    if(type==mgr_svr::MGR_SVR)
                    {
                        _mgr_svr_id = id;
                    }
                    mgr_svr::PACKAGE package ;
                    mgr_svr::START_SVR start;
                    package.set_id(mgr_svr::START);
                    package.set_svr_id(_ID);
                    start.set_type(_type);
                    for(int i = 0;i<_link_svr.size();++i)
                    {
                        start.add_link_svr(_link_svr[i]);
                    }
                    start.set_version(_version);
                    std::string pkg,send_pkg;
                    start.SerializeToString(&pkg);
                    package.set_package(pkg);
                    package.SerializeToString(&send_pkg);
                    DataHandle * phandle = G_NETWORK->GetHandle(fd);
                    phandle->SendPackage(send_pkg);
                    G_TIMER->AddTimer("server_heart_timer",id,3000,this);
                }       
            }
            else
            {
                return -4;
            }
            
        }   
    }

    return 0;
}

int HttpSvr::Start(std::string conf_path)
{
    //return GetSvrInfo(conf_path)&&LinkConfSvr("manager","")==0;
    return GetSvrInfo(conf_path)&&0==G_NETWORK->EpollUp()&&0==G_NETWORK->Listen();
}


int HttpSvr::AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
{
    if(_svr_map.find(id)!=_svr_map.end())
    {
        LOG_ERROR("server aready exist! fd:%d,type:%d",id,type);
        return -1;
    }
    SvrData* pSvr = new SvrData (id,type,fd,version,link_svr);
    if(pSvr==NULL)
    {
        LOG_ERROR("new SvrData Err! id : %d" ,id);
        return -2;
    }
    _svr_map[id] = pSvr;
    return true;
}

bool HttpSvr::DelSvrData(int id)
{
    std::map<int,SvrData*>::iterator it= _svr_map.find(id);
    if(it != _svr_map.end() )
    {
        _svr_map.erase(it);
    }
    return true;
}

bool HttpSvr::UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
{
    if(_svr_map.find(id)==_svr_map.end())
    {
        LOG_ERROR("server doesn't exist! fd:%d,type:%d",id,type);
        return false;
    }
    SvrData* pSvr = _svr_map[id];
    if(id>0)
    {  
        pSvr->_ID = id;
    }
    if(type>0)
    {
        pSvr->_type = type;
    }
    if(link_svr.size()>0)
    { 
          pSvr->_link_svr = link_svr;
    }
    if(version.size()>0)
    {
        pSvr->_version =version;
    }
    if(fd>=0)
    {
        pSvr->_fd = fd;
    }
    return true;
}

SvrData* HttpSvr::GetSvrData(int id)
{
    if(_svr_map.find(id) == _svr_map.end())
    {
        return NULL;
    }
    return _svr_map[id];
}

void HttpSvr::Run(void* args)
{
    pthread_mutex_lock(&m_mutex);
    if(args == NULL)
    {
        LOG_ERROR("error args ");
        pthread_mutex_unlock(&m_mutex);
        return;
    }
    if(((Task*)args)->task_id == TASK_TIME_OVER)
    {
        OnTimerOver( (TimerOver*)args);
    }
    else if(((Task*)args)->task_id == TASK_REDIS_RES)
    {
        
    }
    else if(((Task*)args)->task_id == TASK_DATA_HANDLE)
    {
        DataHandle * phandle = (DataHandle * )args;
        LOG_DBG("----- %x %d " ,args,phandle->Event());

        if( phandle->Event()&DATA_CONNECT)
        {
            if(phandle->GetProtocolType() == INIT)
            {
                LOG_DBG("-------------");

                phandle->SetProtocolType(HTTP);
                OnClientRequest(phandle);
            }
        }
        else
        {
            if(phandle->_protocol_type == HTTP||phandle->_protocol_type == HTTPS)
            {
                OnClientRequest(phandle);
            }
            else if(phandle->_protocol_type == WEBSOCKET)
            {
                OnWebSocket(phandle);
            }
            else
            {
                LOG_DBG("-------------");
                OnSvrRequest(phandle);
            }
        }
        
    }
    pthread_mutex_unlock(&m_mutex);

}

void HttpSvr::OnTimerOver(TimerOver* timer_over)
{
    if(timer_over->timer_name == "server_heart_timer")
    {

        int svr_id =timer_over->timer_id ;
        SvrData* Svr = GetSvrData(svr_id);
        if(Svr==NULL)
        {
            LOG_ERROR("Svr data does not exsit! svr id:%d",svr_id);
            delete timer_over;
            return;
        }
        DataHandle* handle = G_NETWORK->GetHandle(Svr->_fd);
        if(handle == NULL)
        {
            LOG_ERROR("Svr handle does not exsit! svr id:%d",svr_id);
            delete timer_over;
            return;
        }
        mgr_svr::PACKAGE res_pkg ;
        res_pkg.set_id(mgr_svr::HEART);
        res_pkg.set_svr_id(_ID);;
        std::string res_str;
        res_pkg.SerializeToString(&res_str);
        handle->SendPackage(res_str);
        LOG_DBG(" leng :%d \n{%s}",res_str.length(), res_pkg.DebugString().c_str());
    }
    delete timer_over;
    timer_over=NULL;
}

void HttpSvr::OnSvrRequest(DataHandle * phandle)
{
    if(phandle->Event()&DATA_CLOSE )
    {
        //do something
        LOG_SHOW("socket deal ret fd:%d event:%d",phandle->GetFD(),phandle->Event());
        return;
    }
    int size = 0;
    const char* pData = (char * )phandle->GetReadData(size);
    std::string data = std::string(pData,0,size);
     if(pData == NULL)
    {
        LOG_ERROR("Get ReadData error fd: %d",phandle->GetFD());                      
        return ;
    }
    mgr_svr::PACKAGE package ;
    package.ParseFromString(data);
    LOG_DBG("{\n%s}", package.DebugString().c_str());
    mgr_svr::RESPONSE res;
    res.ParseFromString(package.package());

    //obj.SerializeToString(&str)
    switch (package.id())
    {
    
    case mgr_svr::HEART:
        {
            G_TIMER->AddTimer("server_heart_timer",package.svr_id(),3000,this);
        }
        break;
    case mgr_svr::RES_START:
    {
        LOG_DBG("start ret : %d ",res.error());
        break;
    }
    case mgr_svr::RES_STOP:
    {
        LOG_DBG("stop ret : %d ",res.error());
        break;
    }

    case mgr_svr::RES_UPDATE:
    {
        LOG_DBG("update ret : %d ",res.error());
        break;
    }
    default:
        LOG_ERROR("err package id:%d",package.id());
        break;
    }
    LOG_DBG("{\n%s}", res.DebugString().c_str());
}

void HttpSvr::OnClientRequest(DataHandle * phandle)
{
    if(phandle == NULL)
    {
        return ;
    }
    if(phandle->Event()&DATA_CLOSE)
    {
        LOG_DBG("close");
        return;
    }
    if( phandle->Event()&DATA_CONNECT)
    {
        //do something
        LOG_DBG("-----------");
        //HandleRequest(phandle);
        LOG_SHOW("socket deal ret fd:%d event:%d",phandle->GetFD(),phandle->Event());
        return;
    }
    Httpd * httpd = phandle->_httpd;
    if(!httpd)
    {
        LOG_ERROR("error httpd");
        return;
    }
    if(httpd->RetCode()!= 0)
    {
        std::string out; 
        httpd->SynthesisResponse(std::string(),std::string(),out);
        phandle->SendPackage(out);
        httpd->SetRetCode(0);
    }
    else
    {
        HandleRequest(phandle);
    }
}

void HttpSvr::HandleRequest(DataHandle * phandle)
{
    Httpd * httpd = phandle->GetHttpd();
    if(!httpd)
    {
        LOG_ERROR("error httpd or url!");
        phandle->SetEvent(DATA_CLOSE);
        return;
    }
    std::string url = httpd->GetValueByKey("url") , out ,headers ,content,
        tmp_url =std::string(_url)+std::string(_root_path)+std::string(_main_page);
    if((httpd->GetValueByKey("Upgrade")=="WebSocket"
        ||httpd->GetValueByKey("Upgrade")=="websocket"||httpd->GetValueByKey("Upgrade")=="WEBSOCKET"))
    {

        if(httpd->HaveKey("Sec-WebSocket-Key"))
        {
            //Sec-Websocket-Accept
            std::string key = httpd->GetValueByKey("Sec-WebSocket-Key");
            std::string res_key = httpd->MakeWebSocketKey(key);
            if(res_key.length()!=0)
            {
                headers =headers + "Sec-Websocket-Accept: " + res_key+
                +"\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n";
                phandle->SetProtocolType ( WEBSOCKET );
                httpd->SetRetCode(101);
            }
            else
            {
                httpd->SetRetCode(400);
            }
            LOG_DBG("key : %s res key : %s",key.c_str(),res_key.c_str());
        }
        else
        {
            httpd->SetRetCode(400);
        }
        

    }
    if(url=="/"||url == _url||url.length()<= 1)
    {
        if(httpd->RetCode()==0)
        {
            LOG_DBG("-------------url :%s",url.c_str());
            _header_map["Content-Language: "] = "en,zh";
            _header_map["Server: "          ] = "PPH_http_sver 1.0";
            _header_map["Content-Length: "  ] = "0";
            _header_map["Location: "        ] = tmp_url;
            httpd->SetState(MOVED);

                httpd->SetRetCode(301);
            headers = "Location: "+_header_map["Location: "        ]+"\r\n"
                +"Content-Language: "+_header_map["Content-Language: "] + "\r\n"
                +"Content-Length: "+_header_map["Content-Length: "] + "\r\n"
                +"Server: "+_header_map["Server: "          ] + "\r\n";
        }
        httpd->SynthesisResponse(headers,content,out);
        LOG_DBG("content len %d \n%s\n%s",content.length(),content.c_str(),out.c_str());
    }
    else if(tmp_url.find(url)!=std::string::npos) 
    {
        LOG_DBG("-------------url :%s",url.c_str());

        if(httpd->RetCode() == 0)
        {
            if(_main_data.length()<= 1)
            {
                int ret = 0;
                std::string path =std::string(_root_path)+std::string(_main_page);
                if((ret = LoadResoures(path,_main_data))<0)
                {
                    if(ret == -1)
                    {
                        httpd->SetRetCode(404);
                    }
                }
            }
            if(content.length()>2048&&httpd->GetValueByKey("Accept-Encoding").find("gzip")!=std::string::npos)
            {
                int ret = 0;
                if((ret = httpd->Encode(GZIP,_main_data,_main_data))==0)
                {
                    headers += "Content-Encoding: gzip\r\n";
                    headers += "Accept-Encoding: gzip\r\n";
                }
            }
            std::string type = GetSrcType(url);
            if(type.length()==0)
            {
                httpd->SetRetCode(404);
            }
            else
            {
                headers =headers+ "Content-Type: " + type+"\r\n";
                headers =headers+"Content-Length: "+std::to_string(_main_data.length()) + "\r\n"
                    +"Server: "+_header_map["Server: "          ] + "\r\n";
                httpd->SetRetCode(200);
            }
            httpd->SynthesisResponse(headers,_main_data,out);
        }
        else
        {
            httpd->SynthesisResponse(headers,std::string(),out);
        }
        
        LOG_DBG("\n%s\n---------------------------------------- data len:%d",out.c_str(),_main_data.length());
    }
    else 
    {
        LOG_DBG("-------------url :%s",url.c_str());
        int ret =0;
        std::string type = GetSrcType(url);
        if(type.length()==0)
        {
            httpd->SetRetCode(404);
        }
        else
        {
            headers =headers+ "Content-Type: " + type+"\r\n";
            if((ret = LoadResoures(url,content))<0)
            {
                if(ret == -1)
                {
                    httpd->SetRetCode(404);
                }
            }
            else if(content.length()>2048&&httpd->GetValueByKey("Accept-Encoding").find("gzip")!=std::string::npos)
            {
                int ret = 0;
                if((ret = httpd->Encode(GZIP,content,content))==0)
                {
                    headers += "Content-Encoding: gzip\r\n";
                    headers += "Accept-Encoding: gzip\r\n";
                                    LOG_DBG("-------------");
                }
            }
        }
        
        if(httpd->RetCode() == 0)
        {
            headers =headers+"Content-Length: "+std::to_string(content.size()) + "\r\n"
            +"Server: "+_header_map["Server: "          ] + "\r\n";
            httpd->SetRetCode(200);
            httpd->SynthesisResponse(headers,content,out);
        }
        else
        {
            httpd->SynthesisResponse(std::string(),std::string(),out);
        }
        
        LOG_DBG("\n%s\n---------------------------------------- data len:%d",out.c_str(),content.size());
    }
    
    phandle->SendPackage(out);

    if(httpd->RetCode()!=301&&((!httpd->HaveKey("Connection"))||(httpd->GetValueByKey("Connection")=="close"))
        &&(phandle->GetProtocolType() ==HTTP ||phandle->GetProtocolType() ==HTTPS))
    {
        LOG_DBG("shutdown read %d %s",httpd->HaveKey("Connection"),httpd->GetValueByKey("Connection").c_str());
        shutdown(phandle->GetFD(), SHUT_RD);
    }
    httpd->SetRetCode(0);
}
/*ERR_ID 0 ;  HEART = 1 ;  CREATE_ROOM     = 2 ;  RES_CREATE_ROOM = 3 ;    DELETE_ROOM     = 4 ;
RES_DELETE_ROOM = 5 ;JOIN_ROOM       = 6 ;RES_JOIN_ROOM= 7 ;LEAVE_ROOM    = 8 ;RES_LEAVE_ROOM= 9 ;
START_GAME      = 10;RES_START_GAME  = 11;OVER_GAME    = 12;RES_OVER_GAME = 13;DEAL_CARDS    = 14;
PLAY_CARDS      = 15;RES_PLAY_CARDS  = 16;FOLD_CARDS   = 17;RES_FOLD_CARDS= 18;GET_CARDS     = 19;
RES_GET_CARDS   = 20;CLEAR_PLAY_CARDS= 21;RES_CLEAR_PLAY_CARDS= 22;CONFIG_ROOM = 23;RES_CONFIG_ROOM = 24;*/
void HttpSvr::OnWebSocket(DataHandle * phandle)
{
    if(phandle->Event()&DATA_CLOSE)
    {
        return;
    }
    if(phandle->GetWebSocket() == NULL)
    {
        return;
    }
    PokerGame::msg_package msg_package ;
    INT64 size = 0;
    const char* pData = phandle->GetWebSocket()->GetReadBuf(size);
    if(pData == NULL)
    {
        LOG_ERROR("Get ReadData error fd: %d",phandle->GetFD());                      
        return ;
    }
    if(!msg_package.ParseFromArray(pData,size))
    {
        LOG_ERROR("parse protobuf msg_package error! ");
        return ;
    }
    LOG_DBG("{\n%s}", msg_package.DebugString().c_str());

    switch (msg_package.id())
    {
    case 0:
        {
            LOG_ERROR("err req_id:0 user_id:%d",msg_package.user_id());
            return;
            break;
        }
    case 1:
        {
            break;
        }
    case 2:
        {
            pthread_mutex_lock(&m_room_mutex);
            OnCreatRoom(msg_package,phandle);
            pthread_mutex_unlock(&m_room_mutex);
            break;
        }
    case 6:
        {
            pthread_mutex_lock(&m_room_mutex);
            if(OnJoinRoom(msg_package,phandle)!=0)
            {
                pthread_mutex_unlock(&m_room_mutex);
                return;
            }
            pthread_mutex_unlock(&m_room_mutex);
            break;
        }
         
    default:
        if(HaveRoom(msg_package.room_id()))
        {
            _room_map[msg_package.room_id()]->HandleRequest(msg_package,phandle);
        }
        return;
        break;
    }
    std::string res_str;
    msg_package.SerializeToString(&res_str);
    phandle->GetWebSocket()->EncodeFream(res_str,WS_BINARY_FRAME);
    LOG_DBG("{\n%s}", msg_package.DebugString().c_str());

    phandle->SendPackage(res_str);
}

int HttpSvr::OnJoinRoom(PokerGame::msg_package & msg_package,DataHandle* handle)
{
    PokerGame::response* res_pkg = new PokerGame::response;
    msg_package.set_id(PokerGame::RES_JOIN_ROOM);
    if(res_pkg == NULL)
    {
        return -1;
    }
    if(msg_package.room_id() == 0)
    {
        res_pkg->set_ret(PokerGame::ERR_REQUEST);
        res_pkg->set_reson("roomid == 0");
    }
    else 
    {
        if(!HaveRoom(msg_package.room_id()))
        {
            res_pkg->set_ret(PokerGame::ERR_REQUEST);
            res_pkg->set_reson("room not find");
        }
        else
        {
            Room* room = _room_map[msg_package.room_id()];
            int ret = 0;
            if((ret=room->JoinState(msg_package.user_id()))==1)
            {
                return room->onReConnect(msg_package,handle);
            }
            else if(ret == -1)
            {
                return -1;
            }

            User* user = new User(msg_package.user_id());
            if(user == NULL)
            {
                res_pkg->set_ret(PokerGame::ERR_REQUEST);
                res_pkg->set_reson("alloc user err");
            }
            else
            {
                if(room->Join(msg_package.user_id(),user)!=0)
                {
                    delete user;
                    res_pkg->set_ret(PokerGame::ERR_REQUEST);
                    res_pkg->set_reson("join user err rooid : "+ std::to_string(msg_package.room_id()));
                }
                else
                {
                    user->_handle = handle;
                    room->Broadcast(msg_package,handle,msg_package.user_id());
                    res_pkg->set_ret(PokerGame::SUCCESS);
                    PokerGame::game_info info;
                    PokerGame::room_conf * pConf = new PokerGame::room_conf(room->_conf);
                    //state seats
                    info.set_state((int)room->_state);
                    for(int i=1;i<room->_seats.size();++i)
                    {
                        if(room->_seats[i]._user>0&&room->_seats[i]._uid!=msg_package.user_id())
                        {
                            PokerGame::seats_info * seat=info.add_seats();
                            seat->set_user_id(room->_seats[i]._uid);
                            seat->set_id(room->_seats[i]._seat_id);
                        }
                    }
                    info.set_allocated_conf(pConf);
                    std::string str_info;
                    info.SerializeToString(&str_info);
                    res_pkg->set_pkg(str_info);
                }
            } 
        }
        
    }
    msg_package.set_id(PokerGame::RES_JOIN_ROOM);
    msg_package.set_allocated_res_pkg(res_pkg);
    return 0;
}

int HttpSvr::OnCreatRoom(PokerGame::msg_package & msg_package,DataHandle* handle)
{
    PokerGame::response* res_pkg = new PokerGame::response;
    if(res_pkg == NULL)
    {
        return -1;
    }
    PokerGame::create_room create;

    int room_id = RandRoomID();
    Room * room = new Room(room_id);
    if(!create.ParseFromString(msg_package.req_pkg()))
    {
        LOG_DBG("parse create room err!");
        res_pkg->set_ret(PokerGame::ERR_REQUEST);
        res_pkg->set_reson("parse create_room err!");
    }
    else if(room == NULL)
    {
        res_pkg->set_ret(PokerGame::ERR_REQUEST);
        res_pkg->set_reson("alloc create room err!");
    }
    else
    {
        _room_map[room_id] = room;
        msg_package.set_room_id(room_id);

        room->_conf = create.conf();
        LOG_DBG("{%s}\n{%s}",create.conf().DebugString().c_str(),room->_conf.DebugString().c_str());
        if(0!=room->InitRoom())
        {
            LOG_ERROR("creat  room err!uid : %d rid : %d",msg_package.user_id(),room_id);
            res_pkg->set_ret(PokerGame::ERR_REQUEST);
            res_pkg->set_reson("create room err!");
        }
        User * user = new User(msg_package.user_id());
        if(!user)
        {
            LOG_ERROR("alloc user err!uid : %d rid : %d",msg_package.user_id(),room_id);
            res_pkg->set_ret(PokerGame::ERR_REQUEST);
            res_pkg->set_reson("create user err!");
        }
        else if(room->Join(msg_package.user_id(),user)!=0)
        {
            LOG_ERROR("join user err!uid : %d rid : %d",msg_package.user_id(),room_id);
            res_pkg->set_ret(PokerGame::ERR_REQUEST);
            res_pkg->set_reson("join user err!");
        }
        else{
            user->_handle = handle;
            res_pkg->set_ret(PokerGame::SUCCESS);
        }
    } 
    msg_package.set_id(PokerGame::RES_CREATE_ROOM);
    msg_package.set_allocated_res_pkg(res_pkg);
    return 0;
}

int HttpSvr::LoadResoures(std::string& path,std::string & out)
{
    if(_src_map[path].length()!=0)
    {
        out= _src_map[path];
        return 0;
    }
    if(path.length()<=1)
    {
        return -1;
    }
    if(path[0]!='.')
    {
        if(path[0]=='/')
        {
            path = "."+path;
        } 
        else
        {
            path = "./"+path;
        }    
    }
    LOG_DBG("\n%s",path.c_str());
    std::string tmp,line;
    std::ifstream fin( path.c_str() );   
    char ch;
    while ( fin.get(ch ))
    {
       out.push_back(ch);
    }
    fin.close();
    _src_map[path] = out;
    return 0;
}

bool HttpSvr::IsSvrSocket(int fd)
{
    std::map<int ,SvrData*> :: iterator it = _svr_map.begin();
    for(;it!=_svr_map.end();it++)
    {
        if(it->second->_fd==fd)
        {
            return true;
        }
    }
    return false;
}

std::string HttpSvr::GetSrcType(std::string& type)
{
    int pos=0;
    if((pos=type.find_last_of('.'))!=std::string::npos)
    {
        if(pos == type.length()-1)
        {
            return std::string();
        }
        std::string tmp = type.substr(pos);
        LOG_DBG("type:%s : %s ",tmp.c_str(),_type_map[tmp].c_str());
        if(_type_map.find(tmp) != _type_map.end())
        {
            return _type_map[tmp];
        }
    }
    LOG_ERROR("not find file type %s",type.c_str());

    return std::string();
}

int HttpSvr::RandRoomID()
{
    int room_id=0;
    srand((unsigned)time(NULL));
    room_id=(rand()*456789) % (999999 - 100000)+ 100000;
    LOG_DBG("rand id %d",room_id);
    if (room_id < 0)
    {
        room_id =-room_id;
    }
    if(HaveRoom(room_id))
    {
        return RandRoomID();
    }
    return room_id;
}

void HttpSvr::Debug()
{

}

HttpSvr* HttpSvr:: GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new HttpSvr();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}