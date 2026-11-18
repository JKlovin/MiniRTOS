#include "minirtos_event.h"
#include "minirtos_task.h"
#include "minirtos.h"
#include "main.h"

/* 系统定义项 */
extern uint32_t primask;
extern volatile uint32_t MiniRTOS_Running;
extern MiniTCB_t *volatile CurrentTCB;
extern List_t Task_ReadyList[5];
extern List_t Task_WaitList_1;
extern List_t Task_WaitList_2;
extern List_t *CurrentWaitList;
extern List_t *OverflowWaitList;
extern List_t Notify_WaitList;

/* 互斥量初始化 */
void Mutex_Init(MiniMutex_t *Task_MutexSignal)
{
	/* 互斥锁所有者置空 */
	Task_MutexSignal->Owner = NULL;

	/* 初始化互斥锁等待链表 */
	ListInit(&Task_MutexSignal->WaitList);
}

/* 信号量初始化 */
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal, uint16_t uxMaxCount)
{
	/* 初始化信号量计数 */
	Task_SemaphoreSignal->uxCount = 0;

	/* 初始化信号量最大计数 */
	Task_SemaphoreSignal->uxMaxCount = uxMaxCount;

	/* 初始化信号量等待链表 */
	ListInit(&Task_SemaphoreSignal->WaitList);
}

