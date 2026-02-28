#include "app_cmd_handle.h"
#include "motor.h"
#include <stdbool.h>
#include "cJSON.h"
#include <string.h>


static bool create_reply_msg(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg, int msg_type_id, const char *result, int error_code, const char *error_message)
{
    if (!cmdMsg || !replyMsg || !result || !error_message)
        return false;

    memset(replyMsg, 0, sizeof(app_reply_msg_t));
    replyMsg->msg_type_id = msg_type_id;  // ACK类型
    replyMsg->msg_seq = cmdMsg->msg_seq;
    strncpy(replyMsg->msg_type, "ack", APP_MSG_TYPE_STR_LEN - 1U);
    strncpy(replyMsg->msg_name, "ack_", APP_MSG_NAME_STR_LEN - 1U);
    strncat(replyMsg->msg_name, cmdMsg->msg_name, APP_MSG_NAME_STR_LEN - strlen(replyMsg->msg_name) - 1U);
    replyMsg->timestamp = (uint64_t)HAL_GetTick();

    cJSON *data_json = cJSON_CreateObject();
    if (!data_json)
        return false;

    if (!cJSON_AddNumberToObject(data_json, "original_msg_type_id", cmdMsg->msg_type_id))
        goto error;
    if(!cJSON_AddNumberToObject(data_json, "original_msg_seq", cmdMsg->msg_seq))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "result", result))
        goto error;
    if(!cJSON_AddNumberToObject(data_json, "error_code", error_code))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "error_message", error_message))
        goto error;

    replyMsg->data_json = data_json;
    return true;
error:
    cJSON_Delete(data_json);
    return false;
}


static bool parse_move_cmd(const app_cmd_msg_t *cmdMsg, char *direction, float *speed)
{
    if (!cmdMsg || !direction || !speed)
        return false;

    cJSON *item = cJSON_GetObjectItem(cmdMsg->data_json, "direction");
    if (!cJSON_IsString(item))
        return false;
    strncpy(direction, item->valuestring, 15U);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed = (float)item->valuedouble;

    return true;
}

static bool parse_movexy_cmd(const app_cmd_msg_t *cmdMsg, char *direction_xy, float *speed_xy, char *direction_lr, float *speed_lr)
{
    if (!cmdMsg || !direction_xy || !speed_xy || !direction_lr || !speed_lr)
        return false;

    cJSON *item = cJSON_GetObjectItem(cmdMsg->data_json, "lr_direction");
    if (!cJSON_IsString(item))
        return false;
    strncpy(direction_lr, item->valuestring, 15U);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "lr_speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed_lr = (float)item->valuedouble;

    item = cJSON_GetObjectItem(cmdMsg->data_json, "ud_direction");
    if (!cJSON_IsString(item))
        return false;
    strncpy(direction_xy, item->valuestring, 15U);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "ud_speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed_xy = (float)item->valuedouble;

    return true;
}


bool app_move_lr_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{

    char direction[16];
    float speed;

    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_move_cmd(cmdMsg, direction, &speed))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x10, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
        
    if(strcmp(direction, "left") == 0)
    {
        Motor_MoveLeft(speed);
    }
    else if(strcmp(direction, "right") == 0)
    {
        Motor_MoveRight(speed);
    }
    else if(strcmp(direction, "stop") == 0)
    {
        Motor_StopMove();
    }
    else {
        create_reply_msg(cmdMsg, replyMsg, 0x10, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
    
    create_reply_msg(cmdMsg, replyMsg, 0x10, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
        
    return true;
}


bool app_move_ud_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char direction[16];
    float speed;

    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_move_cmd(cmdMsg, direction, &speed))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x11, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(direction, "up") == 0)
    {
        Motor_MoveUp(speed);
    }
    else if(strcmp(direction, "down") == 0)
    {
        Motor_MoveDown(speed);
    }
    else if(strcmp(direction, "stop") == 0)
    {
        Motor_StopMove();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x11, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
        
    create_reply_msg(cmdMsg, replyMsg, 0x11, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}

bool app_move_xy_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char direction_xy[16];
    float speed_xy;
    char direction_lr[16];
    float speed_lr;

    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_movexy_cmd(cmdMsg, direction_xy, &speed_xy, direction_lr, &speed_lr))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    // 处理XY移动命令的逻辑
    if(strcmp(direction_lr, "left") == 0)
    {
        Motor_MoveLeft(speed_lr);
    }
    else if(strcmp(direction_lr, "right") == 0)
    {
        Motor_MoveRight(speed_lr);
    }
    else if(strcmp(direction_lr, "stop") == 0)
    {
        Motor_StopMove();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
    // 处理UD移动命令的逻辑
    if(strcmp(direction_xy, "up") == 0)
    {
        Motor_MoveUp(speed_xy);
    }
    else if(strcmp(direction_xy, "down") == 0)
    {
        Motor_MoveDown(speed_xy);
    }
    else if(strcmp(direction_xy, "stop") == 0)
    {
        Motor_StopMove();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }  

    create_reply_msg(cmdMsg, replyMsg, 0x12, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}


bool app_track_switch_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    // 处理跟踪开关命令的逻辑
    return true;
}

bool app_ball_lock_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    // 处理球头锁定命令的逻辑
    return true;
}

bool app_brush_control_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    // 处理滚刷控制命令的逻辑
    return true;
}

bool app_query_status_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    // 处理查询状态命令的逻辑
    return true;
}

bool app_query_pressure_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    // 处理查询压力命令的逻辑
    return true;
}
