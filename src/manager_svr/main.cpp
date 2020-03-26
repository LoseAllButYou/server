#include <iostream>
#include "loger.h"
#include "thread_pool/thread_pool.h"
#include "timer.h"
#include "network.h"
#include "redis_cli.h"
#include "mgr_server.h"

int main(int argc,char** args)
{
    char * loger_path = "";
    if(argc > 1)
    {
        loger_path = args[1]; 
    }
    G_Thread_pool->Init(1234);

    G_Thread_pool->AddFunc("loger",LogThreadFunc);
    LOGER->Start("./loger/","loger",1024*1024*100);
    G_Thread_pool->AddFunc("timer",TimerThreadFunc);
    G_Thread_pool->AddFunc("network",NetworkThreadFunc);
    G_Thread_pool->AddFunc("redis",RedisThreadFunc);
    G_REDIS_CLI->RedisConnect(0);
    G_Thread_pool->AddThreads("redis",1);
    G_Thread_pool->AddThreads("loger",1);
    G_Thread_pool->AddThreads("timer",2);

    DataHandle::work_class = G_MGR_SVR;
    if(!G_MGR_SVR->GetSvrInfo(loger_path))
    {
        GeneralSleep(2000);
        return -1;
    }
    G_NETWORK->SetListenHost("127.0.0.1",8000);
    G_NETWORK->EpollUp();
    G_NETWORK->Listen();
    G_Thread_pool->AddThreads("network",1);
    G_Thread_pool->AddWorkThreads(4);
    G_Thread_pool->JoinAllThread();
    return 0;
}