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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "queue.h"
#include "tim.h"
#include "app_crc16.h"
#include "app_protocol_types.h"
#include "app_protocol_codec.h"
#include "app_cmd_handle.h"

#include "pressure_sensor.h"
#include "safety_edge.h"
#include "proximity_switch.h"
#include "usart.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern DMA_HandleTypeDef hdma_usart3_rx;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_PROTO_CRC16_SIZE        2U
#define APP_PROTO_FRAME_TAIL_SIZE   (APP_PROTO_CRC16_SIZE + 1U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static QueueHandle_t cmdQueue;
static QueueHandle_t replyQueue;

static TaskHandle_t uartRxTaskNotifyHandle;

/* USER CODE END Variables */
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for uartTxTask */
osThreadId_t uartTxTaskHandle;
const osThreadAttr_t uartTxTask_attributes = {
  .name = "uartTxTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for cmdHandleTask */
osThreadId_t cmdHandleTaskHandle;
const osThreadAttr_t cmdHandleTask_attributes = {
  .name = "cmdHandleTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for statusTask */
osThreadId_t statusTaskHandle;
const osThreadAttr_t statusTask_attributes = {
  .name = "statusTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
bool app_msg_add_crc16_and_lf(uint8_t *buf, uint32_t bufSize, uint32_t* finnalLen);

/* USER CODE END FunctionPrototypes */

void appUartRxTask(void *argument);
void appUartTxTask(void *argument);
void appCmdHandleTask(void *argument);
void appStatusTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  cmdQueue = xQueueCreate(8U, sizeof(app_cmd_msg_t));
  replyQueue = xQueueCreate(8U, sizeof(app_reply_msg_t));

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(appUartRxTask, NULL, &uartRxTask_attributes);

  /* creation of uartTxTask */
  uartTxTaskHandle = osThreadNew(appUartTxTask, NULL, &uartTxTask_attributes);

  /* creation of cmdHandleTask */
  cmdHandleTaskHandle = osThreadNew(appCmdHandleTask, NULL, &cmdHandleTask_attributes);

  /* creation of statusTask */
  statusTaskHandle = osThreadNew(appStatusTask, NULL, &statusTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_appUartRxTask */
/**
  * @brief  Function implementing the uartRxTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_appUartRxTask */
void appUartRxTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN appUartRxTask */
  app_cmd_msg_t cmdMsg;
  uint8_t frameBuf[1024U];
  uint8_t uartRecvBuf[1024U];
  uint32_t notifyValue = 0U;

  uartRxTaskNotifyHandle = xTaskGetCurrentTaskHandle();
  
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uartRecvBuf, sizeof(uartRecvBuf) - 1U);
  __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);  // 禁止半传输中断，避免干扰IDLE中断处理
  /* Infinite loop */
  for(;;)
  {
    if (xTaskNotifyWait(0U, UINT32_MAX, &notifyValue, portMAX_DELAY) == pdTRUE)
    {
      // 1. 从DMA接收缓冲区复制数据到帧缓冲区，并添加字符串结束符
      memcpy(frameBuf, uartRecvBuf, notifyValue);
      frameBuf[notifyValue] = '\0';

      LOG_DEUBG("appUartRxTask -> recv: %s", frameBuf);

      // 2. 重新启动DMA接收
      HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uartRecvBuf, sizeof(uartRecvBuf) - 1U);

      // 3. 处理接收到的数据
      if (notifyValue > 0U)
      {
        // 1. 帧格式初步校验：至少要有CRC16和帧尾\n
        if ((notifyValue <= APP_PROTO_FRAME_TAIL_SIZE) || (frameBuf[notifyValue - 1U] != (uint8_t)'\n'))
        {
          LOG_DEUBG("appUartRxTask -> Frame format error");
          continue;
        }
        LOG_DEUBG("appUartRxTask -> Frame format OK");
        // 2. CRC16校验
        uint32_t jsonLen = notifyValue - APP_PROTO_FRAME_TAIL_SIZE;
        uint16_t recvCrc = (uint16_t)frameBuf[jsonLen]              // 获取接收到的CRC16值（小端排序）
                         | ((uint16_t)frameBuf[jsonLen + 1U] << 8U);
        uint16_t calcCrc = app_crc16_compute(frameBuf, (size_t)jsonLen);
        if (recvCrc != calcCrc)
        {
          LOG_DEUBG("appUartRxTask -> CRC error");
          continue;
        }
        LOG_DEUBG("appUartRxTask -> CRC OK");
        // 3. 解析并发送到命令处理队列
        memset(&cmdMsg, 0, sizeof(cmdMsg));
        if(app_protocol_decode_cmd_msg((const char *)frameBuf, &cmdMsg) != 0)
        {
          LOG_DEUBG("appUartRxTask -> Decode error");
          continue;
        }
        LOG_DEUBG("appUartRxTask -> Decode OK");

        if (xQueueSend(cmdQueue, &cmdMsg, 0U) != pdPASS)
        {
          LOG_DEUBG("appUartRxTask -> Cmd queue full");
        }
          LOG_DEUBG("appUartRxTask -> Cmd sent to queue");
      }
    }
  }

  osDelay(pdMS_TO_TICKS(10));
  /* USER CODE END appUartRxTask */
}

