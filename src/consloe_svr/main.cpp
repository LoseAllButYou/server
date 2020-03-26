#include <iostream>
#include "loger.h"
#include "thread_pool/thread_pool.h"
#include "timer.h"
#include "network.h"
#include "redis_cli.h"
#include "consloe.h"

int main(int argc,char** args)
{
    char * loger_path = "";
    if(argc > 1)
    {
        loger_path = args[1]; 
    }
    G_Thread_pool->Init(4567);

    G_Thread_pool->AddFunc("loger",LogThreadFunc);
    LOGER->Start("./loger/","loger",1024*1024*100);
    G_Thread_pool->AddFunc("timer",TimerThreadFunc);
    G_Thread_pool->AddFunc("network",NetworkThreadFunc);
    G_Thread_pool->AddFunc("redis",RedisThreadFunc);
    G_REDIS_CLI->RedisConnect(0);

    G_Thread_pool->AddThreads("redis",1);

    G_Thread_pool->AddThreads("loger",2);
    G_Thread_pool->AddThreads("timer",2);
    G_Thread_pool->AddWorkThreads(4);

    DataHandle::work_class = G_CONSLOE;
    G_NETWORK->EpollUp();
    G_Thread_pool->AddThreads("network",1);
    
    if(!G_CONSLOE->Start(loger_path))
    {
        LOG_ERROR("start err!!!");
        GeneralSleep(2000);
        return -1;
    }
    G_CONSLOE->Cmd();

    G_Thread_pool->JoinAllThread();      
    
    return 0;
}