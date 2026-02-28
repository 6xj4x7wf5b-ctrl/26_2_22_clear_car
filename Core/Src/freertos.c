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
#include <stdio.h>
#include <string.h>

#include "queue.h"
#include "semphr.h"
#include "tim.h"
#include "usbd_cdc_if.h"
#include "app_crc16.h"
#include "app_protocol_types.h"
#include "app_protocol_codec.h"
#include "app_cmd_handle.h"

#include "pressure_sensor.h"
#include "safety_edge.h"
#include "proximity_switch.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_CDC_FRAME_BUF_SIZE      256U
#define APP_SENSOR_PERIOD_MS        1000U
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
static QueueHandle_t errorQueue;
static SemaphoreHandle_t cdcFrameSem;

/* USER CODE END Variables */
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for uartTxTask */
osThreadId_t uartTxTaskHandle;
const osThreadAttr_t uartTxTask_attributes = {
  .name = "uartTxTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for cmdHandleTask */
osThreadId_t cmdHandleTaskHandle;
const osThreadAttr_t cmdHandleTask_attributes = {
  .name = "cmdHandleTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for errorTask */
osThreadId_t errorTaskHandle;
const osThreadAttr_t errorTask_attributes = {
  .name = "errorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void app_cdc_send_string(const char *text);
static void app_push_error(const char *text);
void app_on_cdc_frame_timeout_isr(void);

/* USER CODE END FunctionPrototypes */

void appUartRxTask(void *argument);
void appUartTxTask(void *argument);
void appCmdHandleTask(void *argument);
void appSensorTask(void *argument);
void appErrorTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

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
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  cmdQueue = xQueueCreate(8U, sizeof(app_cmd_msg_t));
  replyQueue = xQueueCreate(8U, sizeof(app_reply_msg_t));
  errorQueue = xQueueCreate(4U, sizeof(app_error_msg_t));
  cdcFrameSem = xSemaphoreCreateBinary();
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(appUartRxTask, NULL, &uartRxTask_attributes);

  /* creation of uartTxTask */
  uartTxTaskHandle = osThreadNew(appUartTxTask, NULL, &uartTxTask_attributes);

  /* creation of cmdHandleTask */
  cmdHandleTaskHandle = osThreadNew(appCmdHandleTask, NULL, &cmdHandleTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(appSensorTask, NULL, &sensorTask_attributes);

  /* creation of errorTask */
  errorTaskHandle = osThreadNew(appErrorTask, NULL, &errorTask_attributes);

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
  uint8_t frameBuf[APP_CDC_FRAME_BUF_SIZE];
  app_cmd_msg_t cmdMsg;

  /* Infinite loop */
  for(;;)
  {
    if (xSemaphoreTake(cdcFrameSem, portMAX_DELAY) == pdTRUE)
    {
      uint32_t readLen = CDC_Read_FS(frameBuf, APP_CDC_FRAME_BUF_SIZE - 1U);
      if (readLen > 0U)
      {
        // 1. 帧格式初步校验：至少要有CRC16和帧尾\n
        if ((readLen <= APP_PROTO_FRAME_TAIL_SIZE) || (frameBuf[readLen - 1U] != (uint8_t)'\n'))
        {
          app_push_error("frame format error\n");
          continue;
        }
        // 2. CRC16校验
        uint32_t jsonLen = readLen - APP_PROTO_FRAME_TAIL_SIZE;
        uint16_t recvCrc = (uint16_t)frameBuf[jsonLen]              // 获取接收到的CRC16值（小端排序）
                         | ((uint16_t)frameBuf[jsonLen + 1U] << 8U);
        uint16_t calcCrc = app_crc16_compute(frameBuf, (size_t)jsonLen);
        if (recvCrc != calcCrc)
        {
          app_push_error("crc16 verify failed\n");
          continue;
        }
        // 3. 解析并发送到命令处理队列
        memset(&cmdMsg, 0, sizeof(cmdMsg));
        if(app_protocol_decode_cmd_msg((const char *)frameBuf, &cmdMsg) != 0)
        {
          app_push_error("cmd parse error\n");
          continue;
        }

        if (xQueueSend(cmdQueue, &cmdMsg, 0U) != pdPASS)
        {
          app_push_error("cmd queue full\n");
        }
      }

      if (CDC_RxDroppedFlag_FS() != 0U)
      {
        CDC_ClearRxDroppedFlag_FS();
        app_push_error("cdc rx overflow\n");
      }
    }
  }
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

  /* Infinite loop */
  for(;;)
  {
    char jsonBuf[2048U];
    if (xQueueReceive(replyQueue, &replyMsg, portMAX_DELAY) == pdTRUE)
    {
      if (app_protocol_encode_cmd_msg(&replyMsg, jsonBuf, sizeof(jsonBuf)) == 0)
      {
        app_cdc_send_string(jsonBuf);
      }
      else
      {
        app_push_error("reply encode error\n");
      }

    }
  }
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
          break;
        case APP_MSG_TYPE_ID_CMD_MOVE_UD:
          app_move_ud_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_MOVE_XY:
          app_move_xy_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_TRACK_SWITCH:
          app_track_switch_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_BALL_LOCK:
          app_ball_lock_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_BRUSH_CONTROL:
          app_brush_control_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_QUERY_STATUS:
          app_query_status_handle(&cmdMsg, &replyMsg);
          break;
        case APP_MSG_TYPE_ID_CMD_QUERY_PRESSURE:
          app_query_pressure_handle(&cmdMsg, &replyMsg);
          break;
        default:
          // create_reply_msg(&cmdMsg, &replyMsg, cmdMsg.msg_type_id | 0x10U, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Unknown command");
          break;
      }

        if (xQueueSend(replyQueue, &replyMsg, 0U) != pdPASS)
        {
          app_push_error(" reply queue full\n");
        }
    }
  }
  /* USER CODE END appCmdHandleTask */
}

