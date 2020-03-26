#include <iostream>
#include "loger.h"
#include "thread_pool/thread_pool.h"
#include "timer.h"
#include "network.h"
#include "redis_cli.h"
#include "http_svr.h"
#include<signal.h>

void SigDeal(int sig)
{
    LOG_DBG("sigal :%d",sig);
}

int main(int argc,char** args)
{
    char * conf_path = "";
    if(argc > 1)
    {
        conf_path = args[1]; 
    }
    signal(SIGPIPE,SigDeal);
    G_Thread_pool->Init(4567);

    G_Thread_pool->AddFunc("loger",LogThreadFunc);
    LOGER->Start("./loger/","loger",1024*1024*100);
    G_Thread_pool->AddFunc("timer",TimerThreadFunc);
    G_Thread_pool->AddFunc("network",NetworkThreadFunc);
    //G_Thread_pool->AddFunc("redis",RedisThreadFunc);
    G_REDIS_CLI->RedisConnect(0);

    //G_Thread_pool->AddThreads("redis",1);

    G_Thread_pool->AddThreads("loger",1);
    G_Thread_pool->AddThreads("timer",1);
    G_Thread_pool->AddWorkThreads(4);

    DataHandle::work_class = G_HTTP;
    if(!G_HTTP->Start(conf_path))
    {
        LOG_ERROR("start err!!!");
        GeneralSleep(2000);
        return -1;
    }
    // if(0>G_NETWORK->EpollUp())
    // {
    //     GeneralSleep(2000);
    //     return -2;
    // }
    // if(0>G_NETWORK->Listen())
    // {
    //     GeneralSleep(2000);
    //     return -2;
    // }
    G_Thread_pool->AddThreads("network",1);

    G_Thread_pool->JoinAllThread();      
    printf("all tread exit");
    return 0;
}