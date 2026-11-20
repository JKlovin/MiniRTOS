#include "minirtos_task.h"
#include "minirtos_event.h"

#include "main.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 系统定义项 */
volatile uint32_t MiniRTOS_Running = 0;
volatile uint32_t System_Tick = 0;
MiniTCB_t *volatile CurrentTCB;

/* 就绪列表定义项 */
List_t Task_ReadyList[UXPRIORITY_MAX];

/* 阻塞列表定义项 */
List_t Task_WaitList_1;
List_t Task_WaitList_2;
List_t *CurrentWaitList;
List_t *OverflowWaitList;
List_t Notify_WaitList;

/* 挂起列表定义项 */
static List_t Task_SuspendList;

/* 任务待删除链表 */
static List_t Task_DeleteList;

/* 空闲任务 TCB */
static MiniTCB_t *IdleTCB = NULL;

static volatile uint32_t uxCritical_Count = 0;
static volatile uint32_t uxSavedPrimask = 0;
static void TaskInit_LRerror(void);
static void IDLE_Handle(void *arg);
static void Task_ReapDeleted(void);

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

	/* 挂起链表初始化 */
	ListInit(&Task_SuspendList);

	/* 待删除链表初始化 */
	ListInit(&Task_DeleteList);

	/* 任务通知 */
	ListInit(&Notify_WaitList);

	/* 创建空闲任务 */
	IdleTCB = MiniCreateTask(IDLE_Handle, "Idle_task", 64, 0);
}

