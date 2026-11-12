#ifndef MINIRTOS_TASK_H
#define MINIRTOS_TASK_H

#include "minirtos.h"

void MiniRTOS_Init(void);
MiniTCB_t * MiniCreateTask(TaskFunction_t pxTaskCode,
														const char *const pcName,
														const uint32_t uxStackDepth,
														unsigned long uxPriority);
void MiniRTOS_delay(uint32_t delay_time);
void Task_StartScheduler(void);
void Get_CurrentTCB(void);
void List_ItemRemove(MiniTCB_t *TCB);
void Task_AddList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer);
void Mini_EnterCritical(void);
void Mini_ExitCritical(void);


#endif
