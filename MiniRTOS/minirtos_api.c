#include "minirtos_api.h"

List_t Task_ReadyList[5];

void ListInit(List_t *List);
void Task_AddList(MiniTCB_t *TCB);
static void TaskInit_LRerror(void);

/* 系统初始化函数 */
void MiniRTOS_Init(void)
{
	for(int i=0;i<5;i++)
	{
		ListInit(&Task_ReadyList[i]);
	}
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
	}else
	{
		TCB->xStateListItem.pxNext=&Task_ReadyList[TCB->uxPriority].xListEnd;
		TCB->xStateListItem.pxPrevious=Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious;
		TCB->xStateListItem.pxPrevious->pxNext=&TCB->xStateListItem;
		Task_ReadyList[TCB->uxPriority].xListEnd.pxPrevious=&TCB->xStateListItem;
	}
	Task_ReadyList[TCB->uxPriority].NumberOfItems++;
}

void Task_StartSchedulaer(void)
{
	
}


static void TaskInit_LRerror(void)
{
	printf("TaskInit_LR_error! \r\n");
	while(1)
	{
	}
}
