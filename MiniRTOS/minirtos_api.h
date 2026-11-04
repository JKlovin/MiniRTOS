#ifndef MINIRTOS_API_H
#define MINIRTOS_API_H

#include "minirtos_task.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>




void MiniRTOS_Init(void);
void MiniCreateTask( TaskFunction_t pxTaskCode,
                     const char * const pcName,
                     const uint32_t uxStackDepth,
                     unsigned long uxPriority);
void Task_StartScheduler(void);
void Get_CurrentTCB(void);
void MiniRTOS_delay(uint32_t delay_time);




#endif // MINIRTOS_API_H
