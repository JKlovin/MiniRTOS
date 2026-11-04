#include "minirtos_api.h"
#include "main.h"


MiniTCB_t * volatile CurrentTCB = NULL ;
volatile uint32_t MiniRTOS_Running = 0 ;

List_t Task_ReadyList[5];
List_t Task_WaitList_1;
List_t Task_WaitList_2;

void ListInit(List_t *List);
void Task_AddList(MiniTCB_t *TCB);
static void TaskInit_LRerror(void);
static __asm void StartFirstTask(void);
void Get_CurrentTCB(void);

/* 系统初始化函数 */
void MiniRTOS_Init(void)
{
	/* 就绪链表初始化 */
	for(int i=0;i<5;i++)
	{
		ListInit(&Task_ReadyList[i]);
	}

	/* 阻塞链表初始化 */
	ListInit(&Task_WaitList_1);
	ListInit(&Task_WaitList_2);

}

/* 任务创建函数 */
void MiniCreateTask( TaskFunction_t pxTaskCode,
                     const char * const pcName,
                     const uint32_t uxStackDepth,
                     unsigned long uxPriority)
{
	MiniTCB_t *TCB;
	uint32_t  *pxStack;
	
	TCB = (MiniTCB_t *) malloc(sizeof(MiniTCB_t));
	if(TCB != NULL)
	{
		pxStack=(uint32_t *) malloc(sizeof(uint32_t)*uxStackDepth);
		if(pxStack !=	NULL)
		{
			
			/* -------------------------------------------------------------- */	
			/* TCB Init */
			/* -------------------------------------------------------------- */	
			TCB->pxStackBase=pxStack;
			TCB->pxTopOfStack=&(TCB->pxStackBase)[uxStackDepth];

			TCB->pxTopOfStack =  (uint32_t *)( ( (uint32_t) TCB->pxTopOfStack ) & ( ~( (uint32_t) 0x0007) ) );

			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0x01000000;											//xPSR
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= ((uint32_t) pxTaskCode) & ((uint32_t) 0xfffffffe);	//R15 PC
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= (uint32_t) TaskInit_LRerror;							//R14 LR
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R12
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R3
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R2
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R1
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R0
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R11
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R10
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R9
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R8
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R7
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R6
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R5
			TCB->pxTopOfStack--;
			*(TCB->pxTopOfStack)= 0;													//R4

			if(uxPriority >= 5)
			{
					printf("%s Task Priority error\r\n",pcName);
					return;
			}
			TCB->uxPriority = uxPriority;
			if(pcName != NULL)
			{
				strncpy(TCB->pcName,pcName,sizeof(TCB->pcName)-1);
				TCB->pcName[sizeof(TCB->pcName)-1]= '\0';
			}else
			{
				TCB->pcName[0]= '\0';
			}

			

			
			/* -------------------------------------------------------------- */	
			/* Add ReadyList */
			/* -------------------------------------------------------------- */					
			Task_AddList(TCB);
			printf("%s Task Init sucess\r\n",pcName);

		}else
		{
			printf("%s Task Stack error\r\n",pcName);
			free(TCB);
		}
		
	}else
	{
		printf("%s TCB Stack error\r\n",pcName);
	}  
}   

void ListInit(List_t *List)
{

	List->NumberOfItems=0;
	List->xIndex=&List->xListEnd;
	List->xListEnd.pxValue=0xffffffff;
	List->xListEnd.pxOwner=NULL;
	List->xListEnd.pxContainer=List;
	List->xListEnd.pxNext=&List->xListEnd;
	List->xListEnd.pxPrevious=&List->xListEnd;

}

void Task_AddList(MiniTCB_t *TCB)
{
	TCB->xStateListItem.pxOwner=TCB;
	TCB->xStateListItem.pxValue=TCB->uxPriority;
	TCB->xStateListItem.pxContainer=&Task_ReadyList[TCB->uxPriority];
	if(Task_ReadyList[TCB->uxPriority].NumberOfItems==0)
	{
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious=&TCB->xStateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxNext=&TCB->xStateListItem;
		TCB->xStateListItem.pxNext=&Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->xStateListItem.pxPrevious=&Task_ReadyList[TCB->uxPriority].xListEnd;
		
		Task_ReadyList[TCB->uxPriority].xIndex=&TCB->xStateListItem;
	}else
	{
		TCB->xStateListItem.pxNext=&Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->xStateListItem.pxPrevious=Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious;
		TCB->xStateListItem.pxPrevious->pxNext=&TCB->xStateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious=&TCB->xStateListItem;
	}
	Task_ReadyList[TCB->uxPriority].NumberOfItems++;
}

void Task_StartScheduler(void)
{
	printf("Task_Scheduler_Init!\r\n");

	/* -------------------------------------------------------------- */
	/* Systick Init */
	/* -------------------------------------------------------------- */
	HAL_SYSTICK_Config(72000000/1000);


	Get_CurrentTCB();

	StartFirstTask();


}

static __asm void StartFirstTask(void)
{
    PRESERVE8

    /* 从中断向量表中拿到MSP指针初始化值 */
    ldr r0, =0xE000ED08
    ldr r0, [ r0 ]
    ldr r0, [ r0 ]

    /* 重新初始化MSP指针 */
    msr msp, r0
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
	while(1)
	{
	}
}

void Get_CurrentTCB(void)
{
	int i;
	for(i=4;i>=0;i--)
	{
		if(Task_ReadyList[i].NumberOfItems > 0)
		{
			break;	
		}
	}

	CurrentTCB=(MiniTCB_t *)Task_ReadyList[i].xIndex->pxOwner;

	Task_ReadyList[i].xIndex=Task_ReadyList[i].xIndex->pxNext;
	if(Task_ReadyList[i].xIndex == &Task_ReadyList[i].xListEnd)
		Task_ReadyList[i].xIndex=Task_ReadyList[i].xIndex->pxNext;

}

void MiniRTOS_delay(uint32_t delay_time)
{
	HAL_Delay(delay_time);
}
