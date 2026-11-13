#include "minirtos_queue.h"
#include "minirtos_event.h"
#include "minirtos_task.h"
#include "minirtos.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>

extern volatile uint32_t MiniRTOS_Running;
extern MiniTCB_t *volatile CurrentTCB;

/* 队列初始化 */
void Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length)
{
	uint32_t *Queue;

	if(Queue_Length == NULL || Queue_Length == 0 || Queue_Length > UINT16_MAX)
	{
		printf("Queue Length Error\r\n");

		return;
	}

	/* 分配队列内存 */
	Queue = (uint32_t *)malloc((Queue_Length) * sizeof(uint32_t));

	/* 队列内存申请失败 */
	if (Queue == NULL)
	{
		printf("Queue Init Error\r\n");

		return;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 初始化队列 */
	MiniQueue->Queue = Queue;
	MiniQueue->head = MiniQueue->Queue;
	MiniQueue->tail = MiniQueue->Queue;
	MiniQueue->uxCount = 0;
	MiniQueue->uxMaxCount = Queue_Length;
	ListInit(&MiniQueue->xWaitingToReceive);
	ListInit(&MiniQueue->xWaitingToSend);

	/* 退出临界区 */
	Mini_ExitCritical();
	return;
}

/* 发送队列 */
void Queue_Send(MiniQueue_t *MiniQueue, uint32_t Queue_Value)
{
	MiniTCB_t *TCB1;

	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();
		/* 判断队列是否已满 */
		if (MiniQueue->uxCount == MiniQueue->uxMaxCount)
		{
			/* 从就绪链表删除，添加任务到等待链表 */
			List_ItemRemove(CurrentTCB);
			Task_AddEventWaitList(CurrentTCB, CurrentTCB->uxPriority, &MiniQueue->xWaitingToSend);

			/* 请求任务调度 */
			if (MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}

			/* 退出临界区 */
			Mini_ExitCritical();
		}
		else
			break;
	}

	/* 将数据写入队列 */
	*MiniQueue->head = Queue_Value;
	/* 队列计数加一 */
	MiniQueue->uxCount++;
	/* 队列头指针加一 */
	MiniQueue->head++;
	/* 队列头指针越界，需要回绕 */
	if (MiniQueue->head >= MiniQueue->Queue + MiniQueue->uxMaxCount)
	{
		MiniQueue->head = MiniQueue->Queue;
	}

	/* 判断等待接收队列的任务链表是否为空 */
	if (MiniQueue->xWaitingToReceive.NumberOfItems != 0)
	{
		/* 唤醒等待接收队列的任务 */
		TCB1 = Task_WakeupEventWaitList(&MiniQueue->xWaitingToReceive);
		if (TCB1 != NULL)
		{
			/* 判断任务优先级,抢占式调度 */
			if (TCB1->uxPriority > CurrentTCB->uxPriority)
			{
				/* 请求任务调度 */
				if (MiniRTOS_Running == 1)
				{
					SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
				}
			}
		}
	}

	/* 退出临界区 */
	Mini_ExitCritical();
}

/* 接收队列 */
uint32_t Queue_Receive(MiniQueue_t *MiniQueue)
{
	uint32_t Queue_Value;
	MiniTCB_t *TCB1;

	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();
		/* 判断队列是否为空 */
		if (MiniQueue->uxCount == 0)
		{
			/* 从就绪链表删除，添加任务到等待链表 */
			List_ItemRemove(CurrentTCB);
			Task_AddEventWaitList(CurrentTCB, CurrentTCB->uxPriority, &MiniQueue->xWaitingToReceive);

			/* 请求任务调度 */
			if (MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}

			/* 退出临界区 */
			Mini_ExitCritical();
		}
		else
			break;
	}

	/* 从队列中读取数据 */
	Queue_Value = *MiniQueue->tail;
	/* 队列计数减一 */
	MiniQueue->uxCount--;
	/* 队列尾指针加一 */
	MiniQueue->tail++;
	/* 队列尾指针越界，需要回绕 */
	if (MiniQueue->tail >= MiniQueue->Queue + MiniQueue->uxMaxCount)
	{
		MiniQueue->tail = MiniQueue->Queue;
	}

	/* 判断等待发送队列的任务链表是否为空 */
	if (MiniQueue->xWaitingToSend.NumberOfItems != 0)
	{
		/* 唤醒等待发送队列的任务 */
		TCB1 = Task_WakeupEventWaitList(&MiniQueue->xWaitingToSend);
		if (TCB1 != NULL)
		{
			/* 判断任务优先级,抢占式调度 */
			if (TCB1->uxPriority > CurrentTCB->uxPriority)
			{
				/* 请求任务调度 */
				if (MiniRTOS_Running == 1)
				{
					SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
				}
			}
		}
	}

	/* 退出临界区 */
	Mini_ExitCritical();
	return Queue_Value;
}
