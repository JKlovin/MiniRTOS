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
	size_t QueueSize;

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

/* 带有超时等待的队列接收函数 */
MiniRTOS_State Queue_ReceiveTimeout(MiniQueue_t *MiniQueue, void *Data, uint32_t WaitTicks)
{
	MiniTCB_t *TCB;
	uint8_t *QueueEnd;

	uint32_t StartTick;
	uint32_t ElapsedTick;
	uint32_t RemainingTick;

	/* 参数检查 */
	if ((MiniQueue == NULL) || (Data == NULL) ||
		(MiniQueue->Queue == NULL) || (MiniQueue->uxItemSize == 0U) ||
		(MiniQueue->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 只允许调度器运行后的任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	QueueEnd = MiniQueue->Queue + (size_t)MiniQueue->uxMaxCount * MiniQueue->uxItemSize;

	/* 记录阻塞开始时间 */
	StartTick = MiniRTOS_GetTick();

	/* 状态机判断 */
	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 1. 队列中有数据 */
		/* 直接接收 */
		if (MiniQueue->uxCount > 0U)
		{
			/* 数据读取 */
			memcpy(Data, MiniQueue->tail, MiniQueue->uxItemSize);

			MiniQueue->uxCount--;

			MiniQueue->tail += MiniQueue->uxItemSize;

			/* 循环队列到达队尾 */
			if (MiniQueue->tail >= QueueEnd)
			{
				MiniQueue->tail = MiniQueue->Queue;
			}

			/* 接收成功，唤醒等待发送的任务 */
			if (MiniQueue->xWaitingToSend.NumberOfItems > 0U)
			{
				TCB = Mini_EventWakeup(&MiniQueue->xWaitingToSend);

				/* 判断是否需要立刻调度 */
				if (TCB != NULL)
				{
					if ((TCB->uxPriority > CurrentTCB->uxPriority))
					{
						if (MiniRTOS_Running == 1)
						{
							/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
							SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
						}
					}
				}
			}

			/* 退出临界区 */
			Mini_ExitCritical();

			return MiniRTOS_OK;
		}

		/* 2. 队列为空 */
		/* 不允许等待 */
		if (WaitTicks == 0U)
		{
			/* 退出临界区 */
			Mini_ExitCritical();

			return MiniRTOS_Timeout;
		}

		/* 3. 队列为空 */
		/* 允许等待 */
		/* 判断是否死等 */
		if (WaitTicks != MINIRTOS_WAIT_FOREVER)
		{
			/* 该任务已经消耗时间 */
			ElapsedTick = MiniRTOS_GetTick() - StartTick;

			/* 消耗时间大于等待超时时间，即早就等待超时了 */
			if (ElapsedTick >= WaitTicks)
			{
				/* 退出临界区 */
				Mini_ExitCritical();

				/* 立刻返回 */
				return MiniRTOS_Timeout;
			}

			/* 剩余等待时间 */
			RemainingTick = WaitTicks - ElapsedTick;
		}
		else /* 死等 */
		{
			/* 剩余等待时间 */
			RemainingTick = MINIRTOS_WAIT_FOREVER;
		}

		/* 4. 需要阻塞超时等待 */
		/* 加入事件阻塞列表、时间阻塞列表 */
		Mini_EventBlock(&MiniQueue->xWaitingToReceive);

		/* 判断是否无限期超时等待 */
		/* 无限期超时等待不加入时间阻塞列表 */
		if (RemainingTick != MINIRTOS_WAIT_FOREVER)
		{
			Task_AddWaitList(CurrentTCB, RemainingTick);
		}

		/* 退出临界区 */
		Mini_ExitCritical();

		/*
		 * 5. 任务被唤醒后，重新检查队列
		 *
		 * 可能是：
		 * - 事件提前触发
		 * - 等待时间到期
		 *
		 * 不直接认为唤醒代表接收成功
		 */
	}
}

/* 带有超时等待的队列发送函数 */
MiniRTOS_State Queue_SendTimeout(MiniQueue_t *MiniQueue, const void *Data, uint32_t WaitTicks)
{
	MiniTCB_t *WakeTCB;
	uint8_t *QueueEnd;

	uint32_t StartTick;
	uint32_t ElapsedTick;
	uint32_t RemainingTick;

	/* 参数检查 */
	if ((MiniQueue == NULL) || (Data == NULL) || (MiniQueue->Queue == NULL) ||
		(MiniQueue->uxItemSize == 0U) || (MiniQueue->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 只允许任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	QueueEnd = MiniQueue->Queue + (size_t)MiniQueue->uxMaxCount * MiniQueue->uxItemSize;

	/* 记录阻塞开始时间 */
	StartTick = MiniRTOS_GetTick();

	/* 状态机判断 */
	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 1. 队列中有数据 */
		/* 直接发送 */
		if (MiniQueue->uxCount < MiniQueue->uxMaxCount)
		{
			/* 数据发送 */
			memcpy(MiniQueue->head, Data, MiniQueue->uxItemSize);

			MiniQueue->uxCount++;

			MiniQueue->head += MiniQueue->uxItemSize;

			/* 循环队列到达队尾 */
			if (MiniQueue->head >= QueueEnd)
			{
				MiniQueue->head = MiniQueue->Queue;
			}

			/* 唤醒等待接收的任务 */
			if (MiniQueue->xWaitingToReceive.NumberOfItems > 0U)
			{
				WakeTCB = Mini_EventWakeup(&MiniQueue->xWaitingToReceive);

				/* 判断是否需要立刻调度 */
				if (WakeTCB != NULL)
				{
					if ((WakeTCB->uxPriority > CurrentTCB->uxPriority))
					{
						if (MiniRTOS_Running == 1)
						{
							/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
							SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
						}
					}
				}
			}

			/* 退出临界区 */
			Mini_ExitCritical();

			return MiniRTOS_OK;
		}

		/* 2. 队列为满 */
		/* 不允许等待 */
		if (WaitTicks == 0U)
		{
			/* 退出临界区 */
			Mini_ExitCritical();

			return MiniRTOS_Timeout;
		}

		/* 3. 队列为满 */
		/* 允许等待 */
		/* 判断是否死等 */
		if (WaitTicks != MINIRTOS_WAIT_FOREVER)
		{
			ElapsedTick = MiniRTOS_GetTick() - StartTick;

			/* 消耗时间大于等待超时时间，即早就等待超时了 */
			if (ElapsedTick >= WaitTicks)
			{
				/* 退出临界区 */
				Mini_ExitCritical();

				/* 立刻返回 */
				return MiniRTOS_Timeout;
			}

			/* 剩余等待时间 */
			RemainingTick = WaitTicks - ElapsedTick;
		}
		else /* 死等 */
		{
			/* 剩余等待时间 */
			RemainingTick = MINIRTOS_WAIT_FOREVER;
		}

		/* 4. 需要阻塞超时等待 */
		/* 加入事件阻塞列表、时间阻塞列表 */
		Mini_EventBlock(&MiniQueue->xWaitingToSend);

		/* 判断是否无限期超时等待 */
		/* 无限期超时等待不加入时间阻塞列表 */
		if (RemainingTick != MINIRTOS_WAIT_FOREVER)
		{
			Task_AddWaitList(CurrentTCB, RemainingTick);
		}

		/* 退出临界区 */
		Mini_ExitCritical();

		/*
		 * 5. 任务被唤醒后，重新检查队列
		 *
		 * 可能是：
		 * - 事件提前触发
		 * - 等待时间到期
		 *
		 * 不直接认为唤醒代表接收成功
		 */
	}
}

/* 无限阻塞的队列发送函数 */
uint8_t Queue_Send(MiniQueue_t *MiniQueue, const void *Data)
{
	return (Queue_SendTimeout(MiniQueue, Data, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 不阻塞的队列发送函数 */
uint8_t Queue_TrySend(MiniQueue_t *MiniQueue, const void *Data)
{
	return (Queue_SendTimeout(MiniQueue, Data, 0U) == MiniRTOS_OK) ? 1U : 0U;
}

/* 无限阻塞的队列接收函数 */
uint8_t Queue_Receive(MiniQueue_t *MiniQueue, void *Data)
{
	return (Queue_ReceiveTimeout(MiniQueue, Data, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 不阻塞的队列接收函数 */
uint8_t Queue_TryReceive(MiniQueue_t *MiniQueue, void *Data)
{
	return (Queue_ReceiveTimeout(MiniQueue, Data, 0U) == MiniRTOS_OK) ? 1U : 0U;
}
