#include "datahandle.h"
#include "network.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "timer.h"
#include "loger.h"
#include <errno.h>
#include <string.h>

using namespace std;

Task* DataHandle::work_class =NULL;

DataHandle::DataHandle( int fd,PROTOCOL_TYPE protocol_type )
    :Task(TASK_DATA_HANDLE,"datahandle")
    ,_protocol_type(protocol_type),_httpd(NULL),_ws(NULL)
    ,data_event(DATA_INIT),_fd(fd),write_buffer (NULL),write_buf_size(0)
    ,read_buffer(NULL),read_buf_size(0),read_size(0),write_size(0),task_num(0)
{
    pthread_mutex_init(&m_event_mutex,NULL);
    pthread_mutex_init(&m_data_mutex,NULL);
    pthread_mutex_init(&m_write_mutex,NULL);
    pthread_mutex_init(&m_cond_mutex,NULL);
    pthread_mutex_init(&m_count_mutex,NULL);
}

DataHandle::~DataHandle()
{
    LOG_DBG("delete self %d",_fd);
    pthread_mutex_destroy(&m_event_mutex);
    pthread_mutex_destroy(&m_data_mutex);
    pthread_mutex_destroy(&m_write_mutex);
    pthread_mutex_destroy(&m_cond_mutex);
    pthread_mutex_destroy(&m_count_mutex);
    if(write_buffer)
    {
        delete[] write_buffer;
        write_buffer = NULL;
    }
    if(read_buffer)
    {
        delete[] read_buffer;
        read_buffer = NULL;
    }
    if(_httpd)
    {
        delete _httpd;
        _httpd = NULL;
    }
    if(_ws)
    {
        delete _ws;
        _ws = NULL;
    }
}

void DataHandle::Run(void * args)
{
    LOG_DBG("--- event : %d fd:%d task num : %d",Event(),_fd,GetTaskNum());
    if(GetTaskID()==TASK_DATA_CLOSE)
    {
        --task_num;
        return;
    }
    if(Event() == DATA_INIT)
    {
        --task_num;
        return;
    }
    if(pthread_mutex_trylock(&m_data_mutex) == 0)
    {
        if(Event() & DATA_CONNECT && !(Event() & DATA_CLOSE) )
        {
            //处理业务逻辑
            LOG_DBG("--- event : conn");
            work_class->Run(this);
            DelEvent(DATA_CONNECT);
        }

        if(Event() & DATA_READ && !(Event() & DATA_CLOSE) )
        {
            LOG_DBG("--- event : read protocol type:%d",_protocol_type);
            if(_protocol_type == HTTP || _protocol_type == HTTPS )
            {
                ReadHttp();
            }
            else if(_protocol_type == WEBSOCKET)
            {
                ReadWebSocket();
                //ReadData();
            }
            else
            {
                ReadData();
            }
            
        }
        
        if(Event() & DATA_WRITE && NeedWrite() && !(Event() & DATA_CLOSE))
        {
            LOG_DBG("--- event : write");
            pthread_mutex_lock(&m_write_mutex);
            WriteData();
            pthread_mutex_unlock(&m_write_mutex);

        }
        pthread_mutex_unlock(&m_data_mutex);
    }
    --task_num;
    LOG_DBG("woker thread exit fd:%d",_fd);
}

void DataHandle::SendPackage(std::string & package )
{
    pthread_mutex_lock(&m_write_mutex);
    if(_protocol_type == TCP || _protocol_type == UDP)
        {
        int size  = package.size();
        unsigned char* buf = new unsigned char[size+sizeof(int)];
        if(buf == NULL)
        {
            LOG_ERROR("new buf error! errno: %d",errno);
            pthread_mutex_unlock(&m_write_mutex);
            return;
        }
        memcpy(buf,&size,sizeof(int));
        memcpy(buf+sizeof(int),package.data(),size);
        AddWriteData(buf,size+sizeof(int));
        delete buf;
    }
    else
    {
        AddWriteData((unsigned char*)package.data(),package.length());
    }
    pthread_mutex_unlock(&m_write_mutex);
}
void DataHandle::ReadWebSocket()
{
    if(GetWebSocket()==NULL)
    {
        SetEvent(DATA_CLOSE);
        return;
    }
    if(_ws->ReadWebSocket(_fd)<0)
    {
        return;
    }
    if(_ws->_read_state==WS_OVER&&_ws->_frame_type == WS_BINARY_FRAME && work_class)
    {
        work_class->Run(this);
    }
    int ret = 0;
    int size=0;
    if(Event()&DATA_READ && !(Event()&DATA_CLOSE ))
    {
        if((ret = recv(_fd,&size,1,MSG_PEEK))==0)
        {
            DelEvent(DATA_READ);
            SetEvent(DATA_CLOSE);
        }
        else if(ret > 0 )
        {
            SetEvent(DATA_READ);
            ++task_num;
            AddToTreadPool();
            return ;
        }
        else
        {
            //被中断 再判断一次是否需要读
            if(ret == EINTR)
            {
                if((ret = recv(_fd,&size,1,MSG_PEEK))>0)
                {
                    SetEvent(DATA_READ);
                    ++task_num;
                    AddToTreadPool();
                    return ;
                }
                else
                {
                    DelEvent(DATA_READ);
                    return;
                }   
            }
            DelEvent(DATA_READ);
        }
    }
}

