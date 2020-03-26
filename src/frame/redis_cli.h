#pragma once
#include <iostream>
#include <string>
#include "thread_pool/task.h"
#include "thread_pool/thread_pool.h"
#include <pthread.h>
#include "../3rd/hredis/hiredis.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>

#define G_REDIS_CLI RedisClient::GetInstance()

class RedisReq :public Task
{
private:
    /* data */
public:
    RedisReq(int redis_id,int req_id,Task* owner ,std::string command);
    ~RedisReq();
    virtual void Debug(){};
    virtual void Clear(){}
    virtual void Run(void * args);
public:
    int redis_host_id;
    std::string req_command;
    int redis_req_id;
    Task * req_owner;
};

class RedisRes : public Task
{
private:
    /* data */
public:
    RedisRes(int redis_id,int req_id,Task* owner ,redisReply* reply);
    ~RedisRes();
    virtual void Clear(){}
    virtual void Debug(){};
    virtual void Run(void * args);
public:
    int redis_host_id;
    int redis_req_id;
    Task * req_owner;
    redisReply* p_reply;
};

void * RedisThreadFunc(void* args);

class RedisClient : public Task
{
private:
    /* data */
public:
    RedisClient();
    ~RedisClient();
    virtual void Clear(){}
    virtual void Debug(){};
    virtual void Run(void * args);
    int RedisConnect(int redis_host_id,std::string ip = "127.0.0.1",short port =6379 ,std::string pswd="");
    int RedisCommand(Task* req);
    bool RedisReconnect(int redis_host_id);
    void PushReq(Task* req);
    int SetTimeout(int sec,int usec);
    int Auth(int redis_host_id,std::string pswd);
    Task* PopReq();
    static RedisClient* GetInstance();
public:
    static RedisClient* g_self;
    bool redis_close;
private:
    redisContext* redis_context;
    std::string   redis_ip ;
    std::string   auth_pswd;
    short         redis_port;
    std::queue<Task*> req_queue;
    mutable pthread_cond_t m_cond;
    mutable pthread_mutex_t m_data_mutex;
    std::map<int,redisContext*> redis_conn_map;
};

