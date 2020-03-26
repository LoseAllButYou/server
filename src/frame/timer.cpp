#include "timer.h"
#include "loger.h"
#include "thread_pool/define.h"

using namespace std;

TimerManager * TimerManager:: g_self = NULL;
static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_data_mutex = PTHREAD_MUTEX_INITIALIZER;

void GeneralSleep(unsigned int millisecond)
{
#ifdef WIN32
    ::Sleep(millisecond);
#else
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    tv.tv_sec = millisecond / 1000;
    tv.tv_usec = (millisecond % 1000) * 1000;
    select(0, NULL, NULL, NULL, &tv);
#endif
}

// class Timer---------------------------------------------------------------------------

Timer::Timer(std::string name,int id,long  msec,Task* owner)
    :timer_id(id)
    ,timer_msec(msec),timer_owner(owner)
{
    timer_name  = name;

}

Timer::~Timer()
{
    std::string().swap(timer_name);
    timer_begin.tv_sec = 0;
    timer_begin.tv_usec = 0;
}

void Timer::StartTimer()
{
    gettimeofday(&timer_begin,NULL);
    LOG_DBG("start time %ld ",timer_begin.tv_sec);
    
}

bool Timer::operator > (const Timer & other) const
{
    return timer_msec > other.timer_msec;
}

// class TimerOver---------------------------------------------------------------------------

TimerOver::TimerOver(std::string name,int id,Task* owner)
    :Task(TASK_TIME_OVER,"timer_over"),timer_name(name),timer_id(id)
    ,timer_owner(owner)
{}

TimerOver::~TimerOver()
{
    timer_owner = NULL;
}

void TimerOver:: Run(void * args)
{
    if(timer_owner)
    {
        LOG_DBG("timer_over  %s , %d",timer_name.c_str(),timer_id);
        timer_owner->Run(this);
    }
}
void TimerOver::Debug()
{
    LOG_DBG("timer_over  %s , %d",timer_name.c_str(),timer_id);
}

// class TimerManager---------------------------------------------------------------------------

void * TimerThreadFunc(void* args)
{
    G_TIMER->Run(NULL);
    return args;
}


TimerManager::TimerManager(/* args */)
    :Task(TASK_TIMER,"timer"), timer_close(false)
{
    //条件变量的初始化操作
    pthread_cond_init(&m_cond,NULL);
    InitHeap();
    LOG_DBG("init timer manager success!");
}

TimerManager::~TimerManager()
{
    //互斥变量使用完成之后必须进行destroy操作
    pthread_mutex_destroy(&m_mutex);
    pthread_mutex_destroy(&m_data_mutex);
    //条件变量使用完成之后进行destroy操作
    pthread_cond_destroy(&m_cond);
}

void TimerManager::InitHeap()
{
    make_heap(timer_heap.begin(), timer_heap.end(), greater<Timer>());
}

void TimerManager::TraverseHeap()
{
    for(int i = 0; i<(int)timer_heap.size();++i)
    {
        timeval now ;
        gettimeofday(&now,NULL);
        if((now.tv_sec-timer_heap[i].timer_begin.tv_sec )*1000 + ((now.tv_usec/1000) -(timer_heap[i].timer_begin.tv_usec/1000))
            >= timer_heap[i].timer_msec)
        {
            LOG_DBG("now sec %ld, usec %ld,begin sec %ld, usec %ld",now.tv_sec,now.tv_usec,timer_heap[i].timer_begin.tv_sec,
            timer_heap[i].timer_begin.tv_usec);

            TimerOut(timer_heap[i]);
            pop_heap(timer_heap.begin(), timer_heap.end(), greater<Timer>());
            timer_heap.pop_back();
        }
        else
        {
            break;
        }
    }
}

void TimerManager::Run(void * args)
{
    while(!timer_close)
    {
        pthread_mutex_lock(&m_mutex);
        if(timer_heap.size() == 0)
        {
            //基于条件变量阻塞，无条件等待
            pthread_cond_wait(&m_cond,&m_mutex) ;
        }
        pthread_mutex_unlock(&m_mutex); 
        pthread_mutex_lock(&m_data_mutex);
        TraverseHeap();
        pthread_mutex_unlock(&m_data_mutex); 
        GeneralSleep(10);//10毫秒休眠一次
    }
}

void TimerManager::Debug()
{
    LOG_DBG("timerManager ！！！");
}

int TimerManager::AddTimer(string name,int id,long  msec ,Task * owner)
{
    pthread_mutex_lock(&m_data_mutex);
    Timer timer(name,id,msec,owner) ;
    timer.StartTimer(); 
    timer_heap.push_back(timer);
    push_heap(timer_heap.begin(), timer_heap.end(), greater<Timer>());
    //激活一个等待条件成立的线程
    pthread_mutex_unlock(&m_data_mutex); 
    pthread_cond_signal(&m_cond);
    LOG_DBG("add timer name: %s,id :%d,msec:%llu allsize %d,",name.c_str(),id,msec,timer_heap.size());
    return 0;
}

//移除定时器
bool TimerManager::RemoveTimer(Task* owner,int id)
{
    pthread_mutex_lock(&m_data_mutex);

    for(int i = 0; i<(int)timer_heap.size();++i)
    {
        if(timer_heap[i].timer_owner == owner && id==timer_heap[i].timer_id )
        {
            timer_heap.erase(timer_heap.begin()+i);
            LOG_DBG("remove timer %d",id);
            break;
        }
    }
    InitHeap();
    pthread_mutex_unlock(&m_data_mutex); 
    return 0;
}

//定时器时间到
int  TimerManager::TimerOut(Timer & timer)
{
    TimerOver * over = new TimerOver(timer.timer_name,timer.timer_id,timer.timer_owner);
    // 添加到任务队列
    LOG_DBG("timer out name: %s,id :%d,msec:%llu onwor %x",
        timer.timer_name.c_str(),timer.timer_id,timer.timer_msec,timer.timer_owner);
    G_Thread_pool ->PushTask(over);
    return 0;
}

TimerManager* TimerManager::GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new TimerManager();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}