void DataHandle::ReadHttp()
{
    if(GetHttpd()==NULL)
        return;

    if(_httpd->RecvRequest(_fd)<0)
    {
        LOG_DBG("recv http err! fd %d",_fd);
        _httpd->Clear();
        SetEvent(DATA_CLOSE);
        return;
    }
    if(work_class)
    {
        work_class->Run(this);
    }
    int ret = 0;
    int size=0;
    if(Event()&DATA_READ && !(Event()&DATA_CLOSE ))
    {
        if((ret = recv(_fd,&size,1,MSG_PEEK))==0)
        {
            DelEvent(DATA_READ);
            SetEvent(DATA_CLOSE);
        }
        else if(ret > 0 )
        {
            SetEvent(DATA_READ);
            ++task_num;
            AddToTreadPool();
            return ;
        }
        else
        {
            //被中断 再判断一次是否需要读
            if(ret == EINTR)
            {
                if((ret = recv(_fd,&size,1,MSG_PEEK))>0)
                {
                    SetEvent(DATA_READ);
                    ++task_num;
                    AddToTreadPool();
                    return ;
                }
                else
                {
                    DelEvent(DATA_READ);
                    return;
                }
                
            }
            DelEvent(DATA_READ);
        }
    }
}

void DataHandle::ReadData()
{
    int all_size = 0;
    int size = 0;
    //先读长度4个字节
    while(true)
    {
        int ret = 0;
        ret =recv(_fd,&all_size+size,sizeof(int)-size,0);
        if(ret == 0)
        {
            LOG_DBG("_fd : %d close!",_fd);
            DelEvent(DATA_READ);
            SetEvent(DATA_CLOSE);
            return;
        }
        else if(ret<0)
        {
            if(errno == EAGAIN )
            {
                DelEvent(DATA_READ);
                return ;
            }
            if(errno == EINTR)
            {
                LOG_DBG("_fd : %d close!errno : %d  std %s ",_fd,errno,strerror(errno));
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",_fd,errno);
                DelEvent(DATA_READ);
                SetEvent(DATA_CLOSE);
                return;
            }
        }

        size+=ret;
        if(size == sizeof(int))
        {
            break;
        }
    }
    size = 0;
    if(read_buf_size>=all_size )
    {
        memset(read_buffer,0,read_buf_size);
    }
    else
    {
        if(read_buffer)
        {
            delete[] read_buffer;
            read_buf_size = all_size;
        }
        read_buffer = new unsigned char[all_size];
    }
    LOG_DBG("read size %d",all_size);
    read_size = 0;
    while(true)
    {
        LOG_DBG("++++READ");

        int ret = 0;
        ret= recv(_fd,read_buffer+size,all_size - size,0);
        if(ret == 0)
        {
            LOG_DBG("_fd : %d close!  std %s ",_fd,strerror(errno));
            DelEvent(DATA_READ);
            SetEvent(DATA_CLOSE);
            break;
        }
        if(ret<0)
        {
            if(errno == EINTR||errno == EAGAIN)
            {
                LOG_ERROR("read err fd : %d errno : %d",_fd,errno);
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",_fd,errno);
                DelEvent(DATA_READ);
                SetEvent(DATA_CLOSE);
                break;
            }
        }
        size += ret;
        if(all_size == size)
        {
            read_size = all_size;
            //调试打印日志--交付完整数据包
            if(work_class)
            {
                work_class->Run(this);
            }
            ret = -10000;
            if(Event()&DATA_READ)
            {
                if((ret = recv(_fd,&size,1,MSG_PEEK))==0)
                {
                    DelEvent(DATA_READ);
                    SetEvent(DATA_CLOSE);
                }
                else if(ret > 0 )
                {
                    SetEvent(DATA_READ);
                    ++task_num;
                    AddToTreadPool();
                    return ;
                }
                else
                {
                    //被中断 再判断一次是否需要读
                    if(ret == EINTR)
                    {
                        if((ret = recv(_fd,&size,1,MSG_PEEK))>0)
                        {
                            SetEvent(DATA_READ);
                            ++task_num;
                            AddToTreadPool();
                            return ;
                        }
                        else
                        {
                            DelEvent(DATA_READ);
                            return;
                        }    
                    }
                    DelEvent(DATA_READ);
                }
            }
            break;
        }
    }
}

