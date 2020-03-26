#include "task_queue.h"
#include <stdio.h>

Task_pool::Task_pool()
{
    pthread_mutex_init(&m_mutex, NULL);
}

Task_pool::~Task_pool()
{
    pthread_mutex_destroy(&m_mutex);
}

void Task_pool::Debug()
{
    printf("[DEBUG] task_queue\n");
}

Task* Task_pool::Pop()
{
    //pthread_mutex_lock(&m_mutex);
    Task* tmp = task_queue.front();
    if(tmp)
    {
        task_queue.pop();
    }
   // pthread_mutex_unlock(&m_mutex);
    return tmp;
}

void Task_pool::Push(Task* task)
{
    //pthread_mutex_lock(&m_mutex);
    task_queue.push(task);
   // pthread_mutex_unlock(&m_mutex);
}

int Task_pool::GetSize()
{
    return task_queue.size();
}