/* USER CODE BEGIN Header_appSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_appSensorTask */
void appSensorTask(void *argument)
{
  /* USER CODE BEGIN appSensorTask */
  

  /* Infinite loop */
  for(;;)
  {
    
  }
  /* USER CODE END appSensorTask */
}

/* USER CODE BEGIN Header_appErrorTask */
/**
* @brief Function implementing the errorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_appErrorTask */
void appErrorTask(void *argument)
{
  /* USER CODE BEGIN appErrorTask */
  app_error_msg_t errMsg;

  /* Infinite loop */
  for(;;)
  {
    if (xQueueReceive(errorQueue, &errMsg, portMAX_DELAY) == pdTRUE)
    {
      // app_cdc_send_string(errMsg.text);
    }
  }
  /* USER CODE END appErrorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

static void app_cdc_send_string(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  uint16_t len = (uint16_t)strlen(text);
  if (len == 0U)
  {
    return;
  }

  while (CDC_Transmit_FS((uint8_t *)text, len) == USBD_BUSY)
  {
    osDelay(1);
  }
}

static void app_push_error(const char *text)
{
  app_error_msg_t errMsg;

  if (text == NULL)
  {
    return;
  }

  memset(&errMsg, 0, sizeof(errMsg));
  // strncpy(errMsg.text, text, sizeof(errMsg.text) - 1U);
  (void)xQueueSend(errorQueue, &errMsg, 0U);
}

void app_on_cdc_frame_timeout_isr(void)
{
  BaseType_t higherPrioTaskWoken = pdFALSE;

  if (cdcFrameSem != NULL)
  {
    (void)xSemaphoreGiveFromISR(cdcFrameSem, &higherPrioTaskWoken);
  }

  (void)HAL_TIM_Base_Stop_IT(&htim5);
  __HAL_TIM_SET_COUNTER(&htim5, 0U);

  portYIELD_FROM_ISR(higherPrioTaskWoken);
}

/* USER CODE END Application */

