#include "redis_cli.h"
#include "loger.h"
#include "thread_pool/define.h"
#include "timer.h"
using namespace std;

RedisClient* RedisClient::g_self = NULL;

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

//RedisReq--------------------------------------------
RedisReq::RedisReq(int redis_id,int req_id,Task* owner ,std::string command)
    :Task(TASK_REDIS_REQ,"redis_req")
    ,redis_host_id(redis_id),redis_req_id(req_id),req_owner(owner)
    ,req_command(command)
{
}

RedisReq::~RedisReq()
{
}

void RedisReq::Run(void * args)
{
    G_REDIS_CLI->PushReq(this);
}

//RedisRes--------------------------------------------
RedisRes::RedisRes(int redis_id,int req_id,Task* owner ,redisReply* reply)
    :Task(TASK_REDIS_RES,"redis_res")
    ,redis_host_id(redis_id),redis_req_id(req_id),req_owner(owner),p_reply(reply)
{
}

RedisRes::~RedisRes()
{
    freeReplyObject(p_reply);
}

void RedisRes::Run(void * args)
 {
     req_owner->Run(this);
 }

// RedisClient --------------------------------------

void * RedisThreadFunc(void* args)
{
    G_REDIS_CLI->Run(NULL);
    return args;
}


RedisClient::RedisClient( )
    :Task(TASK_REDIS_CLIENT,"redis_client")
    ,redis_ip(""),redis_port(0)
    ,redis_context(NULL),redis_close(false)
    ,auth_pswd("")
{
    //条件变量的初始化操作
    pthread_cond_init(&m_cond,NULL);
    pthread_mutex_init(&m_data_mutex,NULL);
}

RedisClient::~RedisClient()
{

    //互斥变量使用完成之后必须进行destroy操作
    pthread_mutex_destroy(&m_mutex);
    pthread_mutex_destroy(&m_data_mutex);

    //条件变量使用完成之后进行destroy操作
    pthread_cond_destroy(&m_cond);
    std::map<int,redisContext*>::iterator it=redis_conn_map.begin();

    for(;it!=redis_conn_map.end();it++)
    {
        redisContext* redis_context = it->second;
        if(redis_context)
        {
            redisFree(redis_context);
        }
        redis_conn_map.erase(it);
    }
}

void RedisClient::Run(void * args)
{
    while(!redis_close)
    {
        if(redis_conn_map.size()== 0)
        {
        pthread_mutex_lock(&m_mutex);
        //基于条件变量阻塞，无条件等待，或者是计时等待pthread_cond_timewait
        pthread_cond_wait(&m_cond,&m_mutex) ;
        pthread_mutex_unlock(&m_mutex);
        }
        Task * task =PopReq();
        if(task)
        {
            RedisCommand(task);
            delete task;
        }
    }
}

int  RedisClient::RedisConnect(int redis_host_id,string ip ,short port,string pswd)
{
    if(ip == ""||port <= 0)
    {
        LOG_ERROR("error redis connect host! ip : %s,port : %d",ip.c_str(),port);
        return -1;
    }
    if(redis_conn_map.find(redis_host_id)!=redis_conn_map.end())
    {
        LOG_ERROR("error redis connect exist! id : %d",redis_host_id);
        return -2;
    }
    redis_ip=ip;
    redis_port=port;
    redisContext* redis_context = redisConnect(ip.c_str(),port);
    if( redis_context || redis_context->err )
    {
        LOG_ERROR("connect redis server err! ip : %s port : %d ,err : %s",ip.c_str(),port,redis_context->errstr);
        redisFree(redis_context);
        return -3;
    }
    redis_conn_map[redis_host_id] = redis_context;
    if(pswd!="")
    {
        return Auth(redis_host_id,pswd);
    }
    LOG_SHOW("connect redis success !ip :%s port :%d",ip.c_str(),port);

    return 0;
}

