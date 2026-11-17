#ifndef MINIRTOS_QUEUE_H
#define MINIRTOS_QUEUE_H

#include "minirtos.h"


uint8_t Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length, uint16_t ItemSize);
uint8_t Queue_Send(MiniQueue_t *MiniQueue, const void *Data);
uint8_t Queue_TrySend(MiniQueue_t *MiniQueue,const void *Data);
uint8_t Queue_Receive(MiniQueue_t *MiniQueue, void *Data);


#endif
