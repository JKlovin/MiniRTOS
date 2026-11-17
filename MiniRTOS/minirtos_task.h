#ifndef MINIRTOS_TASK_H
#define MINIRTOS_TASK_H

#include "minirtos.h"

void MiniRTOS_Init(void);
MiniTCB_t * MiniCreateTask(TaskFunction_t pxTaskCode,
														const char *const pcName,
														const uint32_t uxStackDepth,
														unsigned long uxPriority);
void MiniRTOS_delay(uint32_t delay_time);
uint32_t MiniRTOS_GetTick(void);
uint8_t MiniRTOS_DelayUntil(uint32_t *PreviousWakeupTime,uint32_t Delay_Time);
void ListInit(List_t *List);
void Task_StartScheduler(void);
void Get_CurrentTCB(void);
void List_ItemRemove(MiniTCB_t *TCB);
void Task_AddReadyList(MiniTCB_t *TCB);											
void Mini_EnterCritical(void)	;
void Mini_ExitCritical(void)	;
void MiniRTOS_TickHandler(void);


#endif