/* 空闲任务 */
static void IDLE_Handle(void *arg)
{
	while (1)
	{
		/* 清理等待删除的任务 */
		Task_ReapDeleted();

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

	/* 参数检查 */
	if ((pxTaskCode == NULL) || (uxStackDepth < MIN_STACK_SIZE) ||
		(uxPriority >= UXPRIORITY_MAX))
	{
		return NULL;
	}

	/* 创建TCB，申请TCB内存空间 */
	TCB = (MiniTCB_t *)malloc(sizeof(MiniTCB_t));
	if (TCB != NULL)
	{
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

			/* 初始化事件列表项 */
			TCB->EventListItem.pxValue = uxPriority;
			TCB->EventListItem.pxOwner = TCB;

			TCB->EventListItem.pxContainer = NULL;
			TCB->EventListItem.pxNext = NULL;
			TCB->EventListItem.pxPrevious = NULL;

			/* 初始化互斥锁关系 */
			TCB->pxWaitingMutex = NULL;
			TCB->pxHeldMutexList = NULL;
			TCB->uxEventWaitOrder = 0ULL;

			/* 任务通知值初始化 */
			TCB->NotifyValue = 0U;
			TCB->NotifyState = Notify_Idle;
			
			/* 加入就绪列表 */
			Task_AddReadyList(TCB);

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

		/* 判断是否正在等待互斥锁 */
		if (TCB->pxWaitingMutex != NULL)
		{
			Mutex_OnWaiterTimeout(TCB);
		}
		/* 其他普通事件超时 */
		else if (TCB->EventListItem.pxContainer != NULL)
		{
			List_RemoveNode(&TCB->EventListItem);
		}

		/* 从延时链表删除 */
		List_RemoveNode(&TCB->StateListItem);

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
	}
	else /* 如果列表中有节点，尾插法 */
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

/* RTOS系统延时函数 */
void MiniRTOS_delay(uint32_t delay_time)
{
	/* __get_IPSR() != 0U 表示处理器处于异常或中断上下文*/
	/* __get_PRIMASK() != 0U 表示处理器屏蔽中断*/
	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return;
	}

	/* 如果延时时间为0，立刻请求任务调度 */
	if (delay_time == 0U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
		return;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 从就绪链表删除 */
	List_RemoveNode(&CurrentTCB->StateListItem);

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
uint8_t MiniRTOS_DelayUntil(uint32_t *PreviousWakeupTime, uint32_t Delay_Time)
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
	List_RemoveNode(&CurrentTCB->StateListItem);

	/* 加入延时阻塞链表 */
	Task_AddWaitList(CurrentTCB, WakeupTime);

	if (MiniRTOS_Running == 1)
	{
		/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	Mini_ExitCritical();

	return 0;
}

/* 添加延时链表 */
void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time)
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

/* 进入临界区 */
uint32_t Mini_EnterCriticalFromISR(void)
{
	uint32_t SavedMask;

	/* 保存原有 PRIMASK 状态 */
	SavedMask = __get_PRIMASK();

	/* 屏蔽可配置优先级异常 */
	__disable_irq();

	/* 返回进入之前的中断屏蔽状态 */
	return SavedMask;
}

/* 退出临界区 */
void Mini_ExitCritical(void)
{
	if (uxCritical_Count == 0U)
	{
		return;
	}

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

/* 退出临界区 */
void Mini_ExitCriticalFromISR(uint32_t SavedMask)
{
	/* 确保前面的内存访问完成 */
	__DSB();

	/* 恢复进入前的 PRIMASK 状态 */
	__set_PRIMASK(SavedMask);

	/* 指令同步 */
	__ISB();
}

/* 将任务体移除当前链表 */
void List_RemoveNode(ListItem_t *Item)
{
	List_t *List;

	if (Item == NULL)
	{
		return;
	}

	/* 获取节点所属的链表 */
	List = Item->pxContainer;

	if (List == NULL)
	{
		return;
	}

	/* 如果 Index 正好指向待删除节点 */
	if (List->xIndex == Item)
	{
		List->xIndex = Item->pxNext;
	}

	/* 双向链表节点删除 */
	Item->pxPrevious->pxNext = Item->pxNext;
	Item->pxNext->pxPrevious = Item->pxPrevious;

	/* 链表节点数量减一 */
	List->NumberOfItems--;

	/* 清除节点原有的链表关系 */
	Item->pxNext = NULL;
	Item->pxPrevious = NULL;
	Item->pxContainer = NULL;
}

/* 任务挂起 */
MiniRTOS_State Task_Suspend(MiniTCB_t *TCB)
{
	MiniTCB_t *Target;
	List_t *StateList;
	uint8_t SelfSuspend;

	if ((MiniRTOS_Running == 0U) || (CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) || (__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* NULL 表示挂起当前任务 */
	Target = (TCB == NULL) ? CurrentTCB : TCB;

	/* 不允许挂起 Idle 任务 */
	if (Target == IdleTCB)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 暂时不允许挂起持有互斥锁的任务 */
	if (Target->pxHeldMutexList != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 暂时不允许挂起正在等待互斥锁的任务 */
	if (Target->pxWaitingMutex != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 暂时不允许挂起正在等待事件的任务 */
	if (Target->EventListItem.pxContainer != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 获取当前任务所在的状态链表 */
	StateList = Target->StateListItem.pxContainer;

	/* 只允许挂起就绪状态的任务 */
	if (Target->uxPriority >= UXPRIORITY_MAX ||
		StateList != &Task_ReadyList[Target->uxPriority])
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 判断是否挂起自己 */
	SelfSuspend = (Target == CurrentTCB) ? 1U : 0U;

	/* 从就绪链表移除 */
	List_RemoveNode(&Target->StateListItem);

	/* 设置挂起链表节点信息 */
	Target->StateListItem.pxValue = 0U;
	Target->StateListItem.pxOwner = Target;

	/* 尾插法 */
	Target->StateListItem.pxNext = &Task_SuspendList.xListEnd;
	Target->StateListItem.pxPrevious = Task_SuspendList.xListEnd.pxPrevious;

	Task_SuspendList.xListEnd.pxPrevious->pxNext = &Target->StateListItem;
	Task_SuspendList.xListEnd.pxPrevious = &Target->StateListItem;

	Target->StateListItem.pxContainer = &Task_SuspendList;
	Task_SuspendList.NumberOfItems++;

	/* 挂起当前运行任务，必须切换 */
	if (SelfSuspend != 0U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	/* 退出临界区 */
	Mini_ExitCritical();

	return MiniRTOS_OK;
}

/* 任务挂起恢复 */
MiniRTOS_State Task_Resume(MiniTCB_t *TCB)
{
	if (TCB == NULL)
	{
		return MiniRTOS_Invalid;
	}

	if ((MiniRTOS_Running == 0U) ||
		(CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) ||
		(__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* 检查任务是否处于挂起链表 */
	if (TCB->StateListItem.pxContainer != &Task_SuspendList)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 检查 */
	if ((TCB->uxPriority >= UXPRIORITY_MAX) ||
		(TCB->EventListItem.pxContainer != NULL) ||
		(TCB->pxWaitingMutex != NULL))
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 从挂起链表移除 */
	List_RemoveNode(&TCB->StateListItem);

	/* 加入对应优先级就绪链表 */
	Task_AddReadyList(TCB);

	/* 恢复的任务优先级更高，请求调度 */
	if (TCB->uxPriority > CurrentTCB->uxPriority)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	/* 退出临界区 */
	Mini_ExitCritical();

	return MiniRTOS_OK;
}

/* 任务挂起恢复 */
MiniRTOS_State Task_ResumeFromISR(MiniTCB_t *TCB, uint8_t *pxHigherPriorityTaskWoken)
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

	/* 检查任务是否处于挂起链表 */
	if (TCB->StateListItem.pxContainer != &Task_SuspendList)
	{
		Mini_ExitCriticalFromISR(SavedMask);
		return MiniRTOS_Error;
	}

	/* 检查 */
	if ((TCB->uxPriority >= UXPRIORITY_MAX) ||
		(TCB->EventListItem.pxContainer != NULL) ||
		(TCB->pxWaitingMutex != NULL))
	{
		Mini_ExitCriticalFromISR(SavedMask);
		return MiniRTOS_Error;
	}

	/* 从挂起链表移除 */
	List_RemoveNode(&TCB->StateListItem);

	/* 加入对应优先级就绪链表 */
	Task_AddReadyList(TCB);

	/* 恢复的任务优先级更高，请求调度 */
	if (TCB->uxPriority > CurrentTCB->uxPriority)
	{
		*pxHigherPriorityTaskWoken = 1U;
	}

	/* 退出临界区 */
	Mini_ExitCriticalFromISR(SavedMask);

	return MiniRTOS_OK;
}

/* 任务删除 */
MiniRTOS_State Task_Delete(MiniTCB_t *TCB)
{
	MiniTCB_t *Target;
	List_t *StateList;
	uint8_t SelfDelete;

	if ((MiniRTOS_Running == 0U) ||
		(CurrentTCB == NULL) ||
		(__get_IPSR() != 0U) ||
		(__get_PRIMASK() != 0U))
	{
		return MiniRTOS_Invalid;
	}

	/* 进入临界区 */
	Mini_EnterCritical();

	/* NULL 表示删除当前任务 */
	Target = (TCB == NULL) ? CurrentTCB : TCB;

	/* 禁止删除 Idle 任务 */
	if (Target == IdleTCB)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 不能删除持有互斥锁的任务 */
	if (Target->pxHeldMutexList != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 不能删除正在等待互斥锁的任务 */
	if (Target->pxWaitingMutex != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 暂不支持删除正在等待事件的任务 */
	if (Target->EventListItem.pxContainer != NULL)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 获取任务当前的状态链表 */
	StateList = Target->StateListItem.pxContainer;

	/* 检查任务优先级 */
	if (Target->uxPriority >= UXPRIORITY_MAX)
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 只允许删除就绪任务或挂起任务 */
	if ((StateList != &Task_ReadyList[Target->uxPriority]) &&
		(StateList != &Task_SuspendList))
	{
		Mini_ExitCritical();
		return MiniRTOS_Error;
	}

	/* 判断是否删除自己 */
	SelfDelete = (Target == CurrentTCB) ? 1U : 0U;

	/* 从原来的状态链表中移除 */
	List_RemoveNode(&Target->StateListItem);

	/* 设置删除链表节点 */
	Target->StateListItem.pxValue = 0U;
	Target->StateListItem.pxOwner = Target;

	/* 插入待删除链表 */
	Target->StateListItem.pxNext = &Task_DeleteList.xListEnd;
	Target->StateListItem.pxPrevious = Task_DeleteList.xListEnd.pxPrevious;

	Task_DeleteList.xListEnd.pxPrevious->pxNext = &Target->StateListItem;
	Task_DeleteList.xListEnd.pxPrevious = &Target->StateListItem;

	Target->StateListItem.pxContainer = &Task_DeleteList;
	Task_DeleteList.NumberOfItems++;

	/* 当前任务自删除必须立即请求切换 */
	if (SelfDelete != 0U)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	/* 退出临界区 */
	Mini_ExitCritical();

	/*
	 * 自删除后不能返回任务的正常业务流程。
	 * PendSV 会切换到其他就绪任务。
	 */
	if (SelfDelete != 0U)
	{
		for (;;)
		{
			__NOP();
		}
	}

	return MiniRTOS_OK;
}

/* 回收已经删除的任务 */
static void Task_ReapDeleted(void)
{
	MiniTCB_t *TCB;
	ListItem_t *Item;

	while (1)
	{
		/* 进入临界区 */
		Mini_EnterCritical();

		/* 没有等待回收的任务 */
		if (Task_DeleteList.NumberOfItems == 0U)
		{
			Mini_ExitCritical();
			break;
		}

		/* 取出删除链表第一个任务 */
		Item = Task_DeleteList.xListEnd.pxNext;

		TCB = (MiniTCB_t *)Item->pxOwner;

		/* 从删除链表中移除 */
		List_RemoveNode(Item);

		/* 退出临界区 */
		Mini_ExitCritical();

		/*
		 * 此时任务已经不再参与调度。
		 * Idle 正在使用自己的 PSP 栈，
		 * 可以安全释放目标任务的内存。
		 */

		/* 释放任务栈 */
		free(TCB->pxStackBase);

		/* 释放任务控制块 */
		free(TCB);
	}
}
