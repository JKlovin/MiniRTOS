#ifndef __MINIRTOS_H
#define __MINIRTOS_H

#include <stdint.h>


/* 无限等待标识 */
#define MINIRTOS_WAIT_FOREVER UINT32_MAX


typedef void (*TaskFunction_t)(void *arg);
typedef struct List List_t;
typedef struct MiniMutex MiniMutex_t;


/* 任务通知状态机 */
typedef enum
{
  Notify_Idle		= 0x00U,
  Notify_Wait		= 0x01U,
  Notify_Ready	= 0x02U
} Notify_Status;

/* 函数状态返回值 */
typedef enum
{
    MiniRTOS_OK = 0U,
    MiniRTOS_Timeout,
    MiniRTOS_Error,
    MiniRTOS_Invalid
} MiniRTOS_State;


/* 列表项结构体 */
typedef struct List_item
{
    /* 列表项的值 */
    uint32_t pxValue;

    /* 下一个列表项 */
    struct List_item *pxNext;

    /* 上一个列表项 */
    struct List_item *pxPrevious;

    /* 列表项所属的任务体 */
    void *pxOwner;

    /* 列表项所属的列表 */
    List_t *pxContainer;

} ListItem_t;

/* 列表结构体 */
struct List
{
    /* 列表中的元素数量 */
    unsigned long NumberOfItems;

    /* 列表的索引项 */
    ListItem_t *xIndex;

    /* 列表的结束项 */
    ListItem_t xListEnd;
};

/* 任务结构体 TCB */
typedef struct MiniTCB
{
	/* 任务栈栈顶指针 	*/
	/* STM向下生长 		*/
	uint32_t *pxTopOfStack;

	/* 任务栈及地址 		*/
	uint32_t *pxStackBase;

	/* 任务调度优先级 	*/
	uint32_t uxPriority;
	
	/* 任务原始优先级 	*/
	uint32_t uxInitPriority;

	/* 任务名称 				*/
	char pcName[32];

	/* 任务状态列表项 	*/
	ListItem_t StateListItem;
	
	/* 事件状态列表项	*/
	ListItem_t EventListItem;
	
	/* 任务通知值 			*/
	uint32_t 				NotifyValue;
	
	/* 任务通知状态 		*/
	Notify_Status		NotifyState	;
	
	/* 当前任务正在等待的互斥锁 */
	MiniMutex_t *pxWaitingMutex;

	/* 当前任务持有的互斥锁链表 */
	MiniMutex_t *pxHeldMutexList;
	
	/* 当前事件等待的原始进入顺序 */
	uint64_t uxEventWaitOrder;

} MiniTCB_t;

/* 互斥锁 */
typedef struct MiniMutex
{
	/* 互斥锁所有者 */
	MiniTCB_t *Owner;

	/* 互斥锁等待列表 */
	List_t WaitList;
	
	/* 等待者的最高有效优先级 */
	uint32_t uxHighestWaitPriority;
	
	/* 同一任务持有的下一把互斥锁 */
	struct MiniMutex *pxNextHeldMutex;

} MiniMutex_t;

/* 信号量 */
typedef struct MiniSemaphore
{
	/* 当前可用信号量 */
	uint16_t uxCount;

	/* 最大可用信号量 */	
	uint16_t uxMaxCount;

	/* 信号量等待列表 */
	List_t WaitList;
	
}MiniSemaphore_t;

/* 队列 */
typedef struct MiniQueue
{
	/* 当前队列长度 */
	uint16_t uxCount		;
	
	/* 最大队列长度 */
	uint16_t uxMaxCount 	;
	
	/* 每个元素的字节数 */
  uint16_t uxItemSize;
	
	/* 队列头指针 */
	uint8_t *head			;
	
	/* 队列指针 */
	uint8_t *Queue			;
	
	/* 队列尾指针 */
	uint8_t *tail			;

	/* 等待队列数据列表 */
	List_t xWaitingToReceive;

	/* 等待队列槽位列表 */
	List_t xWaitingToSend;
	
}MiniQueue_t;



__asm void StartFirstTask(void);
void SysTick_Handler(void);
void __asm PendSV_Handler(void);
void __asm SVC_Handler(void);


#endif