void DataHandle::WriteData()
{
    if(!write_buffer||write_size == 0)
    {
        if(((!_httpd->HaveKey("Connection"))||(_httpd->GetValueByKey("Connection")=="close"))
            &&(_protocol_type ==HTTP ||_protocol_type ==HTTPS))
        {
            LOG_DBG("shutdown read %d %s",_httpd->HaveKey("Connection"),_httpd->GetValueByKey("Connection").c_str());
            // SetEvent(DATA_CLOSE);
            // G_NETWORK-> ClearDataHandle(_fd);
            shutdown(_fd, SHUT_RD);
        }
        LOG_ERROR(" error write buf!");
        return;
    }
    int size = 0;
    while(true)
    {
        int ret= write(_fd,write_buffer+size,write_size-size);
        if(ret == 0)
        {
            LOG_DBG("_fd : %d close!",_fd);
            SetEvent(DATA_CLOSE);
            break;
        }
        if(ret<0)
        {
            if(errno == EAGAIN || errno == EINTR)
            {
                //LOG_DBG("_fd : %d again!",_fd);
                continue;
            }
            else
            {
                LOG_ERROR("write err fd : %d errno : %d",_fd,errno);
                SetEvent(DATA_CLOSE);
                break;
            }
        }
        size += ret;
        if(size == write_size)
        {
            LOG_DBG("_fd : %d write over! size%d",_fd,size);
            write_size = 0;
            break;
        }
    }
    if(((!_httpd->HaveKey("Connection"))||(_httpd->GetValueByKey("Connection")=="close"))
            &&(_protocol_type ==HTTP ||_protocol_type ==HTTPS))
    {
        LOG_DBG("shutdown read %d %s",_httpd->HaveKey("Connection"),_httpd->GetValueByKey("Connection").c_str());
        // SetEvent(DATA_CLOSE);
        // G_NETWORK-> ClearDataHandle(_fd);
        shutdown(_fd, SHUT_RD);
    }
}

void DataHandle::CloseEvent()
{
    if(GetTaskNum()==1)
    {
        if(work_class)
        {
           work_class->Run(this);
        }
        G_NETWORK-> DelSocketFd(_fd);
        return;
    }
    if(pthread_mutex_trylock(&m_cond_mutex)==0)
    {
        LOG_DBG("1111111111task num %d fd:%d",GetTaskNum(),_fd);
        if(GetTaskNum()>1)
        {
            AddToTreadPool();
        }
        else
        {
            pthread_mutex_unlock(&m_cond_mutex);
            if(work_class)
            {
                work_class->Run(this);
            }
            G_NETWORK-> DelSocketFd(_fd);  
            return;   
        }
        pthread_mutex_unlock(&m_cond_mutex);
        return;
    }
    --task_num;
    //Lsk_num;OG_DBG("22222222task num %d",GetTaskNum());
    return;
}

void DataHandle::SetEvent(int event)
{
    pthread_mutex_lock(&m_event_mutex);
    data_event = data_event|event;
    pthread_mutex_unlock(&m_event_mutex); 
}

int DataHandle::Event()
{
    pthread_mutex_lock(&m_event_mutex);
    int event = data_event;
    pthread_mutex_unlock(&m_event_mutex); 
    return event;
}

void DataHandle::DelEvent(int event)
{
    pthread_mutex_lock(&m_event_mutex);
    if(data_event&event)
    {
        data_event-=event;
    }
    pthread_mutex_unlock(&m_event_mutex); 
}

void DataHandle::AddWriteData(const unsigned char *buf ,int size)
{
    if(!buf||size==0)
    {
        return ;
    }
    if((write_buffer && write_buf_size-write_size<size)||!write_buffer)
    {
        LOG_DBG("all %d size %d buf_size %d",write_buf_size,size,write_size);
        unsigned char * tmp = new  unsigned char[write_size + size];
        if(write_buffer&& write_size>0)
        {
                memcpy(tmp,write_buffer,write_size);
        }
        memcpy(tmp+write_size,buf,size);
        delete[] write_buffer;
        write_buffer = tmp;
        write_buf_size =write_size + size;
        write_size +=size; 
    }
    else
    {
        memcpy(write_buffer+write_size,buf,size);
        write_size+=size;
    }
    if(Event()&DATA_WRITE)
    {
        WriteData();
    }
    // else
    // {
    //     AddToTreadPool();
    // }
    
}

const unsigned char *  DataHandle::GetReadData(int& size)
{
    if(read_size == 0)
    {
        return NULL;
    }
    size = read_size;
    read_size = 0;
    return read_buffer;
}

void DataHandle::AddToTreadPool()
{
    pthread_mutex_lock(&m_count_mutex);
    G_Thread_pool->PushTask(this);
    pthread_mutex_unlock(&m_count_mutex);
}

int DataHandle::GetTaskNum()
{
    //pthread_mutex_lock(&m_count_mutex);
    //pthread_mutex_unlock(&m_count_mutex);
    //std::atomic_thread_fence(memory_order_seq_cst);

    return std::atomic_load(&task_num);
}

void DataHandle::SetTaskClose()
{
    if(this)
        SetTaskID(TASK_DATA_CLOSE);
}

Httpd* DataHandle::GetHttpd()
{
    if(_httpd == NULL)
    {
        _httpd = new Httpd;
        if(_httpd == NULL)
        {
            LOG_ERROR("new httpd err");
            return NULL;
        }
    }
    return _httpd;
}

WebSocket* DataHandle::GetWebSocket()
{
    if(_ws == NULL)
    {
        _ws = new WebSocket;
        if(_ws == NULL)
        {
            LOG_ERROR("new httpd err");
            return NULL;
        }
    }
    return _ws;
}