void Mini_EventBlock(List_t *EventList)
{
	ListItem_t *NewItem;
	ListItem_t *Index;

	/* 当前任务移除就绪链表 */
	List_ItemRemove(CurrentTCB);

	/* 使用TCB中的事件链表项 */
	NewItem = &CurrentTCB->EventListItem;

	NewItem->pxOwner = CurrentTCB;
	NewItem->pxValue = CurrentTCB->uxPriority;

	/* 从头节点开始查找 */
	Index = EventList->xListEnd.pxNext;

	/* 按优先级从小到大排列 */
	while (Index != &EventList->xListEnd)
	{
		if (Index->pxValue > NewItem->pxValue)
		{
			break;
		}

		Index = Index->pxNext;
	}

	/* 尾插法 */
	NewItem->pxNext = Index;
	NewItem->pxPrevious = Index->pxPrevious;
	Index->pxPrevious->pxNext = NewItem;
	Index->pxPrevious = NewItem;

	/* 更新链表归属 */
	NewItem->pxContainer = EventList;

	/* 队列等待列表元素加一 */
	EventList->NumberOfItems++;

	/* 请求调度 */
	if (MiniRTOS_Running == 1U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
}

MiniTCB_t *Mini_EventWakeup(List_t *EventList)
{
	ListItem_t *Index;
	MiniTCB_t *TCB;

	if ((EventList == NULL) || (EventList->NumberOfItems == 0U))
	{
		return NULL;
	}

	/* 取得最高优先级的一组等待任务 */
	Index = EventList->xListEnd.pxPrevious;

	/* 取得优先级最好且最早等待事件的任务 */
	while (Index->pxPrevious != &EventList->xListEnd)
	{
		if (Index->pxPrevious->pxValue != Index->pxValue)
		{
			break;
		}

		Index = Index->pxPrevious;
	}

	/* 先保存待唤醒任务的 TCB */
	TCB = (MiniTCB_t *)Index->pxOwner;

	/* 解除队列事件等待 */
	List_RemoveNode(&TCB->EventListItem);

	/* 判断是否在等待超时 */
	if (TCB->StateListItem.pxContainer != NULL)
	{
		List_RemoveNode(&TCB->StateListItem);
	}

	/* 加入就绪链表 */
	Task_AddReadyList(TCB);

	return TCB;
}

/* 获取互斥锁 */
void MutexSignal_Take(MiniMutex_t *Task_MutexSignal)
{
	List_t *List;

	while (1)
	{

		/* 进入临界区 */
		Mini_EnterCritical();

		/* 互斥锁空闲，直接获取 */
		if (Task_MutexSignal->Owner == NULL)
		{
			/* 互斥锁所有者设置为当前任务 */
			Task_MutexSignal->Owner = CurrentTCB;

			/* 退出临界区 */
			Mini_ExitCritical();
			break;
		}

		/* 禁止重复获取 */
		if (Task_MutexSignal->Owner == CurrentTCB)
		{
			/* 当前任务已持有互斥锁，直接退出临界区 */
			Mini_ExitCritical();
			break;
		}

		/* 没有互斥锁，获取失败 */
		List = Task_MutexSignal->Owner->StateListItem.pxContainer;
		/* 优先级继承，防止优先级反转 */
		if (Task_MutexSignal->Owner->uxPriority < CurrentTCB->uxPriority)
		{
			/* 优先级继承任务处于就绪列表 */
			if (List == &Task_ReadyList[Task_MutexSignal->Owner->uxPriority])
			{
				/* 重新设置互斥锁拥有者的运行优先级，并重新加入原有列表 */
				List_ItemRemove(Task_MutexSignal->Owner);
				Task_MutexSignal->Owner->uxPriority = CurrentTCB->uxPriority;
				Task_AddReadyList(Task_MutexSignal->Owner);
			}
			/* 优先级继承任务处于阻塞列表 */
			else if (List == CurrentWaitList || List == OverflowWaitList)
			{
				/* 重新设置互斥锁拥有者的运行优先级 */
				Task_MutexSignal->Owner->uxPriority = CurrentTCB->uxPriority;
			}
			/* 优先级继承任务处于就绪列表或阻塞列表之外的列表，如 互斥锁等待 或 信号量等待 列表 */
			else
			{
				/* 重新设置互斥锁拥有者的运行优先级，并重新加入原有列表 */
				Task_MutexSignal->Owner->uxPriority = CurrentTCB->uxPriority;
				List_ItemRemove(Task_MutexSignal->Owner);
				Task_AddEventWaitList(Task_MutexSignal->Owner, Task_MutexSignal->Owner->uxPriority, List);
			}
		}

		/* 从就绪链表删除 */
		List_ItemRemove(CurrentTCB);

		/* 加入互斥锁阻塞链表 */
		Task_AddEventWaitList(CurrentTCB, CurrentTCB->uxPriority, &Task_MutexSignal->WaitList);

		/* 请求任务调度 */
		if (MiniRTOS_Running == 1)
		{
			SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
		}

		/* 退出临界区 */
		Mini_ExitCritical();
	}
	return;
}

/* 释放互斥锁 */
void MutexSignal_Release(MiniMutex_t *Task_MutexSignal)
{
	/* 进入临界区 */
	Mini_EnterCritical();

	/* 判断互斥锁所有者是否为当前任务 */
	if (Task_MutexSignal->Owner == CurrentTCB)
	{
		/* 释放互斥锁 */
		Task_MutexSignal->Owner = NULL;

		/* 任务优先级还原 */
		if (CurrentTCB->uxPriority != CurrentTCB->uxInitPriority)
		{
			/* 恢复当前任务的初始优先级 */
			CurrentTCB->uxPriority = CurrentTCB->uxInitPriority;
			List_ItemRemove(CurrentTCB);
			Task_AddReadyList(CurrentTCB);
		}

		/* 判断互斥锁等待链表是否为空 */
		if (Task_MutexSignal->WaitList.NumberOfItems > 0)
		{
			/* 唤醒等待互斥锁的任务 */
			Task_WakeupEventWaitList(&Task_MutexSignal->WaitList);

			/* 请求任务调度 */
			if (MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}
		}
	}

	/* 退出临界区 */
	Mini_ExitCritical();
}

/* 带有超时等待的取出信号量 */
MiniRTOS_State Semaphore_TakeTimeout(MiniSemaphore_t *Semaphore, uint32_t WaitTicks)
{
	uint32_t StartTick;
	uint32_t ElapsedTick;
	uint32_t RemainingTick;

	/* 参数检查 */
	if ((Semaphore == NULL) || (Semaphore->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 只允许调度器运行后的任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 记录阻塞开始时间 */
	StartTick = MiniRTOS_GetTick();

	/* 状态机判断 */
	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 1. 信号量可用 */
		/* 直接获取 */
		if (Semaphore->uxCount > 0)
		{
			/* 信号量计数减一 */
			Semaphore->uxCount--;

			/* 退出临界区 */
			Mini_ExitCritical();
			return MiniRTOS_OK;
		}

		/* 2. 信号量不可用 */
		/* 不允许等待 */
		if (WaitTicks == 0U)
		{
			/* 退出临界区 */
			Mini_ExitCritical();

			/* 直接返回 */
			return MiniRTOS_Timeout;
		}

		/* 3. 信号量不可用 */
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

			/* 剩余等待预算 */
			RemainingTick = WaitTicks - ElapsedTick;
		}
		else /* 死等 */
		{
			/* 剩余等待时间 */
			RemainingTick = MINIRTOS_WAIT_FOREVER;
		}

		/* 4. 需要阻塞超时等待 */
		/* 加入事件阻塞列表、时间阻塞列表 */
		Mini_EventBlock(&Semaphore->WaitList);

		/* 判断是否无限期超时等待 */
		/* 无限期超时等待不加入时间阻塞列表 */
		if (RemainingTick != MINIRTOS_WAIT_FOREVER)
		{
			Task_AddWaitList(CurrentTCB, RemainingTick);
		}
		/* 退出临界区 */
		Mini_ExitCritical();

		/*
		 * 5. 任务被唤醒后，重新检查信号量
		 *
		 * 可能是：
		 * - 事件提前触发
		 * - 等待时间到期
		 *
		 * 不直接认为唤醒代表获取成功
		 */
	}
}

/* 释放信号量 */
MiniRTOS_State Semaphore_Give(MiniSemaphore_t *Semaphore)
{
	MiniTCB_t *TCB;

	/* 参数检查 */
	if ((Semaphore == NULL) || (Semaphore->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 只允许调度器运行后的任务上下文调用 */
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 信号量计数已满 */
	if (Semaphore->uxCount >= Semaphore->uxMaxCount)
	{
		/* 退出临界区 */
		Mini_ExitCritical();

		return MiniRTOS_Error;
	}

	/* 信号量计数加一 */
	Semaphore->uxCount++;

	/* 唤醒一个正在等待的任务 */
	TCB = Mini_EventWakeup(&Semaphore->WaitList);

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

	/* 退出临界区 */
	Mini_ExitCritical();

	return MiniRTOS_OK;
}

/* 无限阻塞获取 */
uint8_t Semaphore_Take(MiniSemaphore_t *Semaphore)
{
	return (Semaphore_TakeTimeout(Semaphore, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 非阻塞获取 */
uint8_t Semaphore_TryTake(MiniSemaphore_t *Semaphore)
{
	return (Semaphore_TakeTimeout(Semaphore, 0U) == MiniRTOS_OK) ? 1U : 0U;
}

/* 发送任务通知 */
void Mini_TaskNotify_Sent(MiniTCB_t *TCB, uint32_t Value)
{
	/* 进入临界区 */
	Mini_EnterCritical();

	/* 设置任务通知值 */
	TCB->NotifyValue = Value;

	/* 判断任务通知状态 */
	/* 任务正在等待通知 */
	if (TCB->NotifyState == Notify_Wait)
	{
		/* 将任务从事件等待链表移除 */
		List_ItemRemove(TCB);
		/* 设置任务通知状态为就绪 */
		TCB->NotifyState = Notify_Ready;
		/* 将任务添加到就绪链表 */
		Task_AddReadyList(TCB);

		/* 判断任务优先级是否高于当前任务 */
		if (TCB->uxPriority > CurrentTCB->uxPriority)
		{
			/* 请求任务调度 */
			if (MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}
		}
	}
	else /* 通知已经发送，但任务未等待通知 */
	{
		/* 设置任务通知状态为就绪 */
		TCB->NotifyState = Notify_Ready;
	}

	/* 退出临界区 */
	Mini_ExitCritical();
}

/* 获取任务通知 */
uint32_t Mini_TaskNotify_Get(void)
{
	uint32_t Value;

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 判断任务通知状态 */
	/* 还没收到通知 */
	while (CurrentTCB->NotifyState != Notify_Ready)
	{
		/* 设置任务通知状态为等待 */
		CurrentTCB->NotifyState = Notify_Wait;
		/* 添加任务到事件等待链表 */
		List_ItemRemove(CurrentTCB);
		Task_AddEventWaitList(CurrentTCB, CurrentTCB->uxPriority, &Notify_WaitList);

		/* 请求调度 */
		if (MiniRTOS_Running == 1)
		{
			SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
		}

		/* 退出临界区 */
		Mini_ExitCritical();

		/* 进入临界区 */
		Mini_EnterCritical();
	}

	/* 收到通知值 */
	Value = CurrentTCB->NotifyValue;
	/* 设置任务通知状态为空闲 */
	CurrentTCB->NotifyState = Notify_Idle;
	/* 退出临界区 */
	Mini_ExitCritical();

	return Value;
}

/* 元素添加事件链表 */
void Task_AddEventWaitList(MiniTCB_t *TCB, uint32_t pxValue, List_t *pxContainer)
{
	ListItem_t *Index;
	ListItem_t *NewItem;
	List_t *List;

	NewItem = &TCB->StateListItem;

	/* 设置优先级 */
	NewItem->pxValue = pxValue;

	NewItem->pxOwner = TCB;

	List = pxContainer;

	/* 从头节点开始查找 */
	Index = List->xListEnd.pxNext;

	/* 按优先级从小到大排列 */
	while (Index != &List->xListEnd)
	{
		if (Index->pxValue > NewItem->pxValue)
		{
			break;
		}

		Index = Index->pxNext;
	}

	/* 尾插法 */
	NewItem->pxNext = Index;
	NewItem->pxPrevious = Index->pxPrevious;
	Index->pxPrevious->pxNext = NewItem;
	Index->pxPrevious = NewItem;

	/* 更新链表归属 */
	NewItem->pxContainer = List;

	List->NumberOfItems++;
}

/* 元素移除事件链表 */
MiniTCB_t *Task_WakeupEventWaitList(List_t *pxContainer)
{
	List_t *List;
	ListItem_t *Index;
	MiniTCB_t *TCB;

	List = pxContainer;

	if (List->NumberOfItems == 0)
	{
		return NULL;
	}

	Index = List->xListEnd.pxPrevious;

	/* 取得优先级最好且最早等待事件(互斥、信号量、任务通知)的任务 */
	while (Index->pxPrevious != &List->xListEnd)
	{
		if (Index->pxPrevious->pxValue != Index->pxValue)
		{
			break;
		}

		Index = Index->pxPrevious;
	}

	/* 先保存待唤醒任务的 TCB */
	TCB = (MiniTCB_t *)Index->pxOwner;
	/* 从阻塞链表中移除 */
	List_ItemRemove(TCB);
	/* 加入就绪链表 */
	Task_AddReadyList(TCB);

	return TCB;
}
