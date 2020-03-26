#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "thread_pool/task.h"
#include "thread_pool/thread_pool.h"
#include <pthread.h>
#include <algorithm>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define G_TIMER TimerManager::GetInstance()

//毫秒级线程 休眠函数 
void GeneralSleep(unsigned int millisecond);

class Timer 
{
private:
    /* data */
public:
    Timer(std::string name,int id,long  msec,Task* owner);
    ~Timer();
    void StartTimer();
    bool operator> (const Timer & other) const;
public:
    std::string timer_name;
    int         timer_id;
    long   timer_msec;//毫秒 
    timeval     timer_begin;//定时器开始时间
    Task*       timer_owner;
};


class TimerOver : public Task
{
private:
    /* data */
public:
    TimerOver(std::string name,int id,Task* owner);
    ~TimerOver();
    virtual void Run(void * args);
    virtual void Clear(){}
    virtual void Debug();
public:
    std::string timer_name;
    int         timer_id;
    Task*       timer_owner;
};

void * TimerThreadFunc(void* args);

class TimerManager : public Task
{
private:
    void InitHeap();
    void TraverseHeap();//遍历定时器队列
public:
    TimerManager(/* args */);
    ~TimerManager();
    virtual void Run(void * args);
    virtual void Clear(){}
    virtual void Debug();

    int AddTimer(std::string name,int id,long  msec ,Task * owner);
    bool RemoveTimer(Task* owner,int id);//移除定时器
    int  TimerOut(Timer & timer);//定时器时间到
    static TimerManager* GetInstance();
public:
    static TimerManager* g_self;
    std::vector<Timer>   timer_heap;//vec组建小根堆
    bool                 timer_close;
private:
    mutable pthread_cond_t m_cond;
};




