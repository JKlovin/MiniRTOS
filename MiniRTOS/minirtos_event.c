#include "minirtos_event.h"
#include "minirtos_task.h"
#include "minirtos.h"
#include "main.h"

/* 系统定义项 */
extern uint32_t primask;
extern volatile uint32_t MiniRTOS_Running;
extern MiniTCB_t *volatile CurrentTCB;
extern List_t Task_ReadyList[UXPRIORITY_MAX];
extern List_t Task_WaitList_1;
extern List_t Task_WaitList_2;
extern List_t *CurrentWaitList;
extern List_t *OverflowWaitList;
extern List_t Notify_WaitList;

/* 事件等待全局顺序计数器 */
static uint64_t uxEventWaitSequence = 0ULL;

/* 互斥量初始化 */
void Mutex_Init(MiniMutex_t *Mutex)
{
	if (Mutex == NULL)
	{
		return;
	}

	/* 互斥锁所有者置空 */
	Mutex->Owner = NULL;

	/* 互斥锁链表下一个为空 */
	Mutex->pxNextHeldMutex = NULL;

	/* 初始化互斥锁最高优先级 */
	Mutex->uxHighestWaitPriority = 0U;

	/* 初始化互斥锁等待链表 */
	ListInit(&Mutex->WaitList);
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

/* 带优先级、先后的列表插入 */
static void Mini_EventInsertPriority(List_t *EventList, MiniTCB_t *TCB)
{
	ListItem_t *NewItem;
	ListItem_t *Index;
	MiniTCB_t *OtherTCB;

	/* 获取待插入列表 */
	NewItem = &TCB->EventListItem;

	NewItem->pxOwner = TCB;
	NewItem->pxValue = TCB->uxPriority;

	/* 从链表头部开始查找插入位置 */
	Index = EventList->xListEnd.pxNext;

	while (Index != &EventList->xListEnd)
	{
		OtherTCB = (MiniTCB_t *)Index->pxOwner;

		/* 第一排序条件：有效优先级 */
		if (Index->pxValue > NewItem->pxValue)
		{
			break;
		}

		/* 第二排序条件：先后等待顺序 */
		if (Index->pxValue == NewItem->pxValue)
		{
			if (OtherTCB->uxEventWaitOrder > TCB->uxEventWaitOrder)
			{
				break;
			}
		}

		Index = Index->pxNext;
	}

	/* Index节点前插NewItem */
	NewItem->pxNext = Index;
	NewItem->pxPrevious = Index->pxPrevious;

	Index->pxPrevious->pxNext = NewItem;
	Index->pxPrevious = NewItem;

	NewItem->pxContainer = EventList;

	EventList->NumberOfItems++;
}

/* 重新排序事件等待链表 */
static void Mini_EventReorderTask(MiniTCB_t *TCB)
{
	List_t *list;

	/* 获取任务事件等待列表 */
	list = TCB->EventListItem.pxContainer;

	if (list == NULL)
	{
		return;
	}

	/* 移除 */
	List_RemoveNode(&TCB->EventListItem);
	/* 重新插入 */
	Mini_EventInsertPriority(list, TCB);
}

/* 任务阻塞 */
void Mini_EventBlock(List_t *EventList)
{
	/* 从就绪链表移除当前任务 */
	List_RemoveNode(&CurrentTCB->StateListItem);

	/* 排序，保证任务插入移除先后关系 */
	uxEventWaitSequence++;

	CurrentTCB->uxEventWaitOrder = uxEventWaitSequence;

	/* 按优先级和原始等待序号插入 */
	Mini_EventInsertPriority(EventList, CurrentTCB);

	/* 请求任务调度 */
	if (MiniRTOS_Running == 1U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
}

/* 任务唤醒 */
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

/* 获取互斥锁的最高等待优先级 */
static void Mutex_RefreshHighest(MiniMutex_t *Mutex)
{
	if (Mutex->WaitList.NumberOfItems == 0U)
	{
		Mutex->uxHighestWaitPriority = 0U;
	}
	else
	{
		/* 互斥锁等待链表按优先级从小到大排序 */
		/* 通过互斥锁等待链表最后一项，获取当前最高等待优先级 */
		Mutex->uxHighestWaitPriority = Mutex->WaitList.xListEnd.pxPrevious->pxValue;
	}
}

/* 互斥锁所有者权限标记 */
static void Mutex_LinkOwner(MiniMutex_t *Mutex, MiniTCB_t *Owner)
{
	/* 互斥锁所有者设置为当前任务 */
	Mutex->Owner = Owner;
	/* 将当前互斥锁插入任务持有互斥锁链表中 */
	Mutex->pxNextHeldMutex = Owner->pxHeldMutexList;
	Owner->pxHeldMutexList = Mutex;
}

/* 互斥锁所有者解除标记 */
static uint8_t Mutex_UnlinkOwner(MiniMutex_t *Mutex, MiniTCB_t *Owner)
{
	MiniMutex_t **link = &Owner->pxHeldMutexList;

	while ((*link != NULL) && (*link != Mutex))
	{
		link = &(*link)->pxNextHeldMutex;
	}

	if (*link == NULL)
	{
		return 0U;
	}

	*link = Mutex->pxNextHeldMutex;
	Mutex->pxNextHeldMutex = NULL;
	Mutex->Owner = NULL;
	return 1U;
}

/* 互斥锁死锁判断 */
static uint8_t Mutex_WouldDeadlock(MiniTCB_t *Waiter, MiniMutex_t *Mutex)
{
	MiniTCB_t *owner;
	uint32_t depth;

	owner = Mutex->Owner;

	for (depth = 0U; (owner != NULL) && (depth < MINIRTOS_MUTEX_CHAIN_MAX); depth++)
	{
		if (owner == Waiter)
		{
			return 1U;
		}
		if (owner->pxWaitingMutex == NULL)
		{
			return 0U;
		}
		owner = owner->pxWaitingMutex->Owner;
	}

	return (owner != NULL) ? 1U : 0U;
}

/* 修改任务当前执行优先级*/
static void Mutex_SetEffectivePriority(MiniTCB_t *TCB, uint32_t NewPriority)
{
	uint32_t oldPriority;
	List_t *stateList;

	/* 当前任务执行优先级 */
	oldPriority = TCB->uxPriority;

	/* 获取当前任务状态链表 */
	stateList = TCB->StateListItem.pxContainer;

	/* 不需要修改优先级 */
	if (NewPriority == oldPriority)
	{
		return;
	}

	/* 当前任务是就绪态 */
	/* 移除，更改优先级，更新新的就绪列表 */
	if (stateList == &Task_ReadyList[oldPriority])
	{
		List_RemoveNode(&TCB->StateListItem);
		TCB->uxPriority = NewPriority;
		Task_AddReadyList(TCB);
	}
	else /* 当前任务是阻塞态，时间阻塞链表，仅更改优先级 */
	{
		TCB->uxPriority = NewPriority;
	}

	/* 重新排序时间等待链表 */
	Mini_EventReorderTask(TCB);
}

/* 链式互斥锁更新优先级 */
static void Mutex_RecomputeChain(MiniTCB_t *TCB)
{
	uint32_t depth;
	uint32_t target;
	MiniMutex_t *held;

	for (depth = 0U; (TCB != NULL) && (depth < MINIRTOS_MUTEX_CHAIN_MAX); depth++)
	{
		/* 获取任务的原始优先级 */
		target = TCB->uxInitPriority;

		/* 遍历当前任务持有的所有互斥锁 */
		for (held = TCB->pxHeldMutexList; held != NULL; held = held->pxNextHeldMutex)
		{
			/* 更新该互斥锁最高等待优先级 */
			Mutex_RefreshHighest(held);

			/* 逐步求优先级最大值 */
			if ((held->WaitList.NumberOfItems != 0U) && (held->uxHighestWaitPriority > target))
			{
				target = held->uxHighestWaitPriority;
			}
		}

		/* 判断是否需要更新当前任务执行优先级 */
		if (target == TCB->uxPriority)
		{
			break;
		}

		/* 更新当前任务执行优先级 */
		Mutex_SetEffectivePriority(TCB, target);

		/* 当前任务没有等待任何互斥锁 */
		/* 链式结构结束 */
		if (TCB->pxWaitingMutex == NULL)
		{
			break;
		}

		/* 对链式互斥锁下一个互斥锁进行优先级更新 */
		Mutex_RefreshHighest(TCB->pxWaitingMutex);
		TCB = TCB->pxWaitingMutex->Owner;
	}
}

/* 互斥锁等待超时处理 */
void Mutex_OnWaiterTimeout(MiniTCB_t *TCB)
{
	MiniMutex_t *mutex;

	if ((TCB == NULL) || (TCB->pxWaitingMutex == NULL))
	{
		return;
	}

	/* 获取当前任务正在等待的互斥锁 */
	mutex = TCB->pxWaitingMutex;

	/* 移除 */
	if (TCB->EventListItem.pxContainer == &mutex->WaitList)
	{
		List_RemoveNode(&TCB->EventListItem);
	}

	TCB->pxWaitingMutex = NULL;

	/* 更新互斥锁最高优先级 */
	Mutex_RefreshHighest(mutex);

	/* 更新链式互斥锁优先级 */
	if (mutex->Owner != NULL)
	{
		Mutex_RecomputeChain(mutex->Owner);
	}
}

/* 有限超时的申请互斥锁 */
MiniRTOS_State Mutex_TakeTimeout(MiniMutex_t *Mutex, uint32_t WaitTicks)
{
	MiniRTOS_State status;

	if (Mutex == NULL)
	{
		return MiniRTOS_Invalid;
	}

	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 重复获取互斥锁 */
	if (Mutex->Owner == CurrentTCB)
	{
		/* 退出临界区 */
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 互斥锁可获取，获取互斥锁 */
	if (Mutex->Owner == NULL)
	{
		/* 互斥锁所有者标记 */
		Mutex_LinkOwner(Mutex, CurrentTCB);
		/* 退出临界区 */
		Mini_ExitCritical();
		return MiniRTOS_OK;
	}

	/* 互斥锁不可获取 */
	/* 不等待 */
	if (WaitTicks == 0U)
	{
		/* 退出临界区 */
		Mini_ExitCritical();
		return MiniRTOS_Timeout;
	}

	/* 判断互斥锁持有者会不会互相等待，造成死锁 */
	if (Mutex_WouldDeadlock(CurrentTCB, Mutex) != 0U)
	{
		/* 退出临界区 */
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 互斥锁不可获取 */
	/* 允许等待 */
	/* 设置当前任务等待该互斥锁 */
	CurrentTCB->pxWaitingMutex = Mutex;

	/* 进入互斥阻塞链表 */
	Mini_EventBlock(&Mutex->WaitList);

	/* 允许有限超时等待 */
	if (WaitTicks != MINIRTOS_WAIT_FOREVER)
	{
		Task_AddWaitList(CurrentTCB, WaitTicks);
	}

	/* 更新当前互斥锁优先级 */
	Mutex_RefreshHighest(Mutex);

	/* 更新当前互斥锁所有者优先级 */
	Mutex_RecomputeChain(Mutex->Owner);

	/* 退出临界区 */
	Mini_ExitCritical();

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 判断是否成功获取互斥锁 */
	status = (Mutex->Owner == CurrentTCB) ? MiniRTOS_OK : MiniRTOS_Timeout;

	/* 退出临界区 */
	Mini_ExitCritical();

	return status;
}

/* 释放互斥锁 */
MiniRTOS_State Mutex_Release(MiniMutex_t *Mutex)
{
	MiniTCB_t *nextOwner = NULL;

	if (Mutex == NULL)
	{
		return MiniRTOS_Invalid;
	}
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 互斥锁所有者不是当前任务 */
	if (Mutex->Owner != CurrentTCB)
	{
		/* 退出临界区 */
		Mini_ExitCritical();
		/* 直接退出 */
		return MiniRTOS_Error;
	}

	/* 权限释放失败 */
	if (Mutex_UnlinkOwner(Mutex, CurrentTCB) == 0U)
	{
		/* 退出临界区 */
		Mini_ExitCritical();
		/* 直接退出 */
		return MiniRTOS_Error;
	}

	/* 存在任务在等待该互斥锁 */
	if (Mutex->WaitList.NumberOfItems != 0U)
	{
		/* 唤醒下一个优先级最高的等待任务 */
		nextOwner = Mini_EventWakeup(&Mutex->WaitList);
		if (nextOwner != NULL)
		{
			nextOwner->pxWaitingMutex = NULL;
			/* 互斥锁所有者标记 */
			Mutex_LinkOwner(Mutex, nextOwner);
		}
	}

	/* 更新当前互斥锁最高优先级 */
	Mutex_RefreshHighest(Mutex);
	/* 更新链式互斥锁最高优先级 */
	Mutex_RecomputeChain(CurrentTCB);

	if (nextOwner != NULL)
	{
		/* 更新链式互斥锁最高优先级 */
		Mutex_RecomputeChain(nextOwner);
	}

	/* 请求任务调度 */
	if (MiniRTOS_Running == 1U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	/* 退出临界区 */
	Mini_ExitCritical();
	return MiniRTOS_OK;
}

/* 无限阻塞获取 */
uint8_t Mutex_Take(MiniMutex_t *Mutex)
{
	return (Mutex_TakeTimeout(Mutex, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 非阻塞获取 */
uint8_t Mutex_TryTake(MiniMutex_t *Mutex)
{
	return (Mutex_TakeTimeout(Mutex, 0U) == MiniRTOS_OK) ? 1U : 0U;
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

MiniRTOS_State Semaphore_TakeFromISR(MiniSemaphore_t *Semaphore)
{
	uint32_t SavedMask;

	if ((Semaphore == NULL) || (Semaphore->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	SavedMask = Mini_EnterCriticalFromISR();

	/* 信号量不可用 */
	if (Semaphore->uxCount == 0U)
	{
		/* 退出临界区 */
		Mini_ExitCriticalFromISR(SavedMask);
		return MiniRTOS_Timeout;
	}

	/* 信号量计数减一 */
	Semaphore->uxCount--;

	/* 恢复原有中断屏蔽状态 */
	Mini_ExitCriticalFromISR(SavedMask);

	return MiniRTOS_OK;
}

/* 释放信号量 */
MiniRTOS_State Semaphore_Give(MiniSemaphore_t *Semaphore)
{
	MiniTCB_t *TCB;

	if ((Semaphore == NULL) || (Semaphore->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

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

/* 释放信号量 */
MiniRTOS_State Semaphore_GiveFromISR(MiniSemaphore_t *Semaphore, uint8_t *pxHigherPriorityTaskWoken)
{
	MiniTCB_t *WakeTCB;
	uint32_t SavedMask;

	if ((Semaphore == NULL) || (pxHigherPriorityTaskWoken == NULL) ||
		(Semaphore->uxMaxCount == 0U))
	{
		return MiniRTOS_Invalid;
	}

	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() == 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	SavedMask = Mini_EnterCriticalFromISR();

	/* 信号量计数已满 */
	if (Semaphore->uxCount >= Semaphore->uxMaxCount)
	{
		Mini_ExitCriticalFromISR(SavedMask);
		return MiniRTOS_Error;
	}

	/* 信号量计数加一 */
	Semaphore->uxCount++;

	/* 唤醒一个正在等待的任务 */
	WakeTCB = Mini_EventWakeup(&Semaphore->WaitList);

	/* 判断是否需要立刻调度 */
	if (WakeTCB != NULL)
	{
		if (WakeTCB->uxPriority > CurrentTCB->uxPriority)
		{
			*pxHigherPriorityTaskWoken = 1U;
		}
	}

	/* 退出临界区 */
	Mini_ExitCriticalFromISR(SavedMask);

	return MiniRTOS_OK;
}

/* 无限阻塞获取信号量 */
uint8_t Semaphore_Take(MiniSemaphore_t *Semaphore)
{
	return (Semaphore_TakeTimeout(Semaphore, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 非阻塞获取信号量 */
uint8_t Semaphore_TryTake(MiniSemaphore_t *Semaphore)
{
	return (Semaphore_TakeTimeout(Semaphore, 0U) == MiniRTOS_OK) ? 1U : 0U;
}

/* 发送任务通知 */
MiniRTOS_State TaskNotify_Send(MiniTCB_t *TCB, uint32_t Value)
{
	/* 参数检查 */
	if (TCB == NULL)
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

	/* 设置任务通知值 */
	TCB->NotifyValue = Value;

	/* 设置通知为待接收状态 */
	TCB->NotifyState = Notify_Ready;

	if (TCB->EventListItem.pxContainer == &Notify_WaitList)
	{
		/* 从通知事件等待链表中移除 */
		List_RemoveNode(&TCB->EventListItem);

		/* 如果同时在系统时间阻塞链表中，解除超时等待 */
		if ((TCB->StateListItem.pxContainer == CurrentWaitList) ||
			(TCB->StateListItem.pxContainer == OverflowWaitList))
		{
			List_RemoveNode(&TCB->StateListItem);
		}

		/* 将目标任务恢复到就绪链表 */
		Task_AddReadyList(TCB);

		/* 判断是否需要立刻调度 */
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

MiniRTOS_State TaskNotify_SendFromISR(MiniTCB_t *TCB, uint32_t Value, uint8_t *pxHigherPriorityTaskWoken)
{
	uint32_t SavedMask;

	if ((TCB == NULL) || (pxHigherPriorityTaskWoken == NULL))
	{
		return MiniRTOS_Invalid;
	}

	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() < 16U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	SavedMask = Mini_EnterCriticalFromISR();

	/* 设置任务通知值 */
	TCB->NotifyValue = Value;

	/* 设置通知为待接收状态 */
	TCB->NotifyState = Notify_Ready;

	if (TCB->EventListItem.pxContainer == &Notify_WaitList)
	{
		/* 从通知事件等待链表中移除 */
		List_RemoveNode(&TCB->EventListItem);

		/* 如果同时在系统时间阻塞链表中，解除超时等待 */
		if ((TCB->StateListItem.pxContainer == CurrentWaitList) ||
			(TCB->StateListItem.pxContainer == OverflowWaitList))
		{
			List_RemoveNode(&TCB->StateListItem);
		}

		/* 将目标任务恢复到就绪链表 */
		Task_AddReadyList(TCB);

		/* 判断是否需要立刻调度 */
		if ((TCB->uxPriority > CurrentTCB->uxPriority))
		{
			*pxHigherPriorityTaskWoken = 1U;
		}
	}

	/* 退出临界区 */
	Mini_ExitCriticalFromISR(SavedMask);

	return MiniRTOS_OK;
}

/* 获取任务通知 */
MiniRTOS_State TaskNotify_WaitTimeout(uint32_t *Value, uint32_t WaitTicks)
{
	uint32_t StartTick;
	uint32_t ElapsedTick;
	uint32_t RemainingTick;

	/* 参数检查 */
	if (Value == NULL)
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

	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 1. 任务体通知中有数据 */
		/* 直接接收 */
		if (CurrentTCB->NotifyState == Notify_Ready)
		{
			/* 读取通知值 */
			*Value = CurrentTCB->NotifyValue;

			/* 通知已消费，恢复为空闲 */
			CurrentTCB->NotifyState = Notify_Idle;

			Mini_ExitCritical();

			return MiniRTOS_OK;
		}

		/* 2. 任务体通知中没有数据 */
		/* 不允许等待 */
		if (WaitTicks == 0U)
		{
			/* 未收到通知，恢复空闲状态 */
			CurrentTCB->NotifyState = Notify_Idle;

			Mini_ExitCritical();

			return MiniRTOS_Timeout;
		}

		/* 3. 任务体通知中没有数据 */
		/* 允许等待 */
		/* 判断是否死等 */
		if (WaitTicks != MINIRTOS_WAIT_FOREVER)
		{
			/* 该任务已经消耗时间 */
			ElapsedTick = MiniRTOS_GetTick() - StartTick;

			/* 消耗时间大于等待超时时间，即早就等待超时了 */
			if (ElapsedTick >= WaitTicks)
			{
				/* 清理通知等待状态 */
				CurrentTCB->NotifyState = Notify_Idle;

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

		/* 设置任务正在等待通知 */
		CurrentTCB->NotifyState = Notify_Wait;

		/* 4. 需要阻塞超时等待 */
		/* 加入事件阻塞列表、时间阻塞列表 */
		Mini_EventBlock(&Notify_WaitList);

		/* 判断是否无限期超时等待 */
		/* 无限期超时等待不加入时间阻塞列表 */
		if (RemainingTick != MINIRTOS_WAIT_FOREVER)
		{
			Task_AddWaitList(CurrentTCB, RemainingTick);
		}

		/* 退出临界区 */
		Mini_ExitCritical();
	}
}

/* 无限阻塞等待通知 */
uint8_t TaskNotify_Wait(uint32_t *Value)
{
	return (TaskNotify_WaitTimeout(Value, MINIRTOS_WAIT_FOREVER) == MiniRTOS_OK) ? 1U : 0U;
}

/* 非阻塞获取通知 */
uint8_t TaskNotify_TryWait(uint32_t *Value)
{
	return (TaskNotify_WaitTimeout(Value, 0U) == MiniRTOS_OK) ? 1U : 0U;
}
