#ifndef MINIRTOS_TASK_H
#define MINIRTOS_TASK_H

#include "minirtos.h"

#define UXPRIORITY_MAX 5
#define MIN_STACK_SIZE 32

/* MiniRTOS初始化 */
void MiniRTOS_Init(void);
/* 启动调度器 */
void Task_StartScheduler(void);
/* 获取执行任务体 */
void Get_CurrentTCB(void);
/* 获取当前系统Systick */
uint32_t MiniRTOS_GetTick(void);
/* 相对延时函数ms */
void MiniRTOS_delay(uint32_t delay_time);
/* 绝对延迟时间ms */
uint8_t MiniRTOS_DelayUntil(uint32_t *PreviousWakeupTime,uint32_t Delay_Time);										
/* 列表初始化 */
void ListInit(List_t *List);
/* 进入临界区 */
void Mini_EnterCritical(void);
/* 退出临界区 */
void Mini_ExitCritical(void);
/* 添加入就绪列表 */
void Task_AddReadyList(MiniTCB_t *TCB);
/* 添加时间阻塞列表 */
void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time);
/* 移除任务体列表项 */
void List_RemoveNode(ListItem_t *Item);
/* 任务创建 */
MiniTCB_t * MiniCreateTask(TaskFunction_t pxTaskCode,
														const char *const pcName,
														const uint32_t uxStackDepth,
														unsigned long uxPriority);
/* 任务挂起 */
MiniRTOS_State Task_Suspend(MiniTCB_t *TCB);
/* 任务挂起恢复 */
MiniRTOS_State Task_Resume(MiniTCB_t *TCB);
/* 任务删除 */
MiniRTOS_State Task_Delete(MiniTCB_t *TCB);
														
														
/*-------------------------------------------------------------------------------*/
/*-----------------------------------中断--------------------------------------*/
/*-------------------------------------------------------------------------------*/
/* 进入临界区 */
uint32_t Mini_EnterCriticalFromISR(void);
/* 退出临界区 */
void Mini_ExitCriticalFromISR(uint32_t SavedMask);
/* 任务挂起恢复 */												
MiniRTOS_State Task_ResumeFromISR(MiniTCB_t *TCB,uint8_t *pxHigherPriorityTaskWoken);


#endif
