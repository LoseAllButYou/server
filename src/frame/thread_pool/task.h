#pragma once

#include <iostream>
#include <stdio.h>

#include <string>

class Task
{
private:

public:
    virtual void Debug() ;
    virtual void Run(void * args) = 0;
    virtual void Clear()= 0 ;
public:
    Task(int id , std::string  name): task_id(id),task_name(name) {
        pthread_mutex_init(&m_task_mutex,NULL);
    };
    virtual ~Task(){
        pthread_mutex_destroy(&m_task_mutex);
    };
    void SetTaskID(int id)
    {
        pthread_mutex_lock(&m_task_mutex);
        task_id = id;
        pthread_mutex_unlock(&m_task_mutex);
    }

    int GetTaskID()
    {
        pthread_mutex_lock(&m_task_mutex);
        int id=task_id;
        pthread_mutex_unlock(&m_task_mutex);
        return id;
    }
//共有成员变量
public:
    int task_id;
    std::string  task_name;
    mutable pthread_mutex_t m_task_mutex;//数据锁
};