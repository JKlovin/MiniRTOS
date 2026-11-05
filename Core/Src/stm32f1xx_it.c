/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32f1xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "minirtos_api.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern volatile uint32_t		System_Tick				;
extern MiniTCB_t *volatile	CurrentTCB				;
extern volatile uint32_t		MiniRTOS_Running	;
extern List_t								*CurrentWaitList 	;
extern List_t								*OverflowWaitList	;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void __asm SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */
  extern CurrentTCB;
  extern MiniRTOS_Running;

	/* 保证8字节栈对齐 */
	PRESERVE8

	/* -------------------------------------------- */
	/* 获取第一个任务的栈顶指针 */
	/* -------------------------------------------- */

	/* 获取 CurrentTCB 全局指针变量的地址 */
	ldr r3, =CurrentTCB

	/* 读取 CurrentTCB，获取当前任务 TCB 地址 */
	ldr r1, [r3]

	/* 读取 TCB 第一个成员 pxTopOfStack */
	/* r0 保存当前任务的栈顶指针 */
	ldr r0, [r1]

	/* -------------------------------------------- */
	/* 恢复第一个任务的软件保存寄存器 */
	/* -------------------------------------------- */

	/* 从任务栈中恢复 R4-R11 */
	/* 每恢复一个寄存器，r0 指针自动递增 */
	ldmia r0!, {r4-r11}

	/* 设置 PSP，指向硬件异常栈帧的起始位置 */
	/* 后续异常返回时由硬件自动恢复 R0-R3、R12、LR、PC、xPSR */
	msr psp, r0

	/* 指令同步，确保 PSP 更新生效 */
	isb

	/* -------------------------------------------- */
	/* 设置调度器运行标志 */
	/* -------------------------------------------- */

	/* 获取 MiniRTOS_Running 变量地址 */
	ldr r2, =MiniRTOS_Running

	/* 设置运行标志为1 */
	mov r0, #1
	str r0, [r2]

	/* -------------------------------------------- */
	/* 允许被 BASEPRI 屏蔽的中断 */
	/* -------------------------------------------- */

	/* BASEPRI = 0，取消 BASEPRI 中断屏蔽 */
	mov r0, #0
	msr basepri, r0

	/* -------------------------------------------- */
	/* 异常返回，正式启动第一个任务 */
	/* -------------------------------------------- */

	/* 设置 EXC_RETURN = 0xFFFFFFFD */
	/* 返回线程模式，使用 PSP，恢复任务上下文 */
	orr r14, #0xd

	/* 异常返回，硬件自动恢复剩余寄存器 */
	/* PC 恢复为任务入口函数地址，开始执行任务 */
	bx r14

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void __asm PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
  extern CurrentTCB;
  extern Get_CurrentTCB;

	/* 保证8字节栈对齐 */
	PRESERVE8

	/* -------------------------------------------- */
	/* 进入任务上下文切换，关闭可屏蔽中断 */
	/* -------------------------------------------- */

	/* PRIMASK = 1，禁止可屏蔽中断 */
	/* 避免切换过程中被普通中断打断 */
	cpsid i

	/* -------------------------------------------- */
	/* 保存当前任务上下文 */
	/* -------------------------------------------- */

	/* 读取 PSP，获取当前任务的硬件异常压栈后栈顶 */
	/* 进入 PendSV 前硬件已自动保存 R0-R3、R12、LR、PC、xPSR */
	mrs r0, psp

	/* 指令同步 */
	isb

	/* 获取 CurrentTCB 全局指针变量的地址 */
	ldr r3, =CurrentTCB

	/* 获取当前运行任务的 TCB 地址 */
	ldr r2, [r3]

	/* 将 R4-R11 压入当前任务栈 */
	/* STM32 栈向低地址增长，r0 自动递减 */
	stmdb r0!, {r4-r11}

	/* 将软件压栈后的栈顶地址保存到当前 TCB */
	/* TCB 的第一个成员为 pxTopOfStack */
	str r0, [r2]

	/* -------------------------------------------- */
	/* 保存异常处理函数需要使用的寄存器 */
	/* -------------------------------------------- */

	/* 使用 MSP 保存 r3 和 LR(EXC_RETURN) */
	/* 防止调用 C 函数时寄存器被覆盖 */
	stmdb sp!, {r3,r14}

	/* -------------------------------------------- */
	/* 执行任务调度，选择下一个任务 */
	/* -------------------------------------------- */

	/* 调用 C 语言调度函数 */
	/* 检查阻塞任务是否到期，并选择最高优先级就绪任务 */
	/* 调度结果通过更新 CurrentTCB 返回 */
	bl Get_CurrentTCB

	/* 恢复之前保存的 r3 和 LR */
	ldmia sp!, {r3,r14}

	/* -------------------------------------------- */
	/* 恢复下一个任务上下文 */
	/* -------------------------------------------- */

	/* 通过 CurrentTCB 获取下一任务的 TCB 地址 */
	ldr r1, [r3]

	/* 读取下一任务保存的栈顶指针 pxTopOfStack */
	ldr r0, [r1]

	/* 从下一任务栈恢复 R4-R11 */
	/* r0 自动递增，指向剩余硬件异常栈帧 */
	ldmia r0!, {r4-r11}

	/* 将 PSP 更新为下一任务的栈顶指针 */
	msr psp, r0

	/* 指令同步，确保 PSP 更新生效 */
	isb

	/* -------------------------------------------- */
	/* 完成任务切换，恢复中断 */
	/* -------------------------------------------- */

	/* PRIMASK = 0，允许可屏蔽中断 */
	cpsie i

	/* 异常返回 */
	/* 硬件从下一任务的 PSP 栈中恢复 */
	/* R0-R3、R12、LR、PC、xPSR */
	/* CPU 开始执行下一任务 */
	bx r14

	/* 占位指令，正常情况下不会执行 */
	nop

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

	System_Tick++;

	/* 检测系统 Tick 溢出,交换链表指针 */
	if(System_Tick == 0)
	{
		List_t *Temp = CurrentWaitList;
		CurrentWaitList = OverflowWaitList;
		OverflowWaitList = Temp;
	}

	if(MiniRTOS_Running == 1)
	{
		/* 设置 PendSV 挂起标志位,请求任务上下文切换 */
		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
	}
	

  /* USER CODE END SysTick_IRQn 0 */

  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM4 global interrupt.
  */
void TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM4_IRQn 0 */

  /* USER CODE END TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM4_IRQn 1 */

  /* USER CODE END TIM4_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
