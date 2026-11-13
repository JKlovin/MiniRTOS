#ifndef MINIRTOS_QUEUE_H
#define MINIRTOS_QUEUE_H

#include "minirtos.h"


void Queue_Init(MiniQueue_t *MiniQueue, uint16_t Queue_Length);
void Queue_Send(MiniQueue_t *MiniQueue,uint32_t Queue_Value);
uint32_t Queue_Receive(MiniQueue_t *MiniQueue);


#endif
