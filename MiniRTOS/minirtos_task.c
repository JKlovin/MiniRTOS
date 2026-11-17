#include "minirtos_task.h"
#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define UXPRIORITY_MAX 5
#define MIN_STACK_SIZE 32

/* 系统定义项 */
volatile uint32_t MiniRTOS_Running 	= 0;
volatile uint32_t System_Tick		= 0;
MiniTCB_t *volatile CurrentTCB;

/* 就绪列表定义项 */
List_t Task_ReadyList[UXPRIORITY_MAX];

/* 阻塞列表定义项 */
List_t Task_WaitList_1	;
List_t Task_WaitList_2	;
List_t *CurrentWaitList	;
List_t *OverflowWaitList;
List_t Notify_WaitList	;

static volatile uint32_t uxCritical_Count	= 0;
static volatile uint32_t uxSavedPrimask		= 0;
static void TaskInit_LRerror(void);
static void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time);
static void IDLE_Handle(void *arg);

/* 系统初始化函数 */
void MiniRTOS_Init(void)
{
	/* 就绪链表初始化 */
	for (int i = 0; i < UXPRIORITY_MAX; i++)
	{
		ListInit(&Task_ReadyList[i]);
	}

	/* 阻塞链表初始化 */
	ListInit(&Task_WaitList_1);
	ListInit(&Task_WaitList_2);

	CurrentWaitList = &Task_WaitList_1;
	OverflowWaitList = &Task_WaitList_2;
	
	/* 任务通知 */
	ListInit(&Notify_WaitList);
	
	/* 创建空闲任务 */
	MiniCreateTask(IDLE_Handle, "Idle_task",64, 0 );
	
}

/* 空闲任务 */
static void IDLE_Handle(void *arg)
{
  while (1)
  {
		__WFI();
  }
}

/* 任务创建函数 */
MiniTCB_t *MiniCreateTask(TaskFunction_t pxTaskCode,
						  const char *const pcName,
						  const uint32_t uxStackDepth,
						  unsigned long uxPriority)
{
	MiniTCB_t *TCB;
	uint32_t *pxStack;
	static int IDLE_Flag = 0;

	/* 创建TCB，申请TCB内存空间 */
	TCB = (MiniTCB_t *)malloc(sizeof(MiniTCB_t));
	if (TCB != NULL)
	{
		/* 任务函数指针检查 */
		if (pxTaskCode == NULL)
		{
				printf("%s Task Code error\r\n", pcName);

				return NULL;
		}
		
		/* 任务栈深度检查 */
		if (uxStackDepth < MIN_STACK_SIZE)
		{
				printf("%s Task StackDepth error\r\n", pcName);

				return NULL;
		}
		
		/* 创建任务堆栈，申请堆栈内存空间 */
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

			/* 构建初始任务堆栈 */
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
			if (uxPriority >= UXPRIORITY_MAX)
			{
				printf("%s Task Priority error\r\n", pcName);

				free(TCB);
				free(pxStack);
				return NULL;
			}

			TCB->uxInitPriority = uxPriority;
			TCB->uxPriority = uxPriority;

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
			Task_AddReadyList(TCB);

			/* 任务通知值初始化 */
			TCB->NotifyValue = NULL;
			TCB->NotifyState = Notify_Idle;

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
void ListInit(List_t *List)
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
		Task_AddReadyList(TCB);
	}
}

/* 添加至就绪链表 */
void Task_AddReadyList(MiniTCB_t *TCB)
{
	/* 列表节点归属哪一个TCB */
	TCB->StateListItem.pxOwner = TCB;

	/* 列表节点数值设置 */
	TCB->StateListItem.pxValue = TCB->uxPriority;

	/* 列表节点归属哪一个列表 */
	TCB->StateListItem.pxContainer = &Task_ReadyList[TCB->uxPriority];

	/* 如果列表中没有节点 */
	if (Task_ReadyList[TCB->uxPriority].NumberOfItems == 0)
	{
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious = &TCB->StateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxNext = &TCB->StateListItem;
		TCB->StateListItem.pxNext = &Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->StateListItem.pxPrevious = &Task_ReadyList[TCB->uxPriority].xListEnd;

		Task_ReadyList[TCB->uxPriority].xIndex = &TCB->StateListItem;
	}else/* 如果列表中有节点，尾插法 */
	{
		TCB->StateListItem.pxNext = &Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->StateListItem.pxPrevious = Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious;
		TCB->StateListItem.pxPrevious->pxNext = &TCB->StateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious = &TCB->StateListItem;
	}

	/* 列表节点数增加 */
	Task_ReadyList[TCB->uxPriority].NumberOfItems++;
}

