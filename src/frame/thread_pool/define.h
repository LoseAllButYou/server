#pragma once

enum TaskID
{
    TASK_DEFAULT     = 0,
    TASK_LOGER       = 1,
    TASK_TIMER       = 2,
    TASK_TIME_OVER   = 3,
    TASK_NETWORK     = 4,
    TASK_DATA_HANDLE = 5,
    TASK_REDIS_CLIENT= 6,
    TASK_REDIS_REQ   = 7,
    TASK_REDIS_RES   = 8,
    TASK_DATA_CLOSE  = 9,

};

//数据事件
enum DataHandleEvent
{
    DATA_INIT  = 0,
    DATA_CONNECT = 1,
    DATA_READ  = 2,
    DATA_WRITE = 4,
    DATA_CLOSE = 8,
};
