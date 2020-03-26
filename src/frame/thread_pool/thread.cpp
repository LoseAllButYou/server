#include "thread.h"
#include "../loger.h"

using namespace std;

Thread::Thread(string type,pFunc p_func)
    :thread_type (type)
{
    if(0<pthread_create(&thread_id, NULL, p_func, NULL ))
    {
        LOG_ERROR("Creat Thread err !");
    }
    else
    {
        LOG_DBG("Creat Thread success id : %u!",thread_id);
    }
}

Thread::~Thread()
{}

int Thread::ThreadCancel()
{
    return pthread_cancel(thread_id);
}

int Thread::ThreadJoin()
{
    return pthread_join(thread_id, NULL);
}

