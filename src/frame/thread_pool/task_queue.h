#pragma once

#include <iostream>
#include <stdio.h>

#include <string>
#include <queue>
#include "task.h"

class Task_pool
{
public:
    Task_pool();
    ~Task_pool();
    void Debug();
    Task* Pop();
    void Push(Task* task);
    int GetSize();
private:
    mutable pthread_mutex_t m_mutex;
public:
    std::queue<Task*> task_queue;
};