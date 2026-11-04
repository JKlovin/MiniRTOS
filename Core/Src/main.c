/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "minirtos_task.h"
#include "minirtos_api.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern volatile uint32_t System_Tick;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LED1_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_1_task\r\n");
  }
}
void LED2_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_2_task\r\n");
  }
}
void LED3_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_3_task\r\n");
  }
}
void LED4_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_4_task\r\n");
  }
}
void LED5_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_5_task\r\n");
  }
}
void LED6_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_6_task\r\n");
  }
}
void LED7_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_7_task\r\n");
  }
}
void LED8_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_8_task\r\n");
  }
}
void LED9_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_9_task\r\n");
  }
}
void LED10_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_10_task\r\n");
  }
}
void LED11_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_11_task\r\n");
  }
}
void LED12_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_12_task\r\n");
  }
}
void LED13_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_13_task\r\n");
  }
}
void LED14_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_14_task\r\n");
  }
}
void LED15_Handle(void *arg)
{
  while (1)
  {
    MiniRTOS_delay(5000);
    printf("in Led_15_task\r\n");
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  printf("------------------------------------\r\n");
  printf("------------------------------------\r\n");
  printf("MiniRTOS Init OK!\r\n");
  MiniRTOS_Init();

  MiniCreateTask(LED1_Handle, "Task_1", 128, 0);
  MiniCreateTask(LED2_Handle, "Task_2", 128, 0);
  MiniCreateTask(LED3_Handle, "Task_3", 128, 0);
  // MiniCreateTask(LED4_Handle, "Task_4", 128, 1);
  // MiniCreateTask(LED5_Handle, "Task_5", 128, 1);
  // MiniCreateTask(LED6_Handle, "Task_6", 128, 1);
  MiniCreateTask(LED7_Handle, "Task_7", 128, 2);
  MiniCreateTask(LED8_Handle, "Task_8", 128, 2);
  MiniCreateTask(LED9_Handle, "Task_9", 128, 2);
  // MiniCreateTask(LED10_Handle, "Task_10", 128, 3);
  // MiniCreateTask(LED11_Handle, "Task_11", 128, 3);
  // MiniCreateTask(LED12_Handle, "Task_12", 128, 3);
  MiniCreateTask(LED13_Handle, "Task_13", 128, 4);
  MiniCreateTask(LED14_Handle, "Task_14", 128, 4);
  MiniCreateTask(LED15_Handle, "Task_15", 128, 4);


  Task_StartScheduler();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
