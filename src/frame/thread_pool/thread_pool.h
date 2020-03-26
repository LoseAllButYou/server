#pragma once

#include <iostream>

#include <string>
#include <string.h>

#include <stdio.h>
#include <atomic>
#include "thread.h"
#include <vector>
#include <map>
#include "task_queue.h"
#include <atomic>

#define G_Thread_pool Thread_pool::GetInstance()

void * WorkThreadFunc(void* args);

class Thread_pool
{
private:
public:
    Thread_pool();
    ~Thread_pool();
    int Init(int sem_key);
    int semaphore_p();
    int semaphore_v();
    void JoinAllThread();
    void CancelAllThread();
    void AddThreads(std::string type,int num);//添加服务线程
    void AddWorkThreads(int num);//添加工作线程
    void AddFunc(std::string type,pFunc func);
    void PushTask(Task* task);
    Task* PopTask();
    int GetTaskNum();
    void Run();
    static Thread_pool* GetInstance();
public:
    std::vector<Thread> thread_vec;
    std::map<std::string,pFunc> funcs_map;
    Task_pool task_pool;
    bool thread_pool_close;
    static Thread_pool* g_self;
    std::atomic<int>   work_num;
private:
    int sem_id;//信号量id
};