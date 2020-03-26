#include "mgr_server.h"
#include "timer.h"
#include "redis_cli.h"
#include "config.h"
#include "loger.h"
#include "mgrsvr_protocol.pb.h"
#include "define.h"
#include<bits/stdc++.h>
#include <stdio.h>

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
Mgr_Svr* Mgr_Svr:: g_self = NULL;

 SvrData::SvrData(int id,int type,int fd,std::string version,std::vector<int> link_svr)
    :_ID(id),_type(type),_fd(fd),_version(version),_link_svr(link_svr)
 {
 }

SvrData::~SvrData()
{
    std::string().swap(_version);
    std::vector<int>().swap(_link_svr);
}

Mgr_Svr::Mgr_Svr()
    :Task(TASK_DEFAULT,"manager server")
    ,_ID(-1),_type(-1),_version("")
{
    pthread_mutex_init(&m_mutex,NULL);
}

Mgr_Svr::~Mgr_Svr()
{
    std::string().swap(_version);
    std::map<int,SvrData*>::iterator it= _svr_map.begin();
    for (;it!=_svr_map.end();)
    {
        _svr_map.erase(it);
    }
    pthread_mutex_destroy(&m_mutex);

}

bool Mgr_Svr::GetSvrInfo(std::string conf_path)
{
    Config conf;

    if(conf_path!="")
    {
        conf.SetPath(conf_path);
    }
    conf.ReadConfValue();
    if( 0>conf.GetValue("TYPE",_type)||
        conf.GetValue("ID",_ID)<0||
        conf.GetValue("VERSION",_version)<0||
        conf.GetValue("LINK_SVR",_link_svr)<0)
    {
        LOG_ERROR("server conf : %d %d %s",_ID,_type,_version.c_str());

        return false;
    }
    std::string tmp ="";
    conf.GetValue("LINK_SVR",tmp);
    LOG_DBG("server conf : %d %d %s %s",_ID,_type,_version.c_str(),tmp.c_str());

    return true;
}

int Mgr_Svr::AddSvrData(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
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

bool Mgr_Svr::DelSvrData(int id)
{
    std::map<int,SvrData*>::iterator it= _svr_map.find(id);
    if(it != _svr_map.end() )
    {
        _svr_map.erase(it);
    }

    return true;
}

bool Mgr_Svr::UpDateSvr(int id,int fd,int type,const std::string & version,std::vector<int>& link_svr)
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

SvrData* Mgr_Svr::GetSvrData(int id)
{
    if(_svr_map.find(id) == _svr_map.end())
    {
        return NULL;
    }
    return _svr_map[id];
}


void Mgr_Svr::Run(void* args)
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
        TimerOver* timer_over = (TimerOver*)args;
        if(timer_over->timer_name == "heart_timer")
        {
            int svr_id =timer_over->timer_id ;
            SvrData* Svr = GetSvrData(svr_id);
            if(Svr==NULL)
            {
                LOG_ERROR("Svr data does not exsit! svr id:%d",svr_id);
                pthread_mutex_unlock(&m_mutex);
                return;
            }
            DataHandle* handle = G_NETWORK->GetHandle(Svr->_fd);
            if(handle == NULL)
            {
                LOG_ERROR("Svr handle does not exsit! svr id:%d",svr_id);
                pthread_mutex_unlock(&m_mutex);
                return;
            }
            mgr_svr::PACKAGE res_pkg ;
            res_pkg.set_id(mgr_svr::HEART);
            res_pkg.set_svr_id(_ID);
            mgr_svr::RESPONSE res ;
            res.set_error(mgr_svr::SUCCESS);
            std::string pkg_str;
            res.SerializeToString(&pkg_str);
            res_pkg.set_package(pkg_str);
            std::string res_str;
            res_pkg.SerializeToString(&res_str);
            handle->SendPackage(res_str);
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
        const char* pData = (char*)phandle->GetReadData(size);
        if(pData == NULL)
        {
            LOG_ERROR("Get ReadData error fd: %d",phandle->GetFD());        
            pthread_mutex_unlock(&m_mutex);
            return ;
        }
        std::string data = std::string(pData,0,size);
        mgr_svr::PACKAGE package ;
        mgr_svr::PACKAGE res_pkg ;
        res_pkg.set_svr_id(_ID);
        package.ParseFromString(data);
        LOG_DBG("{\n%s}", package.DebugString().c_str());

        switch (package.id())
        {
        case mgr_svr::HEART:
        {
            G_TIMER->AddTimer("heart_timer",package.svr_id(),3000,this);
            break;
        }
        case mgr_svr::START:
        {
            mgr_svr::START_SVR start_pkg;
            res_pkg.set_id(mgr_svr::RES_START);
            mgr_svr::RESPONSE res ;
            start_pkg.ParseFromString(package.package());
            if(package.svr_id()==-1)
            {
                LOG_ERROR("svr err svr id : %d",package.svr_id());
                res.set_error(mgr_svr::ERR_ID);
            }
            if(start_pkg.type()== -1)
            {
                LOG_ERROR("svr err type svr id : %d",package.svr_id());
                res.set_error(mgr_svr::ERR_TYPE);
            }
            if(start_pkg.link_svr_size()==0)
            {
                LOG_ERROR("svr dosn't have link svr id : %d",package.svr_id());
                res.set_error(mgr_svr::ERR_LINK_NUM);
            }
            else
            {
                std::vector<int>  vec ;
                for(int i = 0;i<start_pkg.link_svr_size();++i)
                {
                    vec.push_back(start_pkg.link_svr(i));
                }
                int ret = AddSvrData(package.svr_id(),phandle->GetFD(),start_pkg.type(),start_pkg.version(),vec);
                if(ret == -1)
                {
                    res.set_error(mgr_svr::ERR_EXIST);
                }
                else if(ret == -2)
                {
                    res.set_error(mgr_svr::ERR_ALLOC);
                }
                else
                {
                    res.set_error(mgr_svr::SUCCESS);       
                }
                
            }  
            std::string pkg_str;
            res.SerializeToString(&pkg_str);
            res_pkg.set_package(pkg_str);
            std::string res_str;
            res_pkg.SerializeToString(&res_str);
            phandle->SendPackage(res_str);
            break;
        }
        case mgr_svr::STOP:
        {
            mgr_svr::RESPONSE res ;
            res_pkg.set_id(mgr_svr::RES_STOP);
            DelSvrData(package.svr_id());
            res.set_error(mgr_svr::SUCCESS);
            std::string pkg_str;
            res.SerializeToString(&pkg_str);
            res_pkg.set_package(pkg_str);
            std::string res_str;
            res_pkg.SerializeToString(&res_str);
            phandle->SendPackage(res_str);
            break;
        }

        case mgr_svr::UPDATE:
        {
            mgr_svr::UPDATE_SVR updata_pkg;
            mgr_svr::RESPONSE res ;
            res_pkg.set_id(mgr_svr::RES_UPDATE);
            updata_pkg.ParseFromString(package.package());
            
            std::vector<int>  vec ;
            for(int i = 0;i<updata_pkg.link_svr_size();++i)
            {
                vec.push_back(updata_pkg.link_svr(i));
            }
            if(!UpDateSvr(package.svr_id(),phandle->GetFD(),updata_pkg.type(),updata_pkg.version(),vec))
            {
                res.set_error(mgr_svr::ERR_EXIST);
            } 
            else
            {
                res.set_error(mgr_svr::SUCCESS);
            }
            std::string pkg_str;
            res.SerializeToString(&pkg_str);
            res_pkg.set_package(pkg_str);
            std::string res_str;
            res_pkg.SerializeToString(&res_str);
            phandle->SendPackage(res_str);
            break;
        }
        case mgr_svr::CMD:
        {
            mgr_svr::CONSLOE_CMD cmd_pkg;
            cmd_pkg.ParseFromString(package.package());
            LOG_DBG("cmd args %d %d %s",package.svr_id(),cmd_pkg.cmd_id(),cmd_pkg.context().c_str());
            DealCmd(package.svr_id(),cmd_pkg.cmd_id(),phandle,cmd_pkg.context());
            break;
        }
        default:
            LOG_ERROR("err package id:%d",package.id());
            break;
        }
        LOG_DBG("{\n%s}", res_pkg.DebugString().c_str());
    }
    pthread_mutex_unlock(&m_mutex);
}

