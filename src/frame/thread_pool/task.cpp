#include "task.h"
#include "stdio.h"


void Task::Debug()
{
    printf("[ base_task ] id : %d , name : %s \n",task_id,task_name.c_str());
}
