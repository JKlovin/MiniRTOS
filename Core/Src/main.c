
/* 中断上下文恢复挂起任务 */
MiniRTOS_State Task_ResumeFromISR(
    MiniTCB_t *TCB,
    uint8_t *pxHigherPriorityTaskWoken)
{
    uint32_t SavedMask;

    /* 1. 参数检查 */
    if ((TCB == NULL) ||
        (pxHigherPriorityTaskWoken == NULL))
    {
        return MiniRTOS_Invalid;
    }

    /* 2. 检查内核运行状态与中断上下文 */
    if ((MiniRTOS_Running == 0U) ||
        (CurrentTCB == NULL) ||
        (__get_IPSR() < 16U))
    {
        return MiniRTOS_Invalid;
    }

    /* 3. 进入 ISR 临界区 */
    SavedMask = Mini_EnterCriticalFromISR();

    /* 4. 检查目标任务是否处于挂起链表 */
    if (TCB->StateListItem.pxContainer != &Task_SuspendList)
    {
        Mini_ExitCriticalFromISR(SavedMask);
        return MiniRTOS_Error;
    }

    /* 5. 检查任务状态和优先级是否合法 */
    if ((TCB->uxPriority >= UXPRIORITY_MAX) ||
        (TCB->EventListItem.pxContainer != NULL) ||
        (TCB->pxWaitingMutex != NULL))
    {
        Mini_ExitCriticalFromISR(SavedMask);
        return MiniRTOS_Error;
    }

    /* 6. 从任务挂起链表中移除 */
    List_RemoveNode(&TCB->StateListItem);

    /* 7. 重新加入任务就绪链表 */
    Task_AddReadyList(TCB);

    /* 8. 判断是否需要调度更高优先级任务 */
    if (TCB->uxPriority > CurrentTCB->uxPriority)
    {
        *pxHigherPriorityTaskWoken = 1U;
    }

    /* 9. 恢复进入前的中断屏蔽状态 */
    Mini_ExitCriticalFromISR(SavedMask);

    return MiniRTOS_OK;
}
