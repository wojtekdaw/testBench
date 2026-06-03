/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "http_server.h"
#include "can_handler.h"
#include "lwip/tcpip.h"
#include "ethernetif.h"
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
/* USER CODE BEGIN Variables */
extern struct netif gnetif; /* LwIP network interface – defined in lwip.c */
osSemaphoreId_t RxPktSemaphore;
const osSemaphoreAttr_t RxPktSemaphore_attr = {
  .name = "RxPktSemaphore"
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void ethernet_rx_thread(void *argument);


extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  RxPktSemaphore = osSemaphoreNew(1, 0, &RxPktSemaphore_attr);

  const osThreadAttr_t rxTask_attributes = {
    .name = "ethRxTask",
    .stack_size = 1024,
    .priority = (osPriority_t) osPriorityHigh,
  };
  osThreadNew(ethernet_rx_thread, NULL, &rxTask_attributes);
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* Signal State 1: Pre-LwIP (RED ON) */
  HAL_GPIO_WritePin(LD3_PORT, LD3_PIN, GPIO_PIN_SET);

  /* init code for LWIP */
  MX_LWIP_Init();

  /* Signal State 2: Post-LwIP (BLUE ON, RED OFF) */
  HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LD3_PORT, LD3_PIN, GPIO_PIN_RESET);

  /* USER CODE BEGIN StartDefaultTask */

  /* Brief delay to allow tcpip_thread to finish its own initialization */
  osDelay(100);

  /* Initialize the HTTP REST server task (uses Netconn API) */
  http_server_init();

  /* Signal State 3: Ready (GREEN ON, BLUE OFF) */
  HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_RESET);
  printf("HTTP: REST API Server ready on port 80\r\n");

  uint32_t tick = 0;
  for(;;)
  {
    /* Poll CAN RX */
    CAN_ProcessRX();

    if (++tick >= 50) {
        tick = 0;
        /* Periodic PHY link-state poll (every 500 ms).
         * ethernet_link_check_state() calls netif_set_link_up/down which must
         * run inside tcpip_thread context – guard with LOCK_TCPIP_CORE(). */
        LOCK_TCPIP_CORE();
        ethernet_link_check_state(&gnetif);
        UNLOCK_TCPIP_CORE();

        HAL_GPIO_TogglePin(LD2_PORT, LD2_PIN); /* Blue heartbeat – OS alive */
    }
    osDelay(10);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  osSemaphoreRelease(RxPktSemaphore);
}

void ethernet_rx_thread(void *argument)
{
  extern void ethernetif_input(struct netif *netif);

  for(;;)
  {
    /* Wait for interrupt or timeout */
    osSemaphoreAcquire(RxPktSemaphore, 100);
    
    /* Process all incoming packets */
    ethernetif_input(&gnetif);
  }
}
/* USER CODE END Application */

