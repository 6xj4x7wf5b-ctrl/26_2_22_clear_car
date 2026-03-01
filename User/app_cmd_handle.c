#include "app_cmd_handle.h"
#include "motor.h"
#include <stdbool.h>
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>


static bool create_reply_msg(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg, int msg_type_id, const char *result, int error_code, const char *error_message)
{
    if (!cmdMsg || !replyMsg || !result || !error_message)
        return false;

    memset(replyMsg, 0, sizeof(app_reply_msg_t));
    replyMsg->msg_type_id = msg_type_id;  // ACK类型
    replyMsg->msg_seq = cmdMsg->msg_seq;
    (void)snprintf(replyMsg->msg_type, APP_MSG_TYPE_STR_LEN, "%s", "ack");
    (void)snprintf(replyMsg->msg_name, APP_MSG_NAME_STR_LEN, "ack_%s", cmdMsg->msg_name);
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
    (void)snprintf(direction, 16U, "%s", item->valuestring);

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
    (void)snprintf(direction_lr, 16U, "%s", item->valuestring);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "lr_speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed_lr = (float)item->valuedouble;

    item = cJSON_GetObjectItem(cmdMsg->data_json, "ud_direction");
    if (!cJSON_IsString(item))
        return false;
    (void)snprintf(direction_xy, 16U, "%s", item->valuestring);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "ud_speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed_xy = (float)item->valuedouble;

    return true;
}

static bool parse_track_switch_cmd(const app_cmd_msg_t *cmdMsg, char *track_mode)
{
    if (!cmdMsg || !track_mode)
        return false;

    cJSON *item = cJSON_GetObjectItem(cmdMsg->data_json, "track_mode");
    if (!cJSON_IsString(item))
        return false;
    (void)snprintf(track_mode, 16U, "%s", item->valuestring);

    return true;
}

static bool parse_ball_lock_cmd(const app_cmd_msg_t *cmdMsg, char *action)
{
    if (!cmdMsg || !action)
        return false;

    cJSON *item = cJSON_GetObjectItem(cmdMsg->data_json, "action");
    if (!cJSON_IsString(item))
        return false;
    (void)snprintf(action, 16U, "%s", item->valuestring);

    return true;
}

static bool parse_brush_control_cmd(const app_cmd_msg_t *cmdMsg, char *action, float *speed)
{
    if (!cmdMsg || !action || !speed)
        return false;

    cJSON *item = cJSON_GetObjectItem(cmdMsg->data_json, "action");
    if (!cJSON_IsString(item))
        return false;
    (void)snprintf(action, 16U, "%s", item->valuestring);

    item = cJSON_GetObjectItem(cmdMsg->data_json, "speed");
    if (!cJSON_IsNumber(item))
        return false;
    *speed = (float)item->valuedouble;

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
    char track_mode[16];
    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_track_switch_cmd(cmdMsg, track_mode))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x13, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(track_mode, "crawler") == 0)      // 履带模式
    {
        // TrackSwitch_On();
    }
    else if(strcmp(track_mode, "wheel") == 0)   // 轮式模式
    {
        // TrackSwitch_Off();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x13, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_reply_msg(cmdMsg, replyMsg, 0x13, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}

bool app_ball_lock_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char action[16];
    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_ball_lock_cmd(cmdMsg, action))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x14, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(action, "lock") == 0)
    {
        // BallLock_On();
    }
    else if(strcmp(action, "unlock") == 0)
    {
        // BallLock_Off();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x14, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_reply_msg(cmdMsg, replyMsg, 0x14, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}

bool app_brush_control_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char action[16];
    float speed;
    if (!cmdMsg || !replyMsg)
        return false;
    if(!parse_brush_control_cmd(cmdMsg, action, &speed))
    {
        create_reply_msg(cmdMsg, replyMsg, 0x15, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(action, "start") == 0)
    {
        // Brush_Start(speed);
    }
    else if(strcmp(action, "stop") == 0)
    {
        // Brush_Stop();
    }
    else
    {
        create_reply_msg(cmdMsg, replyMsg, 0x15, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_reply_msg(cmdMsg, replyMsg, 0x15, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
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
