#pragma once

#include <iostream>

#include <string>
#include <string.h>

#include <stdio.h>
#include <pthread.h>

typedef void*(* pFunc)(void *);

class Thread
{
private:
    /* data */
public:
    Thread(std::string type,pFunc func);
    ~Thread();
    int ThreadCancel();
    int ThreadJoin();
public:
    std::string thread_type;
    pthread_t  thread_id;
};

