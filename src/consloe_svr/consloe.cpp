#include "consloe.h"
#include "timer.h"
#include "network.h"
#include "redis_cli.h"
#include "config.h"
#include "loger.h"
#include "mgrsvr_protocol.pb.h"
#include "define.h"
#include <bits/stdc++.h>
#include <stdio.h>
#include "string.h"

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
Consloe* Consloe:: g_self = NULL;

SvrData::SvrData(int id,int type,int fd,std::string version,std::vector<int> link_svr)
    :_ID(id),_type(type),_fd(fd),_version(version),_link_svr(link_svr)
 {
 }

SvrData::~SvrData()
{
    std::string().swap(_version);
    std::vector<int>().swap(_link_svr);
}

Consloe::Consloe()
    :Task(TASK_DEFAULT,"consloe server")
    ,_chat_res(false),_ID(-1),_type(-1),_mgr_svr_id(-1),_version("")
{
    pthread_mutex_init(&m_mutex,NULL);
}

Consloe::~Consloe()
{
    std::string().swap(_version);
    pthread_mutex_destroy(&m_mutex);
}

bool Consloe::GetSvrInfo(std::string conf_path)
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

    return true;
}

int Consloe::LinkConfSvr(std::string key,std::string conf_path)
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
        _ID = id;
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
            int fd = G_NETWORK->Connect(ip,port);
            if(fd>0)
            {
                if(0>AddSvrData(id,fd,type,version,vec))
                {
                    G_NETWORK->DelSocketFd(fd);
                    return -2;
                }
                else
                {
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
                    G_TIMER->AddTimer("heart_timer",id,3000,this);
                }       
            }
            else
            {
                return -3;
            }
            
        }   
    }
                    LOG_DBG("this : %x",this);
        GeneralSleep(2000);

    return 0;
}

int Consloe::Start(std::string conf_path)
{
    return GetSvrInfo(conf_path)&&LinkConfSvr("manager","")==0;
}


int Consloe::AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
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

bool Consloe::DelSvrData(int id)
{
    std::map<int,SvrData*>::iterator it= _svr_map.find(id);
    if(it != _svr_map.end() )
    {
        _svr_map.erase(it);
    }
    return true;
}

bool Consloe::UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
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

SvrData* Consloe::GetSvrData(int id)
{
    if(_svr_map.find(id) == _svr_map.end())
    {
        return NULL;
    }
    return _svr_map[id];
}

void Consloe::Run(void* args)
{
    pthread_mutex_lock(&m_mutex);
    if(args == NULL)
    {
        LOG_ERROR("error args ");
        pthread_mutex_unlock(&m_mutex);
        return;
    }
    LOG_DBG("----- %x %d " ,args,((Task*)args)->task_id);
    if(((Task*)args)->task_id == TASK_TIME_OVER)
    {

        TimerOver* timer_over = (TimerOver*)args;
        if(timer_over->timer_name == "heart_timer")
        {
            int svr_id =timer_over->timer_id ;
            SvrData* Svr = GetSvrData(svr_id);
            if(Svr==NULL)
            {
                LOG_ERROR("Svr data does not exsit! svr id:%d",svr_id);
                pthread_mutex_unlock(&m_mutex);
                delete timer_over;
                return;
            }
            DataHandle* handle = G_NETWORK->GetHandle(Svr->_fd);
            if(handle == NULL)
            {
                LOG_ERROR("Svr handle does not exsit! svr id:%d",svr_id);
                pthread_mutex_unlock(&m_mutex);
                delete timer_over;
                return;
            }
            mgr_svr::PACKAGE res_pkg ;
            res_pkg.set_id(mgr_svr::HEART);
            mgr_svr::RESPONSE res ;
            res.set_error(mgr_svr::SUCCESS);
            std::string pkg_str;
            res.SerializeToString(&pkg_str);
            res_pkg.set_package(pkg_str);
            std::string res_str;
            res_pkg.SerializeToString(&res_str);
            handle->SendPackage(res_str);
            LOG_DBG("{\n%s}", res_pkg.DebugString().c_str());
        }
        delete timer_over;
    }
    else if(((Task*)args)->task_id == TASK_REDIS_RES)
    {
        
    }
    else if(((Task*)args)->task_id == TASK_DATA_HANDLE)
    {
        DataHandle * phandle = (DataHandle *)args;
        if(phandle->Event()&DATA_CLOSE || phandle->Event()&DATA_CONNECT)
        {
            //do something
            LOG_SHOW("socket deal ret fd:%d event:%d",phandle->GetFD(),phandle->Event());
            pthread_mutex_unlock(&m_mutex);
            return;
        }
        int size = 0;
        const char* pData = (char * )phandle->GetReadData(size);
        std::string data = std::string(pData,0,size);
         if(pData == NULL)
        {
            LOG_ERROR("Get ReadData error fd: %d",phandle->GetFD());
            pthread_mutex_unlock(&m_mutex);                         
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
                G_TIMER->AddTimer("heart_timer",package.svr_id(),3000,this);
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
        case mgr_svr::RES_CMD:
        {
            LOG_DBG("cmd ret : %d ",res.error());

            if(res.context().length() != 0)
            {
                printf("%s\n",res.context().c_str());
                fflush(stdout);
                _chat_res = false;
            }
            break;
        }
        default:
            LOG_ERROR("err package id:%d",package.id());
            break;
        }
        LOG_DBG("{\n%s}", res.DebugString().c_str());
    }
    pthread_mutex_unlock(&m_mutex);

}