bool RedisClient::RedisReconnect(int redis_host_id)
{
    if(redis_conn_map.find(redis_host_id)==redis_conn_map.end())
    {
        return false;
    }
    redisContext* redis_context = redis_conn_map[redis_host_id];
    if(redis_context->err!=0)
    {
        if(redisReconnect(redis_context)!=0)
        {
            LOG_ERROR("redis reconnect err! reason:%s",redis_context->errstr);
            return false;
        }
        if(auth_pswd != "")
        {
        return Auth(redis_host_id,auth_pswd);
        }
    }

    LOG_SHOW("reconnect redis success !ip :%s port :%d",redis_ip.c_str(),redis_port);

    return true;
}

int RedisClient::RedisCommand(Task * task)
{
    if(redis_conn_map.find(((RedisReq*)task)->redis_host_id)==redis_conn_map.end())
    {
        LOG_ERROR("error redis_host_id : %d",((RedisReq*)task)->redis_host_id);
        return -1;
    }
    redisContext* redis_context = redis_conn_map[((RedisReq*)task)->redis_host_id];
    if(!RedisReconnect(((RedisReq*)task)->redis_host_id))
    {
        redis_conn_map.erase(redis_conn_map.find(((RedisReq*)task)->redis_host_id));
        return -2;
    }

    redisReply* r =(redisReply*)redisCommand(redis_context, (((RedisReq*)task)->req_command).c_str());
    if( NULL == r)
    {
        LOG_ERROR("Execut command1 failure\n");
        return -3;
    }
    if(r->type==REDIS_REPLY_ERROR )
    {
        LOG_ERROR("Failed to execute command[%s] str:%s",(((RedisReq*)task)->req_command).c_str(),r->str);
        freeReplyObject(r);
        redis_context =NULL;
        return -4;
    }
    LOG_DBG("-----------%d,%x,%d",((RedisReq*)task)->redis_req_id,((RedisReq*)task)->req_owner,r->integer);
    RedisRes * res = new RedisRes(((RedisReq*)task)->redis_host_id,\
        ((RedisReq*)task)->redis_req_id,((RedisReq*)task)->req_owner,r);
    if(res==NULL)
    {
        return -5;
    }
    G_Thread_pool->PushTask(res);
    return 0;
}

RedisClient* RedisClient::GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new RedisClient();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}

void RedisClient::PushReq(Task* req)
{
    LOG_DBG("----add task");
    pthread_mutex_lock(&m_data_mutex);
    req_queue.push(req);
    pthread_cond_signal(&m_cond);
    pthread_mutex_unlock(&m_data_mutex);
}

Task* RedisClient::PopReq()
{
    if(req_queue.size() == 0)
    {
        return NULL;
    }
    pthread_mutex_lock(&m_data_mutex);
    Task* task = req_queue.front();
    req_queue.pop();
    pthread_mutex_unlock(&m_data_mutex);
    LOG_DBG("----pop task");

    return task;
}

int RedisClient::SetTimeout(int sec,int usec)
{
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    return redisSetTimeout(redis_context,tv);
}

int RedisClient::Auth(int redis_host_id,std::string pswd)
{
    if(pswd=="")
    {
        redis_conn_map.erase(redis_conn_map.find(redis_host_id));
        return -1;
    }
    auth_pswd=pswd;
    string command = "AUTH "+pswd;
    redisReply* reply =(redisReply*)redisCommand(redis_context, command.c_str());
    if(reply == NULL)
    {
        redis_conn_map.erase(redis_conn_map.find(redis_host_id));
        return -2;
    }
    if(reply->type==REDIS_REPLY_ERROR)
    {
        LOG_ERROR("Failed to execute command[%s]\n",command.c_str());
        freeReplyObject(reply);
        redis_context =NULL;
        redis_conn_map.erase(redis_conn_map.find(redis_host_id));
        return -3;
    }
    freeReplyObject(reply);
    return 0;
}
