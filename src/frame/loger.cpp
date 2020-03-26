#include "loger.h"
#include <ctime>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include "thread_pool/define.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "timer.h"
#include <errno.h>
#include <unistd.h>
using namespace std;

Loger* Loger:: g_self = NULL;

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;

Loger::Loger()
    :Task(TASK_LOGER,"logger"),log_path(""),log_size(0)
    ,log_max_size(0),log_name("")
    ,log_fd(-1),close_loger(false)
{
}

Loger::~Loger()
{
    pthread_mutex_destroy(&m_mutex);

    if(log_fd != -1 &&close(log_fd)==-1)
    {
        printf("[ERROR] %s|%s|%d: close log file err fd : %d!\n",__FILE__,__func__, __LINE__,log_fd);
    }
}

void Loger::Start(string  path ,string name,int size  )
{
    log_path = path;
    log_max_size = size;
    log_name = name;
    if(ChangeLogFile())
    {
        printf("[SUCCESS] %s|%s|%d: create log file success !\n",__FILE__,__func__, __LINE__);
    }
}

bool  Loger::JudgeFileFull()
{
    if(log_size>log_max_size)
    {
        return true;  
    }
    return false;
}

bool  Loger::ChangeLogFile()
{
    if(log_path=="" && log_fd==-1)
    {
        return true;
    }
    if(log_path!="./")
    {
        if(0 != access(log_path.c_str(), 0))
        {
            if(mkdir(log_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)==-1)
            {
                printf("[ERROR] creat dir err %s %d errno : %d!\n",log_path.c_str(),errno);
                return false;
            }
        }
    }

    if(log_fd != -1 && close(log_fd)==-1)
    {
        printf("[ERROR] %s|%s|%d: close log file err fd: %d errno : %d!\n",__FILE__,__func__, __LINE__,log_fd,errno);
        log_fd = -1;
    }

    time_t t = time(0);
    char buffer[24] = {0};
    strftime(buffer, 24, "%Y-%m-%d-%H-%M-%S", localtime(&t));
    string name =  log_name+buffer;
    log_size = 0;
    if((log_fd=open((log_path+ name).c_str(),  O_RDWR | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR))==-1)
    {
        printf("[ERROR] %s|%s|%d: creat log file err ret : %d! path :%s\n",__FILE__,__func__, __LINE__,log_fd
        ,(log_path+ name).c_str());
        return false;
    }
    return true;
}

void Loger::PrintLog(string format,va_list args)
{
    if (log_fd == -1)
    {
        vfprintf(stderr,format.c_str(), args  );
    }
    else
    {
        pthread_mutex_lock(&m_mutex);
        char buffer[1024*10] ={0};
        int ret =  vsprintf(buffer,format.c_str(), args  );

        log_queue.push(buffer);
        pthread_mutex_unlock(&m_mutex);
    }
}

void Loger::WriteLog()
{
    pthread_mutex_lock(&m_mutex);

    if(JudgeFileFull()&&!ChangeLogFile())
    {
        pthread_mutex_unlock(&m_mutex);
        return ;
    }
    
    if(log_queue.size()==0)
    {
        pthread_mutex_unlock(&m_mutex);
        GeneralSleep(100);//没有信息 休眠500毫秒
        return;
    }
    string tmp =log_queue.front();
    log_queue.pop();

    if(write(log_fd,tmp.c_str(),tmp.size())==-1)
    {
            // nothing
    }
    log_size +=tmp.size();
    pthread_mutex_unlock(&m_mutex);

}

void Loger::Debug(string format, ...)
{
    va_list args ;
    va_start(args , format);
    Log(format,"[DEBUG]", args);
    va_end( args);
}

void Loger::Error(string format, ...)
{
    va_list args ;
    va_start(args , format);
    Log(format,"[ERROR]", args);
    va_end( args);
}

void Loger::Warn(string format, ...)
{
    va_list args ;
    va_start(args , format);
    Log(format,"[WARN]", args);
    va_end( args);
}

void Loger::Show(string format, ...)
{
    va_list args ;
    va_start(args , format);
    Log(format,"[SHOW]", args);
    va_end( args);
}

void Loger::Log(string format,string level, va_list args)
{
    time_t t = time(0);
    char now[9] = {0};
    strftime(now, 9, "%H:%M:%S", localtime(&t));
    format = level+now+"|%s|%s()|%d:"+format+"\n";
    PrintLog(format,args );
}

void Loger::Run(void * args)
{
    while(!close_loger)
    {
        WriteLog();
    }
}

Loger* Loger::GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new Loger();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}

void * LogThreadFunc(void* args)
{
    LOGER->Run(NULL);
    return args;
}