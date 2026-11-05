#include "minirtos_api.h"
#include "main.h"

MiniTCB_t *volatile CurrentTCB = NULL;

volatile uint32_t MiniRTOS_Running = 0;
volatile uint32_t System_Tick = 0;

List_t Task_ReadyList[5];
List_t Task_WaitList_1;
List_t Task_WaitList_2;
List_t *CurrentWaitList = &Task_WaitList_1;
List_t *OverflowWaitList = &Task_WaitList_2;

void ListInit(List_t *List);
void Task_AddReadyList(MiniTCB_t *TCB);
static void TaskInit_LRerror(void);
static __asm void StartFirstTask(void);
void Get_CurrentTCB(void);
static void List_ItemRemove(MiniTCB_t *TCB);
void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time);

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
				return;
			}
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
	else
	{
		TCB->StateListItem.pxNext = &Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->StateListItem.pxPrevious = Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious;
		TCB->StateListItem.pxPrevious->pxNext = &TCB->StateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious = &TCB->StateListItem;
	}

	/* 列表节点数增加 */
	Task_ReadyList[TCB->uxPriority].NumberOfItems++;
}

void Task_StartScheduler(void)
{
	printf("Task_Scheduler_Init!\r\n");

	/* Systick滴答定时器，时钟配置 */
	HAL_SYSTICK_Config(72000000 / 1000);

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
	ListItem_t *Index;
	MiniTCB_t *TCB;
	List_t *List;

	/* 检查阻塞任务是否到期 */
	List = CurrentWaitList;

	while (List->NumberOfItems > 0)
	{
		/* 取得最早到期的任务 */
		Index = List->xListEnd.pxNext;

		/* 如果还没有到期，直接结束 */
		if (Index->pxValue > System_Tick)
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
		List->xIndex = Index->pxPrevious;
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

	uint32_t primask;
	
	if(delay_time == 0U)
	{
			SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
			return;
	}

	/* 保存中断状态 */
	primask = __get_PRIMASK();

	/* 进入临界区 */
	__disable_irq();

	/* 从就绪链表删除 */
	List_ItemRemove(CurrentTCB);

	/* 加入阻塞链表 */
	Task_AddWaitList(CurrentTCB, delay_time);

	/* 请求任务调度 */
	if (MiniRTOS_Running == 1)
	{
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}

	__DSB();

	/* 恢复原来的中断状态 */
	__set_PRIMASK(primask);
	__ISB();
}

void Task_AddWaitList(MiniTCB_t *TCB, uint32_t delay_time)
{
	ListItem_t *Index;
	ListItem_t *NewItem;
	List_t *List;

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
