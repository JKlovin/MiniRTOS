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
volatile uint32_t System_Tick = 0;

extern MiniTCB_t *volatile CurrentTCB;
extern volatile uint32_t MiniRTOS_Running;
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
__asm void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */
  extern CurrentTCB;
  extern MiniRTOS_Running;

  /* 8字节对齐 */
  PRESERVE8

  /* 获取当前任务栈顶指针*/
  ldr r3, = CurrentTCB
  ldr r1, [ r3 ]
  ldr r0, [ r1 ]

  /* 软件出栈 */
  ldmia r0 !, { r4 - r11 }

  /* 软件出栈后，剩下的任务栈需要硬件出栈，设置PSP为后续用户指针*/
  msr psp, r0
  /* 指令同步*/
  isb

  /* 设置调度器标志位，启动 */
  ldr r2, = MiniRTOS_Running
  mov r0, #1
  str r0, [r2]

  /* 开启中断 */
  mov r0, # 0
  msr basepri, r0

  /* 设置LR寄存器，配置PSP为后续使用指针 */
  orr r14, # 0xd
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
__asm void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
  extern CurrentTCB;
  extern Get_CurrentTCB;

  PRESERVE8

  /* 关闭中断 */
  cpsid i

  /* 获取当前任务栈顶指针 */
  mrs r0, psp
  /* 指令同步*/
  isb

  /* 获取下一次任务栈顶指针 */
  ldr r3, =CurrentTCB 
  ldr r2, [ r3 ]

  /* 对当前任务堆栈软件入栈 */
  stmdb r0 !, { r4 - r11 }
  
  /* 保存当前任务的栈顶指针到 r2 当前任务的 TCB */
  str r0, [ r2 ]

  /* 保存r3，r14寄存器，避免Get_CurrentTCB对其进行修改，
      特别是因为后续的Get_CurrentTCB会对R14进行覆盖 */
  stmdb sp !, { r3, r14 }

  /* 获取再下一次任务块*/
  bl Get_CurrentTCB

  /* 出栈r3，r14寄存器 */
  ldmia sp !, { r3, r14 }

  ldr r1, [ r3 ]
  ldr r0, [ r1 ]
  ldmia r0 !, { r4 - r11 }

  /* 更新PSP */
  msr psp, r0
  isb
  
  /* 恢复中断 */
  cpsie i

  bx r14
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
  if (MiniRTOS_Running == 1)
  {
    /* 挂起 PendSV 异常 */
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
