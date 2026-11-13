#include "minirtos_event.h"
#include "minirtos_task.h"
#include "minirtos.h"
#include "main.h"


/* 系统定义项 */
extern uint32_t 				primask			;
extern volatile 	uint32_t 	MiniRTOS_Running;
extern MiniTCB_t 	*volatile 	CurrentTCB		;
extern List_t Task_ReadyList[5]	;
extern List_t Task_WaitList_1	;
extern List_t Task_WaitList_2	;
extern List_t *CurrentWaitList 	;
extern List_t *OverflowWaitList	;
extern List_t Notify_WaitList	;

/* 互斥量初始化 */
void Mutex_Init(MiniMutex_t *Task_MutexSignal)
{
	/* 互斥锁所有者置空 */
	Task_MutexSignal->Owner = NULL;

	/* 初始化互斥锁等待链表 */
	ListInit(&Task_MutexSignal->WaitList);

}

/* 信号量初始化 */
void SemaphoreSignal_Init(MiniSemaphore_t *Task_SemaphoreSignal,uint16_t uxMaxCount)
{
	/* 初始化信号量计数 */
	Task_SemaphoreSignal->uxCount		=0			;

	/* 初始化信号量最大计数 */
	Task_SemaphoreSignal->uxMaxCount	=uxMaxCount	;

	/* 初始化信号量等待链表 */
	ListInit(&Task_SemaphoreSignal->WaitList)		;
	
}

