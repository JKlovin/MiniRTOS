#ifndef MINIRTOS_QUEUE_H
#define MINIRTOS_QUEUE_H

#include "minirtos.h"

/* 队列初始化 */
uint8_t Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length, uint16_t ItemSize);

/* 有限超时队列发送 */
MiniRTOS_State Queue_SendTimeout(MiniQueue_t *MiniQueue,const void *Data,uint32_t WaitTicks);
/* 无限阻塞队列发送 */
uint8_t Queue_Send(MiniQueue_t *MiniQueue, const void *Data);
/* 非阻塞队列发送 */
uint8_t Queue_TrySend(MiniQueue_t *MiniQueue,const void *Data);

/* 有限超时队列接收 */
MiniRTOS_State Queue_ReceiveTimeout(MiniQueue_t *MiniQueue,void *Data,uint32_t WaitTicks);
/* 无限阻塞队列接收 */
uint8_t Queue_Receive(MiniQueue_t *MiniQueue, void *Data);
/* 非阻塞队列接收 */
uint8_t Queue_TryReceive(MiniQueue_t *MiniQueue, void *Data);







#endif
