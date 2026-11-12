#ifndef MINIRTOS_API_H
#define MINIRTOS_API_H

#include "minirtos_task.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void MiniRTOS_Init(void);
MiniTCB_t *MiniCreateTask(TaskFunction_t pxTaskCode,
                    const char *const pcName,
                    const uint32_t uxStackDepth,
                    unsigned long uxPriority);
void Task_StartScheduler(void);
void MiniRTOS_delay(uint32_t delay_time);
										
void MutexSignal_Take(MiniMutex_t *Task_MutexSignal);
void MutexSignal_Release(MiniMutex_t *Task_MutexSignal);
void Mutex_Init(MiniMutex_t *Task_MutexSignal);
					
void SemaphoreSignal_Take(MiniSemaphore_t *Task_SemaphoreSignal);
void SemaphoreSignal_Release(MiniSemaphore_t *Task_SemaphoreSignal);
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal,
													uint16_t uxMaxCount);

void Mini_TaskNotify_Sent(MiniTCB_t *TCB,uint32_t Value);
uint32_t Mini_TaskNotify_Get(void);


#endif
