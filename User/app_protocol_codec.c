#include "app_protocol_codec.h"

#include "FreeRTOS.h"
#include "task.h"
#include "usb_device.h"
#include "usbd_cdc.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static volatile uint8_t s_app_log_busy = 0U;
static char s_app_log_buf[512];

void app_log_debug(const char *fmt, ...)
{
    va_list args;
    int log_len = 0;
    uint16_t tx_len = 0U;
    USBD_CDC_HandleTypeDef *hcdc = NULL;

    if ((fmt == NULL) || (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED))
    {
        return;
    }

    for (;;)
    {
        taskENTER_CRITICAL();
        if (s_app_log_busy == 0U)
        {
            s_app_log_busy = 1U;
            taskEXIT_CRITICAL();
            break;
        }
        taskEXIT_CRITICAL();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    va_start(args, fmt);
    log_len = vsnprintf(s_app_log_buf, sizeof(s_app_log_buf) - 3U, fmt, args);
    va_end(args);

    if (log_len <= 0)
    {
        goto cleanup;
    }

    if ((size_t)log_len > (sizeof(s_app_log_buf) - 3U))
    {
        log_len = (int)(sizeof(s_app_log_buf) - 3U);
    }

    s_app_log_buf[log_len++] = '\r';
    s_app_log_buf[log_len++] = '\n';
    s_app_log_buf[log_len] = '\0';

    if ((hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) || (hUsbDeviceFS.pClassData == NULL))
    {
        goto cleanup;
    }

    tx_len = (uint16_t)((log_len < (int)sizeof(s_app_log_buf)) ? log_len : ((int)sizeof(s_app_log_buf) - 1));
    while (CDC_Transmit_FS((uint8_t *)s_app_log_buf, tx_len) == USBD_BUSY)
    {
        if ((hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) || (hUsbDeviceFS.pClassData == NULL))
        {
            goto cleanup;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    while ((hcdc != NULL) && (hcdc->TxState != 0U))
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

cleanup:
    taskENTER_CRITICAL();
    s_app_log_busy = 0U;
    taskEXIT_CRITICAL();
}

int app_protocol_decode_cmd_msg(const char *json_str, app_cmd_msg_t *out_msg)
{
    if (!json_str || !out_msg)
        return -1;

    cJSON *root = cJSON_Parse(json_str);    // 解析之后，原本的字符串可以删除
    if (!root)
        return -1;

    // 初始化结构体
    memset(out_msg, 0, sizeof(app_cmd_msg_t));

    // msg_type_id
    cJSON *item = cJSON_GetObjectItem(root, "msg_type_id");
    if (!cJSON_IsNumber(item))
        goto error;
    out_msg->msg_type_id = (uint32_t)item->valuedouble;

    // msg_seq
    item = cJSON_GetObjectItem(root, "msg_seq");
    if (!cJSON_IsNumber(item))
        goto error;
    out_msg->msg_seq = (uint32_t)item->valuedouble;

    // msg_type
    item = cJSON_GetObjectItem(root, "msg_type");
    if (!cJSON_IsString(item))
        goto error;
    strncpy(out_msg->msg_type, item->valuestring, APP_MSG_TYPE_STR_LEN - 1);

    // msg_name
    item = cJSON_GetObjectItem(root, "msg_name");
    if (!cJSON_IsString(item))
        goto error;
    strncpy(out_msg->msg_name, item->valuestring, APP_MSG_NAME_STR_LEN - 1);

    // timestamp
    item = cJSON_GetObjectItem(root, "timestamp");
    if (!cJSON_IsNumber(item))
        goto error;
    out_msg->timestamp = (uint64_t)item->valuedouble;

    // data
    item = cJSON_GetObjectItem(root, "data");
    if (item)
    {
        out_msg->data_json = cJSON_Duplicate(item, 1);  // 深拷贝
        if (!out_msg->data_json)
            goto error;
    }

    cJSON_Delete(root);
    return 0;

error:
    if (out_msg->data_json)
    {
        cJSON_Delete(out_msg->data_json);
        out_msg->data_json = NULL;
    }

    cJSON_Delete(root);
    return -1;
}

int app_protocol_encode_reply_msg(const app_reply_msg_t *msg, char *out_json, size_t out_json_size)
{
    if (!msg || !out_json || out_json_size == 0U)
        return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return -1;

    if (!cJSON_AddNumberToObject(root, "msg_type_id", (double)msg->msg_type_id))
        goto error;

    if (!cJSON_AddNumberToObject(root, "msg_seq", (double)msg->msg_seq))
        goto error;

    if (!cJSON_AddStringToObject(root, "msg_type", msg->msg_type))
        goto error;

    if (!cJSON_AddStringToObject(root, "msg_name", msg->msg_name))
        goto error;

    if (!cJSON_AddNumberToObject(root, "timestamp", (double)msg->timestamp))
        goto error;


    if (msg->data_json)
    {
        cJSON *data_dup = cJSON_Duplicate(msg->data_json, 1);
        if (!data_dup)
            goto error;

        if (!cJSON_AddItemToObject(root, "data", data_dup))
        {
            cJSON_Delete(data_dup);
            goto error;
        }
    }
    else
    {
        cJSON *empty_data = cJSON_CreateObject();
        if (!empty_data)
            goto error;

        if (!cJSON_AddItemToObject(root, "data", empty_data))
        {
            cJSON_Delete(empty_data);
            goto error;
        }
    }

    if (!cJSON_PrintPreallocated(root, out_json, (int)out_json_size, 0))
        goto error;

    cJSON_Delete(root);
    return 0;

error:
    cJSON_Delete(root);
    return -1;
}



void app_protocol_free_cmd_msg(app_cmd_msg_t *msg)
{
    if (!msg)
        return;

    if (msg->data_json)
    {
        cJSON_Delete(msg->data_json);
        msg->data_json = NULL;
    }
}

void app_protocol_free_reply_msg(app_reply_msg_t *msg)
{
    if (!msg)
        return;

    if (msg->data_json)
    {
        cJSON_Delete(msg->data_json);
        msg->data_json = NULL;
    }
}