/* USER CODE BEGIN Header_appUartTxTask */
/**
* @brief Function implementing the uartTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_appUartTxTask */
void appUartTxTask(void *argument)
{
  /* USER CODE BEGIN appUartTxTask */
  app_reply_msg_t replyMsg;

  uint8_t uartSendBuf[1024U];
  uint32_t finnalDataLen = 0;

  /* Infinite loop */
  for(;;)
  {
    if (xQueueReceive(replyQueue, &replyMsg, portMAX_DELAY) == pdTRUE)
    {
      LOG_DEUBG("appUartTxTask -> Received reply from queue");
      if (app_protocol_encode_reply_msg(&replyMsg, (char *)uartSendBuf, sizeof(uartSendBuf)) == 0)
      {
        LOG_DEUBG("appUartTxTask -> Reply encode OK");

        if(!app_msg_add_crc16_and_lf(uartSendBuf, sizeof(uartSendBuf), &finnalDataLen))
        {
          LOG_DEUBG("appUartTxTask -> Failed to add CRC16 and LF");
          continue;
        }
        HAL_UART_Transmit(&huart3, (uint8_t *)uartSendBuf, finnalDataLen, HAL_MAX_DELAY);
        LOG_DEUBG("appUartTxTask -> Sent: %s", uartSendBuf);
      }
      else
      {
        LOG_DEUBG("appUartTxTask -> Reply encode error");
      }

      app_protocol_free_reply_msg(&replyMsg);
    }
  }

    osDelay(pdMS_TO_TICKS(10));
  /* USER CODE END appUartTxTask */
}

/* USER CODE BEGIN Header_appCmdHandleTask */
/**
* @brief Function implementing the cmdHandleTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_appCmdHandleTask */
void appCmdHandleTask(void *argument)
{
  /* USER CODE BEGIN appCmdHandleTask */
  app_cmd_msg_t cmdMsg;
  app_reply_msg_t replyMsg;

  /* Infinite loop */
  for(;;)
  {
    if (xQueueReceive(cmdQueue, &cmdMsg, portMAX_DELAY) == pdTRUE)
    {
      switch (cmdMsg.msg_type_id) {
        case APP_MSG_TYPE_ID_CMD_MOVE_LR:
          app_move_lr_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled MOVE_LR command");
          break;
        case APP_MSG_TYPE_ID_CMD_MOVE_UD:
          app_move_ud_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled MOVE_UD command");
          break;
        case APP_MSG_TYPE_ID_CMD_MOVE_XY:
          app_move_xy_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled MOVE_XY command");
          break;
        case APP_MSG_TYPE_ID_CMD_TRACK_SWITCH:
          app_track_switch_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled TRACK_SWITCH command");
          break;
        case APP_MSG_TYPE_ID_CMD_BALL_LOCK:
          app_ball_lock_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled BALL_LOCK command");
          break;
        case APP_MSG_TYPE_ID_CMD_BRUSH_CONTROL:
          app_brush_control_handle(&cmdMsg, &replyMsg);
          LOG_DEUBG("appCmdHandleTask -> Handled BRUSH_CONTROL command");
          break;
        default:
          // create_reply_msg(&cmdMsg, &replyMsg, cmdMsg.msg_type_id | 0x10U, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Unknown command");
          break;
      }

      if (xQueueSend(replyQueue, &replyMsg, 0U) != pdPASS)
      {
        LOG_DEUBG("appCmdHandleTask -> Reply queue full");
      }
      LOG_DEUBG("appCmdHandleTask -> Reply sent to queue");

      app_protocol_free_cmd_msg(&cmdMsg);
      LOG_DEUBG("appCmdHandleTask -> Freed command message");
    }
    osDelay(pdMS_TO_TICKS(10));
  }
  /* USER CODE END appCmdHandleTask */
}

