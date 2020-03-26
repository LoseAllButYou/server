#include "thread_pool.h"
#include "../loger.h"

#include <sys/sem.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "define.h"
using namespace std;

Thread_pool* Thread_pool ::g_self = NULL;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_data_mutex = PTHREAD_MUTEX_INITIALIZER;

union semun
{
	int val;
	struct semid_ds *buf;
	unsigned short *arry;
};

void * WorkThreadFunc(void* args)
{
    while( !G_Thread_pool->thread_pool_close)
    {
        G_Thread_pool->Run();
    }
    return args;
}

Thread_pool::Thread_pool()
    :thread_pool_close(false)
    ,sem_id(0),work_num(0)
{
    
}

int Thread_pool::Init(int sem_key)
{
    sem_id=semget((key_t)sem_key, 1, 0666 | IPC_CREAT);
    union semun sem_union;
	sem_union.val = 1;

    if(sem_id == -1||semctl(sem_id, 0, SETVAL,sem_union) == -1)
    {
		LOG_ERROR("init semget err! id : %d",sem_id);
        return -1;
    }
    LOG_SHOW("init semget success id : %d",sem_id);
    return 0;
}

Thread_pool::~Thread_pool()
{
    if(semctl(sem_id, 0, IPC_RMID) == -1)
    {
        LOG_ERROR("init semget err! id : %d",sem_id);
    }
    pthread_mutex_destroy(&m_mutex);
}

int Thread_pool::semaphore_p()
{
	//对信号量做减1操作，即等待P（sv）
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = -1;//P()
	sem_b.sem_flg = SEM_UNDO;
	if(semop(sem_id, &sem_b, 1) == -1)
	{
		LOG_ERROR("semaphore_p err! id : %d",sem_id);
		return -1;
	}
	return 0;
}
 
int Thread_pool::semaphore_v()
{
	//这是一个释放操作，它使信号量变为可用，即发送信号V（sv）
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = 1;//V()
	sem_b.sem_flg = SEM_UNDO;
	if(semop(sem_id, &sem_b, 1) == -1)
	{
		LOG_ERROR("semaphore_v err! id : %d",sem_id);
		return  -1;
	}
	return 0;
}

void Thread_pool::JoinAllThread()
{
    for(int i = 0; i<(int) thread_vec.size();++i)
    {
        thread_vec[i].ThreadJoin();
    }
}

void Thread_pool::CancelAllThread()
{
    for(int i = 0; i<(int) thread_vec.size();++i)
    {
        LOG_DBG("thread cancel tid : %d ret :%d",thread_vec[i].thread_id, thread_vec[i].ThreadCancel());
    }
}

void Thread_pool::AddThreads(string type,int num)
{
    if(funcs_map.find(type) == funcs_map.end() || num <= 0)
    {
        LOG_ERROR("err type : %s or num : %d",type.c_str(),num);
        return;
    } 
    for (int i = 0; i<num ;++i)
    {
        thread_vec.push_back(Thread(type,funcs_map[type]));
    }
    LOG_DBG("add thread success ! type : %s  num : %d",type.c_str(),num);
}

//添加工作线程
void Thread_pool::AddWorkThreads(int num)
{
    work_num+= num;
    if(num <= 0)
    {
        return;
    }
    for (int i = 0; i<num ;++i)
    {
        thread_vec.push_back(Thread("worker",WorkThreadFunc));
    }
    LOG_DBG("add thread success ! type : %s  num : %d","worker",num);
}

void Thread_pool::Run()
{
    std::atomic_thread_fence(memory_order_seq_cst);
    atomic< Task*> p_task(PopTask());
    if(atomic_load (&p_task)!=NULL)
    {
        LOG_SHOW("11111 begain work thread id  = % u p_task %x task_id:%d",pthread_self(),
        atomic_load (&p_task),atomic_load (&p_task)->task_id );
        --work_num;
        if(atomic_load (&p_task)->GetTaskID() == TASK_DATA_CLOSE)
        {
            LOG_DBG("clear datahandle");    
            atomic_load (&p_task)->Clear();
        }
        else
        {
            atomic_load (&p_task)->Run(NULL);
        }
        ++work_num;
        LOG_SHOW("22222 over work thread id  = % u p_task %x task_id:%d",pthread_self(),
        atomic_load (&p_task),atomic_load (&p_task)->task_id );
    }
}

void Thread_pool:: AddFunc(string type,pFunc func)
{
    funcs_map[type] = func;
    printf("[SUCCESS] %s|%s|%d: add func success :%s\n",__FILE__,__func__, __LINE__,type.c_str());
}

void Thread_pool::PushTask(Task* task)
{
    pthread_mutex_lock(&m_data_mutex);
    task_pool.Push(task);
    pthread_mutex_unlock(&m_data_mutex);
    //LOG_DBG("add task! all size %d task id %d,",GetTaskNum(),task ==NULL ? NULL:task->task_id);
    semaphore_v();
}

Task* Thread_pool::PopTask()
{
    if(semaphore_p() == 0)
    {
        pthread_mutex_lock(&m_data_mutex);
        std::atomic<Task*> task(task_pool.Pop());
        pthread_mutex_unlock(&m_data_mutex);
        LOG_DBG("pop task! size %d",task_pool.GetSize());
        return atomic_load(&task);
    }
    return NULL;
}

int  Thread_pool::GetTaskNum()
{
    return task_pool.GetSize();
}

Thread_pool* Thread_pool:: GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new Thread_pool();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}