/* 开启调度 */
void Task_StartScheduler(void)
{
	printf("Task_Scheduler_Init!\r\n");

	/* Systick滴答定时器，时钟配置 */
	HAL_SYSTICK_Config(72000000 / 1000);

	/* 初始化System_Tick */
	System_Tick = 0;

	Get_CurrentTCB();
	/* 开启第一个任务 */
	StartFirstTask();
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
	for (i = (int)UXPRIORITY_MAX - 1; i >= 0; i--)
	{
		List = &Task_ReadyList[i];

		/* 如果列表中没有节点 */
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
	/* 如果延时时间为0，立刻请求任务调度 */
	if (delay_time == 0U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
		return;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 从就绪链表删除 */
	List_ItemRemove(CurrentTCB);

	/* 加入阻塞链表 */
	Task_AddWaitList(CurrentTCB, delay_time);

	/* 请求任务调度 */
	if (MiniRTOS_Running == 1)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	/* 退出临界区 */
	Mini_ExitCritical();
}

/* 获取系统时间SystemTick */
uint32_t MiniRTOS_GetTick(void)
{
	return System_Tick;
}

/* 固定周期任务延时 */
uint8_t MiniRTOS_DelayUntil(uint32_t *PreviousWakeupTime,uint32_t Delay_Time)
{
	uint32_t WakeupTime;
	uint32_t ElapsedTick;
	uint32_t CurrentTick;
	
	/* 参数检查 */
	if ((PreviousWakeupTime == NULL) || (Delay_Time == 0U))
	{
			return 2U;
	}
	
	/* 只允许运行中的任务调用 */
	if ((MiniRTOS_Running == 0U) || (__get_IPSR() != 0U) ||
				(__get_PRIMASK() != 0U) || (CurrentTCB == NULL))
	{
			return 2U;
	}
	
	/* 进入临界区 */
	Mini_EnterCritical();
	
	/* 获取当前系统Tick */
	CurrentTick = MiniRTOS_GetTick();

	/* 计算已经过去的Tick */
  ElapsedTick = CurrentTick - *PreviousWakeupTime;
	
	/* 更新下一次计划唤醒时刻 */
	*PreviousWakeupTime += Delay_Time;
	
	/* 判断是否错过计划唤醒时刻 */
	if (ElapsedTick >= Delay_Time)
	{
			Mini_ExitCritical();
			return 1U;
	}
		
  /* 计算还需要阻塞的Tick */
  WakeupTime = Delay_Time - ElapsedTick;
		
	/* 当前任务移出就绪链表 */
	List_ItemRemove(CurrentTCB);
	
	/* 加入延时阻塞链表 */
	Task_AddWaitList(CurrentTCB,WakeupTime);
	
	if(MiniRTOS_Running == 1)
	{
		/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
	
	Mini_ExitCritical();
	
	return 0;
}

/* 添加延时链表 */
static void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time)
{
	ListItem_t *Index;
	ListItem_t *NewItem;
	List_t *List;

	/* 获取任务对应的链表节点 */
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

	/* 链表节点数量加一 */
	List->NumberOfItems++;
}

/* 进入临界区 */
void Mini_EnterCritical(void)
{
	uint32_t Primask;

	/* 保存中断状态 */
	Primask = __get_PRIMASK();

	/* 进入临界区 */
	__disable_irq();

	/* 首次进入临界区时保存中断状态 */
	if (uxCritical_Count == 0)
	{
		uxSavedPrimask = Primask;
	}

	/* 临界区计数加一 */
	uxCritical_Count++;
}

/* 退出临界区 */
void Mini_ExitCritical(void)
{
	/* 临界区计数减一 */
	if (uxCritical_Count > 0)
	{
		uxCritical_Count--;
	}

	/* 退出临界区 */
	if (uxCritical_Count == 0)
	{
		__DSB();
		__set_PRIMASK(uxSavedPrimask);
		__ISB();
	}
}

void MiniRTOS_TickHandler(void)
{
	System_Tick++;

	/* 检测系统 Tick 溢出,交换链表指针 */
	if(System_Tick == 0)
	{
		List_t *Temp = CurrentWaitList;
		CurrentWaitList = OverflowWaitList;
		OverflowWaitList = Temp;
	}

	if(MiniRTOS_Running == 1)
	{
		/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
}

