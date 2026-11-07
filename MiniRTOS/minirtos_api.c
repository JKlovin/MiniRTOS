#include "minirtos_api.h"
#include "main.h"
/*-------------------------------------*//*-------------------------------------*/
/*-------------------------------------*/
/* 系统定义项 */
uint32_t primask;
volatile uint32_t MiniRTOS_Running 	= 0		;
volatile uint32_t System_Tick 			= 0		;
MiniTCB_t *volatile CurrentTCB 			= NULL;

static void ListInit(List_t *List);
static __asm void StartFirstTask(void);
static void TaskInit_LRerror(void);

void Get_CurrentTCB(void);
static void List_ItemRemove(MiniTCB_t *TCB);
static void Mini_EnterCritical(void);
static void Mini_ExitCritical(void);

/* 就绪列表定义项 */
static void Task_AddList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer);

/* 阻塞列表定义项 */
static List_t Task_ReadyList[5];
static List_t Task_WaitList_1;
static List_t Task_WaitList_2;
List_t *CurrentWaitList 	= &Task_WaitList_1;
List_t *OverflowWaitList 	= &Task_WaitList_2;

static void Task_AddWaitList(MiniTCB_t *TCB,uint32_t delay_time);
static void MiniRTOS_WakeUntil(uint32_t Systick_Now);

/* 互斥锁定义项 */
static MiniMutex_t Task_MutexSignal;

static void MiniRTOS_Wakeup_MutexWaitTask(void);
static void Task_AddEventWaitList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer);

/* 信号量定义项 */
static MiniSemaphore_t Task_SemaphoreSignal;

static void MiniRTOS_Wakeup_SemaphoreWaitTask(void);

/*-------------------------------------*/
/*-------------------------------------*//*-------------------------------------*/

/* 系统初始化函数 */
void MiniRTOS_Init(void)
{
	/* 就绪链表初始化 */
	for (int i = 0; i < 5; i++)
	{
		ListInit(&Task_ReadyList[i]);
	}

	/* 阻塞链表初始化 */
	ListInit(&Task_WaitList_1);
	ListInit(&Task_WaitList_2);
	
	/* 互斥锁初始化 */
	Task_MutexSignal.Owner=NULL;
	ListInit(&Task_MutexSignal.WaitList);
	
	/* 信号量初始化 */
	Task_SemaphoreSignal.uxCount=0;
	Task_SemaphoreSignal.uxMaxCount=1;
	ListInit(&Task_SemaphoreSignal.WaitList);
	
}

/* 任务创建函数 */
void MiniCreateTask(TaskFunction_t pxTaskCode,
					const char *const pcName,
					const uint32_t uxStackDepth,
					unsigned long uxPriority)
{
	MiniTCB_t *TCB;
	uint32_t *pxStack;
	static int IDLE_Flag = 0;

	TCB = (MiniTCB_t *)malloc(sizeof(MiniTCB_t));
	if (TCB != NULL)
	{
		pxStack = (uint32_t *)malloc(sizeof(uint32_t) * uxStackDepth);
		if (pxStack != NULL)
		{

			/* -------------------------------------------------------------- */
			/* TCB Init */
			/* -------------------------------------------------------------- */
			/* 默认第一个创建任务为Idle_task */
			if (IDLE_Flag == 0)
			{
				CurrentTCB = TCB;
				IDLE_Flag++;
			}

			/* 任务栈初始化 */
			TCB->pxStackBase = pxStack;
			TCB->pxTopOfStack = &(TCB->pxStackBase)[uxStackDepth];

			/* 栈指针字节对齐 */
			TCB->pxTopOfStack = (uint32_t *)(((uint32_t)TCB->pxTopOfStack) & (~((uint32_t)0x0007)));

			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0x01000000; // xPSR
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = ((uint32_t)pxTaskCode) & ((uint32_t)0xFFFFFFFE); // R15 PC
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = (uint32_t)TaskInit_LRerror; // R14 LR
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R12
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R3
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R2
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R1
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R0
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R11
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R10
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R9
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R8
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R7
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R6
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R5
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack) = 0; // R4

			/* 设置任务优先级 */
			if (uxPriority >= 5)
			{
				printf("%s Task Priority error\r\n", pcName);
				free(TCB);
				free(pxStack);
				return;
			}
			
			TCB->uxInitPriority = uxPriority;
			TCB->uxPriority 		= uxPriority;

			/* 设置任务名称 */
			if (pcName != NULL)
			{
				strncpy(TCB->pcName, pcName, sizeof(TCB->pcName) - 1);
				TCB->pcName[sizeof(TCB->pcName) - 1] = '\0';
			}
			else
			{
				TCB->pcName[0] = '\0';
			}

			/* 加入就绪列表 */
			Task_AddList(TCB,TCB->uxPriority,&Task_ReadyList[TCB->uxPriority]);
			printf("%s Task Init sucess\r\n", pcName);
		}
		else
		{
			printf("%s Task Stack error\r\n", pcName);
			free(TCB);
		}
	}
	else
	{
		printf("%s TCB Stack error\r\n", pcName);
	}
}

