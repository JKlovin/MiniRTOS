#ifndef __MINIRTOS_TASK_H
#define __MINIRTOS_TASK_H

#include <stdint.h>

typedef void (*TaskFunction_t)(void *arg);

typedef struct List List_t;

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
    /* 任务栈栈顶指针 */
    /* STM向下生长 */
    uint32_t *pxTopOfStack;

    /* 任务栈及地址 */
    uint32_t *pxStackBase;

    /* 任务优先级 */
    uint32_t uxPriority;

    /* 任务名称 */
    char pcName[32]; 

    /* 任务状态列表项 */
    ListItem_t xStateListItem;


} MiniTCB_t;

#endif