void Consloe:: Cmd()
{
    printf("******* 控制台 ********\n");
    printf("* 请输出控制命令0 - 6  *\n>> ");
    int cmd = -1;
    char buf[100] = {0};
    int state = 0; //-1,退出 0 ，等待命令  1,聊天 2.等待输入聊天
    mgr_svr::PACKAGE package ;
    mgr_svr::CONSLOE_CMD consloe;
    package.set_id(mgr_svr::CMD);
    package.set_svr_id(_ID);
    int cnt =0;
    while(state!= -1)
    {
        ++cnt;
        if(state == 0)
        {
            cmd = -1;
            scanf("%d",&cmd);
            printf("%d\n",cmd);
            char flush[10]={0};
            if(cmd == -1)
            {
                std::cin>>flush;
            }
            if(cmd<0 || cmd>6)
            {
                printf("* 错误！控制命令1 - 7  *\n>> ");
                continue;
            }
            if(cmd != 6)
            {
                printf(">>指令发送中。。。。。 \n");
            }
            else
            {
                state = 2;
            }
            
        }
        if(cmd == 6)
        {
            if( _chat_res )
            {
                GeneralSleep(500);
                continue;
            }
            if(state == 2)
            {
                printf(">>请输出聊天内容 \"quit\" 退出聊天\n>>");
            }
            else
            {
                printf(">>");
            }
            
            std::cin>>buf;
            if(strcmp("quit",buf)==0)
            {
                printf("已退出聊天请继续输入指令\n>>");
                state = 0;
                memset(buf,0,100);
                continue;
            }
            printf("Svr>>");

            _chat_res = true;
            consloe.set_context(buf);
            memset(buf,0,100);
        }
        
        consloe.set_cmd_id(cmd);    
        if(cmd == 6)
        {
            state = 1;
        }

        std::string res_str;
        std::string csl_str;
        consloe.SerializeToString(&csl_str);
        package.set_package(csl_str);
        package.SerializeToString(&res_str);
        SvrData* pData = GetSvrData(_mgr_svr_id);
        if(pData==NULL)
        {
            LOG_ERROR("error svr data id : %d",_mgr_svr_id);
            return ;
        }
        DataHandle* phandle =  G_NETWORK->GetHandle(pData->_fd);
        if(phandle == NULL)
        {
            LOG_ERROR("error handle fd : %d",pData->_fd);
            return ;
        }
        phandle->SendPackage(res_str);
        LOG_DBG("{\n%s} cnt : %d", package.DebugString().c_str(),cnt);
        package.clear_package();
        consloe.Clear();
        if(state != 1)
        {
            printf(">>指令已发送 ！请继续输入指令或直接退出！\n>>");
        }
        fflush(stdout);
    }
}

void Consloe::Debug()
{

}

Consloe* Consloe:: GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new Consloe();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}