static void ListInit(List_t *List)
{
	/* 列表初始化 */
	List->NumberOfItems = 0;
	List->xIndex = &List->xListEnd;
	List->xListEnd.pxValue = 0xFFFFFFFF;
	List->xListEnd.pxOwner = NULL;
	List->xListEnd.pxContainer = List;
	List->xListEnd.pxNext = &List->xListEnd;
	List->xListEnd.pxPrevious = &List->xListEnd;
}

static void Task_AddList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer)
{
	/* 列表节点归属哪一个TCB */
	TCB->StateListItem.pxOwner = TCB;

	/* 列表节点数值设置 */
	TCB->StateListItem.pxValue = pxValue;

	/* 列表节点归属哪一个列表 */
	TCB->StateListItem.pxContainer = pxContainer;

	/* 如果列表中没有节点 */
	if (pxContainer->NumberOfItems == 0)
	{
		pxContainer->xListEnd.pxPrevious = &TCB->StateListItem;
		pxContainer->xListEnd.pxNext = &TCB->StateListItem;
		TCB->StateListItem.pxNext = &pxContainer->xListEnd;
		TCB->StateListItem.pxPrevious = &pxContainer->xListEnd;

		pxContainer->xIndex = &TCB->StateListItem;
	}
	else
	{
		TCB->StateListItem.pxNext = &pxContainer->xListEnd;
		TCB->StateListItem.pxPrevious = pxContainer->xListEnd.pxPrevious;
		TCB->StateListItem.pxPrevious->pxNext = &TCB->StateListItem;
		pxContainer->xListEnd.pxPrevious = &TCB->StateListItem;
	}

	/* 列表节点数增加 */
	pxContainer->NumberOfItems++;
}

void Task_StartScheduler(void)
{
	printf("Task_Scheduler_Init!\r\n");

	/* Systick滴答定时器，时钟配置 */
	HAL_SYSTICK_Config(72000000 / 1000);
  System_Tick = 0;
	
	Get_CurrentTCB();
	/* 开启第一个任务 */
	StartFirstTask();
	
}

static __asm void StartFirstTask(void)
{
	PRESERVE8

	/* 从中断向量表中拿到MSP指针初始化值 */
	ldr r0, = 0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]

	/* 重新初始化MSP指针 */
	msr msp,r0
	
	/* 打开全局中断 */
	cpsie i
	cpsie f

	/* 内存同步 指令同步 */
	dsb
	isb
	/* SVC异常*/
	svc 0
	nop
	nop
}

static void TaskInit_LRerror(void)
{
	printf("TaskInit_LR_error! \r\n");
	while (1)
	{
	}
}

void Get_CurrentTCB(void)
{
	int i;
	List_t *List;

  /* 唤醒当前时间之前已经到期的任务 */
  MiniRTOS_WakeUntil(System_Tick);

	/* 查找最高优先级就绪任务 */
	for (i = 4; i >= 0; i--)
	{
		List = &Task_ReadyList[i];

		if (List->NumberOfItems == 0)
		{
			continue;
		}

		/* 防止 Index 指向尾节点 */
		if (List->xIndex == &List->xListEnd)
		{
			List->xIndex = List->xIndex->pxNext;
		}

		/* 选择当前任务 */
		CurrentTCB = (MiniTCB_t *)List->xIndex->pxOwner;

		/* 更新轮转指针 */
		List->xIndex = List->xIndex->pxNext;

		/* 防止 Index 指向尾节点 */
		if (List->xIndex == &List->xListEnd)
		{
			List->xIndex = List->xIndex->pxNext;
		}
		break;
	}
}

static void List_ItemRemove(MiniTCB_t *TCB)
{
	ListItem_t *Index;
	List_t *List;

	/* 获取当前任务对应的链表节点 */
	Index = &TCB->StateListItem;

	/* 获取节点所属的链表 */
	List = Index->pxContainer;

	/* 如果 Index 正好指向待删除节点 */
	if (List->xIndex == Index)
	{
		List->xIndex = Index->pxNext;
	}

	/* 双向链表节点删除 */
	Index->pxPrevious->pxNext = Index->pxNext;
	Index->pxNext->pxPrevious = Index->pxPrevious;

	/* 链表节点数量减一 */
	List->NumberOfItems--;

	/* 清除节点原有的链表关系 */
	Index->pxNext = NULL;
	Index->pxPrevious = NULL;
	Index->pxContainer = NULL;
}

