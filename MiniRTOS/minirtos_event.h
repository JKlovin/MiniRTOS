#ifndef MINIRTOS_EVENT_H
#define MINIRTOS_EVENT_H

#include "minirtos.h"

#define MINIRTOS_MUTEX_CHAIN_MAX 32U

/*-------------------------------------------------------------------------------*/
/*-------------------------------事件阻塞唤醒------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 当前任务进入事件阻塞 */
void Mini_EventBlock(List_t *EventList);
/* 唤醒一个最高优先级的事件等待任务 */
MiniTCB_t *Mini_EventWakeup(List_t *EventList);

/*-------------------------------------------------------------------------------*/
/*-----------------------------------互斥锁--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 互斥锁初始化 */
void Mutex_Init(MiniMutex_t *Task_MutexSignal);
/* 有限超时获取互斥锁 */
MiniRTOS_State Mutex_TakeTimeout(MiniMutex_t *Mutex,uint32_t WaitTicks);
/* 无限阻塞获取 */
uint8_t Mutex_Take(MiniMutex_t *Mutex);
/* 非阻塞获取 */
uint8_t Mutex_TryTake(MiniMutex_t *Mutex);
/* 释放互斥锁 */
MiniRTOS_State Mutex_Release(MiniMutex_t *Mutex);
void Mutex_OnWaiterTimeout(MiniTCB_t *TCB);

/*-------------------------------------------------------------------------------*/
/*-----------------------------------任务通知------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 发送任务通知 */
MiniRTOS_State TaskNotify_Send(MiniTCB_t *TCB, uint32_t Value);
/* 有限超时等待任务通知 */
MiniRTOS_State TaskNotify_WaitTimeout(uint32_t *Value,uint32_t WaitTicks);
/* 无限阻塞等待通知 */
uint8_t TaskNotify_Wait(uint32_t *Value);
/* 非阻塞获取通知 */
uint8_t TaskNotify_TryWait(uint32_t *Value);

/*-------------------------------------------------------------------------------*/
/*-----------------------------------信号量--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 信号量初始化 */
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal, uint16_t uxMaxCount);
/* 有限超时获取信号量 */
MiniRTOS_State Semaphore_TakeTimeout(MiniSemaphore_t *Semaphore, uint32_t WaitTicks);
/* 无限阻塞获取信号量 */
uint8_t Semaphore_Take(MiniSemaphore_t *Semaphore);
/* 非阻塞获取信号量 */
uint8_t Semaphore_TryTake(MiniSemaphore_t *Semaphore);
/* 释放信号量 */
MiniRTOS_State Semaphore_Give(MiniSemaphore_t *Semaphore);

/*-------------------------------------------------------------------------------*/
/*-----------------------------------中断--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/*-----------------------------------*/
/*-------------信号量----------------*/
/*-----------------------------------*/

/* 释放信号量 */
MiniRTOS_State Semaphore_GiveFromISR(MiniSemaphore_t *Semaphore,uint8_t *pxHigherPriorityTaskWoken);
/* 获取信号量 */
MiniRTOS_State Semaphore_TakeFromISR(MiniSemaphore_t *Semaphore);

/*-----------------------------------*/
/*-------------任务通知--------------*/
/*-----------------------------------*/
MiniRTOS_State TaskNotify_SendFromISR(MiniTCB_t *TCB,uint32_t Value,uint8_t *pxHigherPriorityTaskWoken);




#endif
