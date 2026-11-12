#include "minirtos_task.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 系统定义项 */
uint32_t 							primask						=0		;
volatile uint32_t 		MiniRTOS_Running	=0		;
volatile uint32_t 		System_Tick				=NULL	;
MiniTCB_t *volatile 	CurrentTCB							;

/* 就绪列表定义项 */
List_t Task_ReadyList[5];

/* 阻塞列表定义项 */
List_t Task_WaitList_1		;
List_t Task_WaitList_2		;
List_t *CurrentWaitList 	;
List_t *OverflowWaitList	;

static void ListInit(List_t *List);
static __asm void StartFirstTask(void);
static void TaskInit_LRerror(void);
static void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time);

extern List_t Mutex_WaitList;
extern List_t SemaphoreSignal_WaitList;
extern List_t Notify_WaitList;


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
	
	CurrentWaitList  = &Task_WaitList_1;
	OverflowWaitList = &Task_WaitList_2;
	
	/* 互斥锁链表初始化 */
	ListInit(&Mutex_WaitList);
	
	/* 信号量链表初始化 */
	ListInit(&SemaphoreSignal_WaitList);
	
	/* 任务通知初始化 */
	ListInit(&Notify_WaitList);
}

/* 任务创建函数 */
MiniTCB_t * MiniCreateTask(TaskFunction_t pxTaskCode,
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
				return NULL;
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
			
			/* 任务通知值初始化 */
			TCB->NotifyValue=NULL;
			TCB->NotifyState=Notify_Free;
			
			printf("%s Task Init sucess\r\n", pcName);
			
			return TCB;
		}
		else
		{
			printf("%s Task Stack error\r\n", pcName);
			free(TCB);
			return NULL;
		}
	}
	else
	{
		printf("%s TCB Stack error\r\n", pcName);
		return NULL;
	}
}

/* 链表初始化 */
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

/* 将到期任务移出阻塞链表，进入就绪态 */
void MiniRTOS_WakeUntil(uint32_t Systick_Now)
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

/* 添加至就绪链表 */
void Task_AddList(MiniTCB_t *TCB,uint32_t pxValue,List_t *pxContainer)
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

/* 开启调度 */
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

/* 运行第一个任务 */
static  __asm void StartFirstTask(void)
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

/* 任务初始化任务栈时，LR异常返回函数 */
static void TaskInit_LRerror(void)
{
	printf("TaskInit_LR_error! \r\n");
	while (1)
	{
	}
}

/* 获取下一次任务体 */
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

/* 将任务体移除当前链表 */
void List_ItemRemove(MiniTCB_t *TCB)
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

/* RTOS系统延时函数 */
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

/* 添加延时链表 */
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

/* 进入临界区 */
void Mini_EnterCritical(void)
{
	/* 保存中断状态 */
	primask = __get_PRIMASK();

	/* 进入临界区 */
	__disable_irq();
	
}

/* 退出临界区 */
void Mini_ExitCritical(void)
{
	__DSB();

	/* 恢复原来的中断状态 */
	__set_PRIMASK(primask);
	__ISB();
	
}