void MiniRTOS_delay(uint32_t delay_time)
{	
	if(delay_time == 0U)
	{
			SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			return;
	}
	
	/* 进入临界区 */
	Mini_EnterCritical();
	
	/* 从就绪链表删除 */
	List_ItemRemove(CurrentTCB);

	/* 加入阻塞链表 */
	Task_AddWaitList(CurrentTCB,delay_time);

	/* 请求任务调度 */
	if (MiniRTOS_Running == 1)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
	
	/* 退出临界区 */
	Mini_ExitCritical();
	
}

static void Task_AddEventWaitList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer)
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

	/* 将新节点插入到 Index 前面 *//* 尾插法 */
	NewItem->pxNext = Index;
	NewItem->pxPrevious = Index->pxPrevious;
	Index->pxPrevious->pxNext = NewItem;
	Index->pxPrevious = NewItem;

	/* 更新链表归属 */
	NewItem->pxContainer = List;

	List->NumberOfItems++;

}

static void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time)
{
	ListItem_t	*Index  ;
	ListItem_t 	*NewItem;
	List_t 			*List   ;

	NewItem = &TCB->StateListItem;

	/* 设置唤醒时间 */
	NewItem->pxValue = System_Tick + delay_time;

	/* 阻塞列表溢出 */
	if (NewItem->pxValue >= System_Tick)
	{
		List = CurrentWaitList;
	}
	else
	{
		List = OverflowWaitList;
	}

	NewItem->pxOwner = TCB;

	/* 从头节点开始查找 */
	Index = List->xListEnd.pxNext;

	/* 按唤醒时间从小到大排列 */
	while (Index != &List->xListEnd)
	{
		if (Index->pxValue > NewItem->pxValue)
		{
			break;
		}

		Index = Index->pxNext;
	}

	/* 将新节点插入到 Index 前面 */
	NewItem->pxNext = Index;
	NewItem->pxPrevious = Index->pxPrevious;
	Index->pxPrevious->pxNext = NewItem;
	Index->pxPrevious = NewItem;

	/* 更新链表归属 */
	NewItem->pxContainer = List;

	List->NumberOfItems++;
}

