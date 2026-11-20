#ifndef MINIRTOS_QUEUE_H
#define MINIRTOS_QUEUE_H

#include "minirtos.h"

/*-------------------------------------------------------------------------------*/
/*---------------------------------队列初始化------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 队列初始化 */
uint8_t Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length, uint16_t ItemSize);

/*-------------------------------------------------------------------------------*/
/*---------------------------------队列发送--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 有限超时队列发送 */
MiniRTOS_State Queue_SendTimeout(MiniQueue_t *MiniQueue,const void *Data,uint32_t WaitTicks);
/* 无限阻塞队列发送 */
uint8_t Queue_Send(MiniQueue_t *MiniQueue, const void *Data);
/* 非阻塞队列发送 */
uint8_t Queue_TrySend(MiniQueue_t *MiniQueue,const void *Data);

/*-------------------------------------------------------------------------------*/
/*---------------------------------队列接收--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 有限超时队列接收 */
MiniRTOS_State Queue_ReceiveTimeout(MiniQueue_t *MiniQueue,void *Data,uint32_t WaitTicks);
/* 无限阻塞队列接收 */
uint8_t Queue_Receive(MiniQueue_t *MiniQueue, void *Data);
/* 非阻塞队列接收 */
uint8_t Queue_TryReceive(MiniQueue_t *MiniQueue, void *Data);

/*-------------------------------------------------------------------------------*/
/*-----------------------------------中断--------------------------------------*/
/*-------------------------------------------------------------------------------*/

/* 队列发送 */
MiniRTOS_State Queue_SendFromISR(MiniQueue_t *MiniQueue,const void *Data,uint8_t *pxHigherPriorityTaskWoken);
/* 队列接收 */
MiniRTOS_State Queue_ReceiveFromISR(MiniQueue_t *MiniQueue,void *Data,uint8_t *pxHigherPriorityTaskWoken);





#endif
