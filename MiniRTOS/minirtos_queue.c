#include "minirtos_queue.h"
#include "minirtos_event.h"
#include "minirtos_task.h"
#include "minirtos.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern volatile uint32_t MiniRTOS_Running;
extern MiniTCB_t *volatile CurrentTCB;

/* 队列初始化 */
uint8_t Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length, uint16_t ItemSize)
{
	uint8_t *Queue;
	size_t 	QueueSize;

	/* 参数检查 */
	if ((MiniQueue == NULL) || (Queue_Length == 0U) || (ItemSize == 0U))
	{
		return 0U;
	}
	
	/* 计算总缓冲区大小，防止乘法溢出 */
	if ((size_t)Queue_Length > SIZE_MAX / (size_t)ItemSize)
	{
			return 0U;
	}
	
	QueueSize = (size_t)Queue_Length * (size_t)ItemSize;
	
	/* 分配队列内存 */
	Queue = (uint8_t *)malloc(QueueSize);

	/* 队列内存申请失败 */
	if (Queue == NULL)
	{
		printf("Queue Init Error\r\n");

		return 0U;
	}

	/* 进入临界区 */
	Mini_EnterCritical();
	
	
	/*
	 * 初始化队列结构
	 * 当前版本要求在启动调度器前调用，
	 * 且不能对已初始化的队列重复初始化。
	 */	
	
	/* 初始化队列 */
	MiniQueue->Queue = Queue;
	MiniQueue->head = Queue;
	MiniQueue->tail = Queue;
	MiniQueue->uxCount = 0;
	MiniQueue->uxMaxCount = Queue_Length;
	MiniQueue->uxItemSize = ItemSize;
	
	ListInit(&MiniQueue->xWaitingToReceive);
	ListInit(&MiniQueue->xWaitingToSend);

	/* 退出临界区 */
	Mini_ExitCritical();
	return 1U;
}

/* 发送队列 */
uint8_t Queue_Send(MiniQueue_t *MiniQueue, const void *Data)
{
	MiniTCB_t *TCB1;
	uint8_t *QueueEnd;

	/* 参数检查 */
	if ((MiniQueue == NULL) || (Data == NULL) ||
			(MiniQueue->Queue == NULL) || (MiniQueue->uxItemSize == 0U) ||
			(MiniQueue->uxMaxCount == 0U))
	{
			return 0U;
	}
	
	/* 只能在调度器运行后的任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
			return 0U;
	}
	
	/* 计算队列缓冲区结束地址 */
	QueueEnd = MiniQueue->Queue + (size_t)MiniQueue->uxMaxCount * MiniQueue->uxItemSize;
	
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
			
			/* 被唤醒后重新判断队列 */
			continue;
		}
		
		/* 队列有空闲位置，跳出循环 */
		break;
	}
	
	/* 将数据写入队列 */
	memcpy(MiniQueue->head,Data,MiniQueue->uxItemSize);
	/* 队列元素计数加一 */
	MiniQueue->uxCount++;
	/* 队列头指针前进一个元素 */
	MiniQueue->head += MiniQueue->uxItemSize;
	
	/* 队列头指针越界，需要回绕 */
	if (MiniQueue->head >= QueueEnd)
	{
		MiniQueue->head = MiniQueue->Queue;
	}

	/* 唤醒等待接收的任务 */
	if (MiniQueue->xWaitingToReceive.NumberOfItems > 0U)
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
	
	return 1U;
}

uint8_t Queue_TrySend(MiniQueue_t *MiniQueue,const void *Data)
{
	MiniTCB_t *TCB1;
	uint8_t *QueueEnd;

	/* 参数检查 */
	if ((MiniQueue == NULL) || (Data == NULL) ||
			(MiniQueue->Queue == NULL) || (MiniQueue->uxItemSize == 0U) ||
			(MiniQueue->uxMaxCount == 0U))
	{
			return 0U;
	}
	
	/* 只能在调度器运行后的任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
			return 0U;
	}
	
	/* 计算队列缓冲区结束地址 */
	QueueEnd = MiniQueue->Queue + (size_t)MiniQueue->uxMaxCount * MiniQueue->uxItemSize;

	/* 进入临界区 */
	Mini_EnterCritical();
	/* 判断队列是否已满 */
	if (MiniQueue->uxCount == MiniQueue->uxMaxCount)
	{
		Mini_ExitCritical();
		
		/* 不进入阻塞，立即返回 */
		return 0U;
	}
	
	/* 将数据写入队列 */
	memcpy(MiniQueue->head,Data,MiniQueue->uxItemSize);
	/* 队列元素计数加一 */
	MiniQueue->uxCount++;
	/* 队列头指针前进一个元素 */
	MiniQueue->head += MiniQueue->uxItemSize;
	
	/* 队列头指针越界，需要回绕 */
	if (MiniQueue->head >= QueueEnd)
	{
		MiniQueue->head = MiniQueue->Queue;
	}

	/* 唤醒等待接收的任务 */
	if (MiniQueue->xWaitingToReceive.NumberOfItems > 0U)
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
	
	return 1U;
}



/* 接收队列 */
uint8_t Queue_Receive(MiniQueue_t *MiniQueue, void *Data)
{
	MiniTCB_t *TCB1;
	uint8_t *QueueEnd;

	if ((MiniQueue == NULL) || (Data == NULL) || (MiniQueue->Queue == NULL) || (MiniQueue->uxItemSize == 0U))
	{
			return 0U;
	}
	
	if ((MiniRTOS_Running == 0U) || (__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
			return 0U;
	}	
	
	QueueEnd = MiniQueue->Queue + (size_t)MiniQueue->uxMaxCount * MiniQueue->uxItemSize;
	
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
			
			/* 被唤醒后重新检查 */
			continue;
		}
		
		break;
	}

  /* 将队列中的数据复制到接收缓冲区 */
  memcpy(Data,MiniQueue->tail,MiniQueue->uxItemSize);
	/* 队列元素计数减一 */
	MiniQueue->uxCount--;
	/* 队列尾指针前进一个元素 */
	MiniQueue->tail += MiniQueue->uxItemSize;
	
	/* 队列尾指针越界，需要回绕 */
	if (MiniQueue->tail >= QueueEnd)
	{
		MiniQueue->tail = MiniQueue->Queue;
	}

	/* 判断等待发送队列的任务链表是否为空 */
	if (MiniQueue->xWaitingToSend.NumberOfItems > 0U)
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
	return 1U;
}
