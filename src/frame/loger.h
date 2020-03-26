#pragma once
#include <iostream>

#include <string>

#include <string.h>
#include <stdio.h>
#include "thread_pool/task.h"
#include "timer.h"
#include <pthread.h>
#include <queue>
#include <stdarg.h>

#define LOGER Loger::GetInstance()
#define LOG_DBG(format,...) Loger::GetInstance()->Debug(format,__FILE__,__func__,__LINE__,##__VA_ARGS__)
#define LOG_WARN(format,...) Loger::GetInstance()->Warn(format,__FILE__,__func__,__LINE__,##__VA_ARGS__)
#define LOG_ERROR(format,...) Loger::GetInstance()->Error(format,__FILE__,__func__,__LINE__,##__VA_ARGS__)
#define LOG_SHOW(format,...) Loger::GetInstance()->Show(format,__FILE__,__func__,__LINE__,##__VA_ARGS__)

void * LogThreadFunc(void* args);

class Loger :public Task
{
private:
    /* data */
    void PrintLog(std::string format,va_list args);
    bool JudgeFileFull();
    bool ChangeLogFile();
public:
    Loger();
    ~Loger();
    void Debug(std::string  format, ...);
    void Error(std::string format, ...);
    void Warn(std::string format, ...);
    void Show(std::string format, ...);
    void Log(std::string format, std::string level ,va_list args);
    void WriteLog();
    void Start(std::string  path = "./",std::string name ="loger",int size = 1024*1024*100);
    virtual void Run(void * args);
    virtual void Debug(){}
    virtual void Clear(){}
    static Loger* GetInstance();
public:
    std::string log_name;
    std::string log_path;
    int log_max_size;
    int log_size;
    bool close_loger;
    static Loger* g_self;
private:
    int log_fd;
    std::queue<std::string> log_queue;
};