/* 获取互斥锁 */
void MutexSignal_Take(MiniMutex_t *Task_MutexSignal)
{
	List_t *List;

	while(1)
	{
		
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 互斥锁空闲，直接获取 */
		if(Task_MutexSignal->Owner == NULL)
		{
			/* 互斥锁所有者设置为当前任务 */
			Task_MutexSignal->Owner = CurrentTCB;
			
			/* 退出临界区 */
			Mini_ExitCritical();
			break;
		}
		
		/* 禁止重复获取 */
		if(Task_MutexSignal->Owner == CurrentTCB)
		{
			/* 当前任务已持有互斥锁，直接退出临界区 */
			Mini_ExitCritical();
			break;
		}
		
		/* 没有互斥锁，获取失败 */
		List = Task_MutexSignal->Owner->StateListItem.pxContainer;
		/* 优先级继承，防止优先级反转 */
		if(Task_MutexSignal->Owner->uxPriority < CurrentTCB->uxPriority)
		{
			/* 优先级继承任务处于就绪列表 */
			if(List == &Task_ReadyList[Task_MutexSignal->Owner->uxPriority])
			{
				/* 重新设置互斥锁拥有者的运行优先级，并重新加入原有列表 */
				List_ItemRemove(Task_MutexSignal->Owner);
				Task_MutexSignal->Owner->uxPriority=CurrentTCB->uxPriority;
				Task_AddReadyList(Task_MutexSignal->Owner);
			}
			/* 优先级继承任务处于阻塞列表 */
			else if(List == CurrentWaitList || List == OverflowWaitList)
			{
				/* 重新设置互斥锁拥有者的运行优先级 */
				 Task_MutexSignal->Owner->uxPriority = CurrentTCB->uxPriority;
			}
			/* 优先级继承任务处于就绪列表或阻塞列表之外的列表，如 互斥锁等待 或 信号量等待 列表 */
			else
			{
				/* 重新设置互斥锁拥有者的运行优先级，并重新加入原有列表 */
				Task_MutexSignal->Owner->uxPriority=CurrentTCB->uxPriority;
				List_ItemRemove(Task_MutexSignal->Owner);
				Task_AddEventWaitList(Task_MutexSignal->Owner,Task_MutexSignal->Owner->uxPriority,List);
			}
		}
		
		/* 从就绪链表删除 */
		List_ItemRemove(CurrentTCB);
		
		/* 加入互斥锁阻塞链表 */
		Task_AddEventWaitList(CurrentTCB,CurrentTCB->uxPriority,&Task_MutexSignal->WaitList);

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
	if(Task_MutexSignal->Owner == CurrentTCB)
	{
		/* 释放互斥锁 */
		Task_MutexSignal->Owner = NULL;
		
		/* 任务优先级还原 */
		if(CurrentTCB->uxPriority != CurrentTCB->uxInitPriority)
		{
			/* 恢复当前任务的初始优先级 */
			CurrentTCB->uxPriority=CurrentTCB->uxInitPriority;
			List_ItemRemove(CurrentTCB);
			Task_AddReadyList(CurrentTCB);
		}

		/* 判断互斥锁等待链表是否为空 */
		if(Task_MutexSignal->WaitList.NumberOfItems > 0)
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


/* 取出信号量 */
void SemaphoreSignal_Take(MiniSemaphore_t *Task_SemaphoreSignal)
{
	while(1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();
		
		/* 判断信号量计数是否大于零 */
		if(Task_SemaphoreSignal->uxCount > 0)
		{
			/* 信号量计数减一 */
			Task_SemaphoreSignal->uxCount--;

			Mini_ExitCritical();
			break;
		}
		
		/* 没有信号量可以获取，进入阻塞 */
		List_ItemRemove(CurrentTCB);
		Task_AddEventWaitList(CurrentTCB,CurrentTCB->uxPriority,&Task_SemaphoreSignal->WaitList);
	
		if(MiniRTOS_Running == 1)
		{
			/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
			SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
		}
		
		/* 退出临界区 */
		Mini_ExitCritical();
	}
	return;
}

/* 释放信号量 */
void SemaphoreSignal_Release(MiniSemaphore_t *Task_SemaphoreSignal)
{

	/* 进入临界区 */
	Mini_EnterCritical();
	
	/* 判断信号量计数是否小于最大计数 */
	if(Task_SemaphoreSignal->uxCount < Task_SemaphoreSignal->uxMaxCount)
	{
		/* 信号量计数加一 */
		Task_SemaphoreSignal->uxCount++;
		/* 判断信号量等待链表是否为空 */
		if(Task_SemaphoreSignal->WaitList.NumberOfItems > 0)
		{
			/* 唤醒等待信号量的任务 */
			Task_WakeupEventWaitList(&Task_SemaphoreSignal->WaitList);

			/* 请求任务调度 */
			if(MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}
		}
	}

	/* 退出临界区 */
	Mini_ExitCritical();
}
/* 发送任务通知 */
void Mini_TaskNotify_Sent(MiniTCB_t *TCB,uint32_t Value)
{
	/* 进入临界区 */
	Mini_EnterCritical();

	/* 设置任务通知值 */
	TCB->NotifyValue 	= Value	;

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
		
	}else /* 通知已经发送，但任务未等待通知 */
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
	while(CurrentTCB->NotifyState != Notify_Ready)
	{
		/* 设置任务通知状态为等待 */
		CurrentTCB->NotifyState = Notify_Wait;
		/* 添加任务到事件等待链表 */
		List_ItemRemove(CurrentTCB);
		Task_AddEventWaitList(CurrentTCB,CurrentTCB->uxPriority,&Notify_WaitList);
		
		/* 请求调度 */
		if(MiniRTOS_Running == 1)
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
void Task_AddEventWaitList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer)
{
	ListItem_t	*Index  ;
	ListItem_t 	*NewItem;
	List_t 			*List   ;

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
	List_t 			*List	;
	ListItem_t 	*Index;
	MiniTCB_t 	*TCB	;
	
	List = pxContainer;
	
	if(List->NumberOfItems == 0)
	{
		return NULL;
	}
	
	Index = List->xListEnd.pxPrevious;
	
	/* 取得优先级最好且最早等待事件(互斥、信号量、任务通知)的任务 */
	while(Index->pxPrevious != &List->xListEnd)
	{
		if(Index->pxPrevious->pxValue != Index->pxValue)
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