void  Mgr_Svr::DealCmd(int svr_id,int cmd,DataHandle* pHandle, const std::string & context)
{
    mgr_svr::RESPONSE res ;
    mgr_svr::PACKAGE res_pkg ;
    res_pkg.set_id(mgr_svr::RES_CMD);
    // STOP_SVR         
    // RESTART          
    // RECONF           
    // CLOSE_ALL_SVR    
    // CLOSE_SVR_BY_TYPE
    // CLOSE_SVR_BY_ID  
    // TEST_CHAT        
    switch (cmd)
    {
    case mgr_svr::STOP_SVR:
    {
        LOG_DBG("SERVER stop by cmd !");
        res.set_error(mgr_svr::SUCCESS);
        std::string pkg_str;
        res.SerializeToString(&pkg_str);
        res_pkg.set_package(pkg_str);
        std::string res_str;
        res_pkg.SerializeToString(&res_str);
        pHandle->SendPackage(res_str);
        LOG_DBG("{\n%s}", res_pkg.DebugString().c_str());
        GeneralSleep(2000);
        G_Thread_pool->CancelAllThread();
        LOGER->close_loger = true;
        G_TIMER->timer_close = true;
        G_REDIS_CLI ->redis_close = true;
        G_NETWORK->epoll_close = true;
        return;
    }
    case mgr_svr::RESTART:
        /* code */
        LOG_DBG("restart");
        break;
    case mgr_svr::RECONF:
    {
        LOG_DBG("reconf");
        GetSvrInfo();
        res.set_error(mgr_svr::SUCCESS);    
        break;
    }
    case mgr_svr::CLOSE_ALL_SVR:
        /* code */
        LOG_DBG("close all");
        break;
    case mgr_svr::CLOSE_SVR_BY_TYPE:
        /* code */
        LOG_DBG("close by type");
        break;
    case mgr_svr::CLOSE_SVR_BY_ID:
        /* code */
        LOG_DBG("close by id");
        break;
    case mgr_svr::TEST_CHAT:
    {
        LOG_DBG("Test chat!");
        printf("Svr_%d>> %s\nSelf>> ",svr_id,context.c_str());
        fflush(stdout);
        char str[100] = {0};
        std::cin>>str;
        res.set_context(std::string(str));
        break;
    }
    default:
        break;
    }
    std::string pkg_str;
    res.SerializeToString(&pkg_str);
    res_pkg.set_package(pkg_str);
    std::string res_str;
    res_pkg.SerializeToString(&res_str);
    pHandle->SendPackage(res_str);
    LOG_DBG("{\n%s}", res_pkg.DebugString().c_str());
}


void Mgr_Svr::Debug()
{

}

Mgr_Svr* Mgr_Svr::GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new Mgr_Svr();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}