/* USER CODE BEGIN Header_appStatusTask */
/**
* @brief Function implementing the statusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_appStatusTask */
void appStatusTask(void *argument)
{
  /* USER CODE BEGIN appStatusTask */
  app_reply_msg_t pressureMsg;
  app_reply_msg_t safetyEdgeMsg;
  app_reply_msg_t deviceStatusMsg;

  PressureSensor_Init();
  /* Infinite loop */
  for(;;)
  {
    // 1. 压力传感器
    if(app_query_pressure_handle(&pressureMsg))
    {
      if (xQueueSend(replyQueue, &pressureMsg, 0U) != pdPASS)
      {
        LOG_DEUBG("appStatusTask -> Reply queue full");
      }
      LOG_DEUBG("appStatusTask -> Pressure status sent to queue");
    }
    else
    {
      LOG_DEUBG("appStatusTask -> Failed to create pressure reply message");
    }

    // 2. 安全触边  --  事件触发
    bool edgeDetect = false;
    if(app_query_safety_edge_handle(&safetyEdgeMsg, edgeDetect))
    {
      if (edgeDetect)
      {
        if(xQueueSend(replyQueue, &safetyEdgeMsg, 0U) != pdPASS)
        {
          LOG_DEUBG("appStatusTask -> Reply queue full");
        }
      }
      LOG_DEUBG("appStatusTask -> Safety edge status sent to queue");
    }
    else
    {
      LOG_DEUBG("appStatusTask -> Failed to create safety edge reply message");
    }

    // 3. 设备上报状态
    if(app_query_device_status_handle(&deviceStatusMsg))
    {
      if (xQueueSend(replyQueue, &deviceStatusMsg, 0U) != pdPASS)
      {
        LOG_DEUBG("appStatusTask -> Reply queue full");
      }
      LOG_DEUBG("appStatusTask -> Device status sent to queue");
    }
    else
    {
      LOG_DEUBG("appStatusTask -> Failed to create device status reply message");
    }

    
    osDelay(pdMS_TO_TICKS(10));
  }
  /* USER CODE END appStatusTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART3)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (uartRxTaskNotifyHandle != NULL)
    {
      (void)xTaskNotifyFromISR(uartRxTaskNotifyHandle,
                               (uint32_t)Size,
                               eSetValueWithOverwrite,
                               &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

bool app_msg_add_crc16_and_lf(uint8_t *buf, uint32_t bufSize, uint32_t* finnalLen)
{
  uint32_t msgLen = strlen((char *)buf);

  if (!buf || bufSize < APP_PROTO_CRC16_SIZE || msgLen + APP_PROTO_FRAME_TAIL_SIZE > bufSize)
    return false;

  uint16_t crc = app_crc16_compute((const uint8_t *)buf, msgLen);
  buf[msgLen] = (uint8_t)(crc & 0xFFU);
  buf[msgLen + 1U] = (uint8_t)((crc >> 8U) & 0xFFU);
  buf[msgLen + 2U] = '\n';

  if (finnalLen != NULL)
  {
    *finnalLen = (uint32_t)(msgLen + APP_PROTO_FRAME_TAIL_SIZE);
  }
  return true;
}

/* USER CODE END Application */

