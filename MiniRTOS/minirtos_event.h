#ifndef MINIRTOS_EVENT_H
#define MINIRTOS_EVENT_H

#include "minirtos.h"


void Mutex_Init(MiniMutex_t *Task_MutexSignal);
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal,uint16_t uxMaxCount);

void Task_AddEventWaitList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer);
MiniTCB_t * Task_WakeupEventWaitList(List_t *pxContainer);

void MutexSignal_Take(MiniMutex_t *Task_MutexSignal);
void MutexSignal_Release(MiniMutex_t *Task_MutexSignal);
void SemaphoreSignal_Take(MiniSemaphore_t *Task_SemaphoreSignal);
void SemaphoreSignal_Release(MiniSemaphore_t *Task_SemaphoreSignal);
void Mini_TaskNotify_Sent(MiniTCB_t *TCB,uint32_t Value);
uint32_t Mini_TaskNotify_Get(void);



#endif
