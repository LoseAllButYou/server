#include <iostream>
#include "loger.h"
#include "thread_pool/thread_pool.h"
#include "timer.h"
#include "network.h"
#include "redis_cli.h"
#include "config.h"
using namespace std;


class Test : public Task
{
private:
    /* data */
    map<int,DataHandle* > data;
public:
    Test(/* args */)
        :Task(1000,"test") ,client(false)  {
        pthread_mutex_init(&m_mutex,NULL);
        G_Thread_pool->PushTask(new RedisReq(0,1000,this,"set test 4567"));
    }
    virtual void Clear(){}
    ~Test(){
        pthread_mutex_destroy(&m_mutex);
    }
    virtual void Run(void* args){
        pthread_mutex_lock(&m_mutex);

        if(((TimerOver*)args)->task_id ==TASK_TIME_OVER)
        {
            LOG_DBG("test timer enter");
            if(data.find(((TimerOver*)args)->timer_id)==data.end()||
            data[((TimerOver*)args)->timer_id]->Event()&DATA_CLOSE)
            {
                pthread_mutex_unlock(&m_mutex);
                return;
            }
            G_TIMER->AddTimer("timer",((TimerOver*)args)->timer_id,1000,this);
            char buffer[100] ={0};
            int size =9;
            memcpy(buffer,&size,4); 
            sprintf(buffer+4,"woshi %2d\0",((TimerOver*)args)->timer_id); 
            GeneralSleep(1000);
            if(data.find(((TimerOver*)args)->timer_id)!=data.end())
                data[((TimerOver*)args)->timer_id]->AddWriteData((unsigned char*) buffer,13);
        }
        else if(((Task*)args)->task_id ==TASK_REDIS_RES)
        {
            LOG_DBG("test redis enter");

            LOG_DBG("redis value : %s req_id : %d this : 0x%x",((RedisRes*)args)->p_reply->str,((RedisRes*)args)->redis_req_id,this);
            G_Thread_pool->PushTask(new RedisReq(0,1001,this,"get test"));
        }
        else
        {
            LOG_DBG("test network enter");

            if(((DataHandle*)args)->Event() & DATA_CONNECT)
            {
                LOG_DBG("connect! fd : %d",((DataHandle*)args)->GetFD());
                if(client)
                {
                    G_TIMER->AddTimer("timer",((DataHandle*)args)->GetFD(),1000,this);
                    char buffer[100] ={0};
                    int size =9;
                    memcpy(buffer,&size,4); 
                    sprintf(buffer+4,"woshi %2d\0",((DataHandle*)args)->GetFD()); 
                    ((DataHandle*)args)->AddWriteData((unsigned char*) buffer,13);
                    data[((DataHandle*)args)->GetFD()] = ((DataHandle*)args);
                }

            }
            if(((DataHandle*)args)->Event() & DATA_CLOSE)
            {
                LOG_DBG("network : close!!!!!!");
                data.erase(data.find(((DataHandle*)args)->GetFD()));
                ((DataHandle*)args)->Clear();
                pthread_mutex_unlock(&m_mutex);

                return; 
            }
            if(((DataHandle*)args)->Event() & DATA_READ)
            {
                int size = 0;
                char* s= (char*)((DataHandle*)args)->GetReadData(size);
                string str= string(s,size);
                LOG_DBG("network ------ : %s",str.c_str());
                if(!client)
                {
                    size = 8;
                    char buffer[100] ={0};
                    memcpy(buffer,&size,4); 
                    sprintf(buffer+4,"thanks!\0"); 
                    ((DataHandle*)args)->AddWriteData((unsigned char*)buffer,12);
                }
                else
                {
                    G_TIMER->AddTimer("timer",((DataHandle*)args)->GetFD(),1000,this);
                }
                
            }

        }
        pthread_mutex_unlock(&m_mutex);

    }
    virtual void Debug(){ }
    bool client;
    mutable pthread_mutex_t m_mutex;//数据锁

};


int main(int argc,char** args)
{
    if(argc == 1)
    {
        G_Thread_pool->Init(1234);
    }
    else
    {
        G_Thread_pool->Init(4567);
    }


    G_Thread_pool->AddFunc("loger",LogThreadFunc);
    LOGER->Start("./","loger",1024*1024*100);
    G_Thread_pool->AddFunc("timer",TimerThreadFunc);
    G_Thread_pool->AddFunc("network",NetworkThreadFunc);
    G_Thread_pool->AddFunc("redis",RedisThreadFunc);
    G_REDIS_CLI->RedisConnect(0);

    G_Thread_pool->AddThreads("redis",1);

    G_Thread_pool->AddThreads("loger",2);
    G_Thread_pool->AddThreads("timer",2);

    G_Thread_pool->AddWorkThreads(4);
    // Config conf;
    // conf.ReadConfValue();
    // int i=0;
    // conf.GetValue("testint",i);
    // printf("-------------- %d\n",i);
    // bool j=0;
    // conf.GetValue("testbool",j);
    // cout<<"--------------" << j <<endl ;
    // double k=0;
    // conf.GetValue("testdouble",k);
    // printf("-------------- %lf\n",k);

    Test test;
    DataHandle::work_class = &test;
    if(argc>1)
    {
        test.client = true;
        G_NETWORK->EpollUp();
        for(int i = 0;i<10;i++)
        {
            G_NETWORK->Connect("127.0.0.1",8000);
        }
        G_Thread_pool->AddThreads("network",1);
        G_Thread_pool->JoinAllThread();

        
    }
    else
    {
        G_NETWORK->SetListenHost("127.0.0.1",8000);
        G_NETWORK->EpollUp();
        G_NETWORK->Listen();
        G_Thread_pool->AddThreads("network",1);
    }
    // for(int i =0;i<1000;++i)
    // {
    //     LOG_DBG("test thread pool %d",i+1);
    // }
    // LOGER->close_loger =true;
    // G_Thread_pool->thread_pool_close =true;
    // G_TIMER->timer_close =true;
    G_Thread_pool->JoinAllThread();
    return 0;
}