/* 获取互斥锁 */
void MutexSignal_Take(void)
{
	List_t *List;

	while(1)
	{
		
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 互斥锁空闲，直接获取 */
		if(Task_MutexSignal.Owner == NULL)
		{
			Task_MutexSignal.Owner = CurrentTCB;
			
			/* 退出临界区 */
			Mini_ExitCritical();
			break;
		}
		
		/* 禁止重复获取 */
		if(Task_MutexSignal.Owner == CurrentTCB)
		{
			/* 退出临界区 */
			Mini_ExitCritical();
			break;
		}
		
		/* 没有互斥锁，获取失败 */
		List = Task_MutexSignal.Owner->StateListItem.pxContainer;
		/* 优先级继承，防止优先级反转 */
		if(Task_MutexSignal.Owner->uxPriority < CurrentTCB->uxPriority)
		{
			/* 优先级继承任务处于就绪列表 */
			if(List == &Task_ReadyList[Task_MutexSignal.Owner->uxPriority])
			{
				List_ItemRemove(Task_MutexSignal.Owner);
				Task_MutexSignal.Owner->uxPriority=CurrentTCB->uxPriority;
				Task_AddList(Task_MutexSignal.Owner,Task_MutexSignal.Owner->uxPriority,&Task_ReadyList[Task_MutexSignal.Owner->uxPriority]);
			}
			/* 优先级继承任务处于 互斥锁等待 或 信号量等待 列表 */
			else if(List == &Task_MutexSignal.WaitList || List == &Task_SemaphoreSignal.WaitList)
			{
				Task_MutexSignal.Owner->uxPriority=CurrentTCB->uxPriority;
				List_ItemRemove(Task_MutexSignal.Owner);
				Task_AddEventWaitList(Task_MutexSignal.Owner,Task_MutexSignal.Owner->uxPriority,List);
			}
			/* 优先级继承任务处于阻塞列表 */
			else
			{}

		}
		
		/* 从就绪链表删除 */
		List_ItemRemove(CurrentTCB);
		
		/* 加入互斥锁阻塞链表 */
		Task_AddEventWaitList(CurrentTCB,CurrentTCB->uxPriority,&Task_MutexSignal.WaitList);

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
void MutexSignal_Release(void)
{
	/* 进入临界区 */
	Mini_EnterCritical();

	if(Task_MutexSignal.Owner == CurrentTCB)
	{
		/* 释放互斥锁 */
		Task_MutexSignal.Owner = NULL;
		
		/* 任务优先级还原 */
		if(CurrentTCB->uxPriority != CurrentTCB->uxInitPriority)
		{
			CurrentTCB->uxPriority=CurrentTCB->uxInitPriority;
			List_ItemRemove(CurrentTCB);
			Task_AddList(CurrentTCB,CurrentTCB->uxPriority,&Task_ReadyList[CurrentTCB->uxPriority]);
		}

		if(Task_MutexSignal.WaitList.NumberOfItems > 0)
		{
			/* 唤醒等待互斥锁的任务 */
			MiniRTOS_Wakeup_MutexWaitTask();
		}
	}
	
	/* 请求任务调度 */
	if (MiniRTOS_Running == 1)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
	
	/* 退出临界区 */
	Mini_ExitCritical();

}

static void MiniRTOS_WakeUntil(uint32_t Systick_Now)
{
	List_t *List;
	ListItem_t *Index;
  MiniTCB_t *TCB;
	
		/* 检查阻塞任务是否到期 */
	List = CurrentWaitList;

	while (List->NumberOfItems > 0)
	{
		/* 取得最早到期的任务 */
		Index = List->xListEnd.pxNext;

		/* 如果还没有到期，直接结束 */
		if (Index->pxValue > Systick_Now)
		{
			break;
		}

		/* 先保存待唤醒任务的 TCB */
		TCB = (MiniTCB_t *)Index->pxOwner;

		/* 从阻塞链表中移除 */
		List_ItemRemove(TCB);

		/* 加入就绪链表 */
		Task_AddList(TCB,TCB->uxPriority,&Task_ReadyList[TCB->uxPriority]);
	}
}

static void MiniRTOS_Wakeup_MutexWaitTask(void)
{
	List_t 			*List	;
	ListItem_t 	*Index;
  MiniTCB_t 	*TCB	;
	
	List=&Task_MutexSignal.WaitList;
	
	if(List->NumberOfItems == 0)
	{
		return;
	}
	
	Index = List->xListEnd.pxPrevious;
	
	/* 取得优先级最好且最早等待互斥锁的任务 */
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
	Task_AddList(TCB,TCB->uxPriority,&Task_ReadyList[TCB->uxPriority]);

}

void SemaphoreSignal_Take(void)
{
	while(1)
	{
		
		/* 进入临界区 */
		Mini_EnterCritical();
		
		if(Task_SemaphoreSignal.uxCount > 0)
		{
			Task_SemaphoreSignal.uxCount--;
			__set_PRIMASK(primask);
			break;
			
		}
		
		List_ItemRemove(CurrentTCB);
		Task_AddEventWaitList(CurrentTCB,CurrentTCB->uxPriority,&Task_SemaphoreSignal.WaitList);
	
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

void SemaphoreSignal_Release(void)
{

	/* 进入临界区 */
	Mini_EnterCritical();
	
	if(Task_SemaphoreSignal.uxCount < Task_SemaphoreSignal.uxMaxCount)
	{
		Task_SemaphoreSignal.uxCount++;
		if(Task_SemaphoreSignal.WaitList.NumberOfItems > 0)
		{
			MiniRTOS_Wakeup_SemaphoreWaitTask();

			if(MiniRTOS_Running == 1)
			{
				SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			}
		}
	}
	
	/* 退出临界区 */
	Mini_ExitCritical();

}

static void MiniRTOS_Wakeup_SemaphoreWaitTask(void)
{
	List_t 			*List	;
	ListItem_t 	*Index;
  MiniTCB_t 	*TCB	;
	
	List=&Task_SemaphoreSignal.WaitList;
	
	if(List->NumberOfItems == 0)
	{
		return;
	}
	
	Index = List->xListEnd.pxPrevious;
	
	/* 取得优先级最好且最早等待信号量的任务 */
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
	Task_AddList(TCB,TCB->uxPriority,&Task_ReadyList[TCB->uxPriority]);

}

static void Mini_EnterCritical(void)
{
	/* 保存中断状态 */
	primask = __get_PRIMASK();

	/* 进入临界区 */
	__disable_irq();
	
}

static void Mini_ExitCritical(void)
{
	__DSB();

	/* 恢复原来的中断状态 */
	__set_PRIMASK(primask);
	__ISB();
	
}

