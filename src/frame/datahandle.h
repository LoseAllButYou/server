#pragma once
#include <iostream>
#include "thread_pool/task.h"
#include "thread_pool/define.h"
#include "http.h"
#include "websocket.h"
#include<atomic>

enum PROTOCOL_TYPE
{
    INIT      = 0,
    UDP       = 1,
    TCP       = 2,
    WEBSOCKET = 3,
    HTTP      = 4,
    HTTPS     = 5,
};

class DataHandle : public Task
{
private:
    /* data */
    void ReadData();
    void ReadHttp();
    void ReadWebSocket();
    void WriteData();
    void CloseEvent();
public:
    DataHandle(int fd,PROTOCOL_TYPE protocol_type = INIT);
    ~DataHandle();
    virtual void Debug(){};
    virtual void Run(void * args);
    void SetEvent(int event);
    int Event();
    void DelEvent(int event);
    void SendPackage(std::string & package );
    void AddWriteData(const unsigned char * buf ,int size);
    const unsigned char *  GetReadData(int& size);
    int GetFD() {return _fd;}
    int GetWriteBufSize(){return write_buf_size;}
    bool NeedWrite(){return write_size>0&&write_buffer;}
    void SetProtocolType(PROTOCOL_TYPE protocol_type){_protocol_type = protocol_type;};
    PROTOCOL_TYPE GetProtocolType(){return _protocol_type;};
    Httpd* GetHttpd();
    WebSocket* GetWebSocket();
    virtual void Clear(){ CloseEvent();}
    int GetTaskNum();
    void SetTaskClose();
    void AddToTreadPool();

public:
    static Task *   work_class;
    PROTOCOL_TYPE   _protocol_type;
    Httpd  *        _httpd;
    WebSocket *     _ws;
    std::atomic<int> task_num;
private:
    int             data_event;
    int             _fd;
    unsigned char * write_buffer;
    int             write_buf_size;
    int             write_size;
    unsigned char * read_buffer;
    int             read_buf_size;
    int             read_size;
    mutable pthread_mutex_t m_data_mutex;//数据锁
    mutable pthread_mutex_t m_event_mutex;//状态锁
    mutable pthread_mutex_t m_write_mutex;//写数据锁
    mutable pthread_mutex_t m_cond_mutex;//析构锁
    mutable pthread_mutex_t m_count_mutex;//线程引用计数锁
};

