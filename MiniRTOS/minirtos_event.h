#ifndef MINIRTOS_EVENT_H
#define MINIRTOS_EVENT_H

#include "minirtos.h"

void Mutex_Init(MiniMutex_t *Task_MutexSignal);
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal, uint16_t uxMaxCount);

void Task_AddEventWaitList(MiniTCB_t *TCB, uint32_t pxValue, List_t *pxContainer);
MiniTCB_t *Task_WakeupEventWaitList(List_t *pxContainer);

void MutexSignal_Take(MiniMutex_t *Task_MutexSignal);
void MutexSignal_Release(MiniMutex_t *Task_MutexSignal);

void Mini_TaskNotify_Sent(MiniTCB_t *TCB, uint32_t Value);
uint32_t Mini_TaskNotify_Get(void);

/* 事件添加阻塞 */
void Mini_EventBlock(List_t *EventList);
/* 事件唤醒阻塞 */
MiniTCB_t *Mini_EventWakeup(List_t *EventList);

/* 有限超时获取信号量 */
MiniRTOS_State Semaphore_TakeTimeout(MiniSemaphore_t *Semaphore, uint32_t WaitTicks);
/* 无限阻塞获取信号量 */
uint8_t Semaphore_Take(MiniSemaphore_t *Semaphore);
/* 非阻塞获取信号量 */
uint8_t Semaphore_TryTake(MiniSemaphore_t *Semaphore);
/* 释放信号量 */
MiniRTOS_State Semaphore_Give(MiniSemaphore_t *Semaphore);

#endif
