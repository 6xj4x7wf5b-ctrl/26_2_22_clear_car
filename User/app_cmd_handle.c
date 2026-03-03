#include "app_cmd_handle.h"
#include "motor.h"
#include <stdbool.h>
#include "cJSON.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
#include "pressure_sensor.h"
#include "safety_edge.h"

typedef struct 
{
    char lr_direction[16];
    float lr_speed;
    char ud_direction[16];
    float ud_speed;
    uint8_t battery_level;
    float voltage;
    float current;
    bool charging;
    char track_mode[16];
    char ball_lock_status[16];
    bool brush_running;
    float brush_speed;
    char brush_direction[16];
    float temperature;
    uint32_t error_code;
    char error_msg[64];
} app_device_status_t;


static uint32_t app_msg_id_seq = 0U;
static app_device_status_t app_device_status;






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


// ============================================== 创建 Message =================================================================

bool create_cmd_reply_msg(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg, int msg_type_id, const char *result, int error_code, const char *error_message)
{
    if (!cmdMsg || !replyMsg || !result || !error_message)
        return false;

    memset(replyMsg, 0, sizeof(app_reply_msg_t));
    replyMsg->msg_type_id = msg_type_id;  // ACK类型
    replyMsg->msg_seq = cmdMsg->msg_seq;
    (void)snprintf(replyMsg->msg_type, APP_MSG_TYPE_STR_LEN, "%s", "ack");
    (void)snprintf(replyMsg->msg_name, APP_MSG_NAME_STR_LEN, "ack_%.*s", (int)(APP_MSG_NAME_STR_LEN - sizeof("ack_")), cmdMsg->msg_name);
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
    if(!cJSON_AddStringToObject(data_json, "error_msg", error_message))
        goto error;

    replyMsg->data_json = data_json;
    return true;
error:
    cJSON_Delete(data_json);
    return false;
}

bool create_pressure_reply_msg(app_reply_msg_t *pressureMsg, float weight, bool status)
{
    if (!pressureMsg)
        return false;

    memset(pressureMsg, 0, sizeof(app_reply_msg_t));
    pressureMsg->msg_type_id = 0x20;  // ACK类型
    pressureMsg->msg_seq = app_msg_id_seq++;
    (void)snprintf(pressureMsg->msg_type, APP_MSG_TYPE_STR_LEN, "%s", "report");
    (void)snprintf(pressureMsg->msg_name, APP_MSG_NAME_STR_LEN, "%s", "pressure");
    pressureMsg->timestamp = (uint64_t)HAL_GetTick();

    cJSON *data_json = cJSON_CreateObject();

    if (!data_json)
        return false;

    if(!cJSON_AddStringToObject(data_json, "data_type", "pressure"))
        goto error;
    if(!cJSON_AddNumberToObject(data_json, "pressure_value", weight))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "unit", "unknown"))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "status", status ? "normal" : "abnormal"))
        goto error;

    pressureMsg->data_json = data_json;
    return true;
error:
    cJSON_Delete(data_json);
    return false;
}

bool create_safety_edge_reply_msg(app_reply_msg_t *safetyEdgeMsg, bool detect, char* position, char* level)
{
    if (!safetyEdgeMsg || !position || !level)
        return false;

    memset(safetyEdgeMsg, 0, sizeof(app_reply_msg_t));
    safetyEdgeMsg->msg_type_id = 0x21;  // ACK类型
    safetyEdgeMsg->msg_seq = app_msg_id_seq++;
    (void)snprintf(safetyEdgeMsg->msg_type, APP_MSG_TYPE_STR_LEN, "%s", "report");
    (void)snprintf(safetyEdgeMsg->msg_name, APP_MSG_NAME_STR_LEN, "%s", "safety_edge");
    safetyEdgeMsg->timestamp = (uint64_t)HAL_GetTick();

    cJSON *data_json = cJSON_CreateObject();
    if (!data_json)
        return false;

    if(!cJSON_AddStringToObject(data_json, "data_type", "safety_edge"))
        goto error;
    if(!cJSON_AddBoolToObject(data_json, "collision_detected", detect))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "edge_position", position))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "collision_level", level))
        goto error;
    if(!cJSON_AddNumberToObject(data_json, "timestamp", safetyEdgeMsg->timestamp))
        goto error;

    safetyEdgeMsg->data_json = data_json;
    return true;
error:
    cJSON_Delete(data_json);
    return false;
}

bool create_device_status_reply_msg(app_reply_msg_t *deviceStatusMsg)
{
    if (!deviceStatusMsg)
        return false;

    memset(deviceStatusMsg, 0, sizeof(app_reply_msg_t));
    deviceStatusMsg->msg_type_id = APP_MSG_TYPE_ID_REPORT_DEVICE_STAT;
    deviceStatusMsg->msg_seq = app_msg_id_seq++;
    (void)snprintf(deviceStatusMsg->msg_type, APP_MSG_TYPE_STR_LEN, "%s", "report");
    (void)snprintf(deviceStatusMsg->msg_name, APP_MSG_NAME_STR_LEN, "%s", "device_status");
    deviceStatusMsg->timestamp = (uint64_t)HAL_GetTick();

    cJSON *data_json = cJSON_CreateObject();
    cJSON *motion_status = NULL;
    cJSON *power_status = NULL;
    cJSON *brush_status = NULL;
    cJSON *system_status = NULL;
    if (!data_json)
        return false;

    if(!cJSON_AddStringToObject(data_json, "data_type", "device_status"))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "device_id", "CLEAN_CAR_001"))
        goto error;

    motion_status = cJSON_CreateObject();
    if (!motion_status)
        goto error;
    if(!cJSON_AddStringToObject(motion_status, "lr_direction", app_device_status.lr_direction))
        goto error;
    if(!cJSON_AddNumberToObject(motion_status, "lr_speed", app_device_status.lr_speed))
        goto error;
    if(!cJSON_AddStringToObject(motion_status, "ud_direction", app_device_status.ud_direction))
        goto error;
    if(!cJSON_AddNumberToObject(motion_status, "ud_speed", app_device_status.ud_speed))
        goto error;
    if(!cJSON_AddItemToObject(data_json, "motion_status", motion_status))
        goto error;
    motion_status = NULL;

    power_status = cJSON_CreateObject();
    if (!power_status)
        goto error;
    if(!cJSON_AddNumberToObject(power_status, "battery_level", app_device_status.battery_level))
        goto error;
    if(!cJSON_AddNumberToObject(power_status, "voltage", app_device_status.voltage))
        goto error;
    if(!cJSON_AddNumberToObject(power_status, "current", app_device_status.current))
        goto error;
    if(!cJSON_AddBoolToObject(power_status, "charging", app_device_status.charging))
        goto error;
    if(!cJSON_AddItemToObject(data_json, "power_status", power_status))
        goto error;
    power_status = NULL;

    if(!cJSON_AddStringToObject(data_json, "track_mode", app_device_status.track_mode))
        goto error;
    if(!cJSON_AddStringToObject(data_json, "ball_lock_status", app_device_status.ball_lock_status))
        goto error;

    brush_status = cJSON_CreateObject();
    if (!brush_status)
        goto error;
    if(!cJSON_AddBoolToObject(brush_status, "running", app_device_status.brush_running))
        goto error;
    if(!cJSON_AddNumberToObject(brush_status, "speed", app_device_status.brush_speed))
        goto error;
    if(!cJSON_AddStringToObject(brush_status, "direction", app_device_status.brush_direction))
        goto error;
    if(!cJSON_AddItemToObject(data_json, "brush_status", brush_status))
        goto error;
    brush_status = NULL;

    system_status = cJSON_CreateObject();
    if (!system_status)
        goto error;
    if(!cJSON_AddNumberToObject(system_status, "temperature", app_device_status.temperature))
        goto error;
    if(!cJSON_AddNumberToObject(system_status, "error_code", app_device_status.error_code))
        goto error;
    if(!cJSON_AddStringToObject(system_status, "error_msg", app_device_status.error_msg))
        goto error;
    if(!cJSON_AddItemToObject(data_json, "system_status", system_status))
        goto error;
    system_status = NULL;

    deviceStatusMsg->data_json = data_json;
    return true;

error:
    cJSON_Delete(motion_status);
    cJSON_Delete(power_status);
    cJSON_Delete(brush_status);
    cJSON_Delete(system_status);
    cJSON_Delete(data_json);
    return false;
}


// ============================================== 被动回复命令 =================================================================

bool app_move_lr_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{

    char direction[16];
    float speed;

    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_move_cmd(cmdMsg, direction, &speed))
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x10, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
        
    if(strcmp(direction, "left") == 0)
    {
        Motor_ChassisMoveLeft(speed);
    }
    else if(strcmp(direction, "right") == 0)
    {
        Motor_ChassisMoveRight(speed);
    }
    else if(strcmp(direction, "stop") == 0)
    {
        Motor_ChassisStopMove();
    }
    else {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x10, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
    
    create_cmd_reply_msg(cmdMsg, replyMsg, 0x10, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
        
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
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x11, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(direction, "up") == 0)
    {
        Motor_ChassisMoveUp(speed);
    }
    else if(strcmp(direction, "down") == 0)
    {
        Motor_ChassisMoveDown(speed);
    }
    else if(strcmp(direction, "stop") == 0)
    {
        Motor_ChassisStopMove();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x11, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
        
    create_cmd_reply_msg(cmdMsg, replyMsg, 0x11, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
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
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    // 处理XY移动命令的逻辑
    if(strcmp(direction_lr, "left") == 0)
    {
        Motor_ChassisMoveLeft(speed_lr);
    }
    else if(strcmp(direction_lr, "right") == 0)
    {
        Motor_ChassisMoveRight(speed_lr);
    }
    else if(strcmp(direction_lr, "stop") == 0)
    {
        Motor_ChassisStopMove();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }
    // 处理UD移动命令的逻辑
    if(strcmp(direction_xy, "up") == 0)
    {
        Motor_ChassisMoveUp(speed_xy);
    }
    else if(strcmp(direction_xy, "down") == 0)
    {
        Motor_ChassisMoveDown(speed_xy);
    }
    else if(strcmp(direction_xy, "stop") == 0)
    {
        Motor_ChassisStopMove();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x12, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }  

    create_cmd_reply_msg(cmdMsg, replyMsg, 0x12, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}


bool app_track_switch_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char track_mode[16];
    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_track_switch_cmd(cmdMsg, track_mode))
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x13, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(track_mode, "extend") == 0)      // 履带模式
    {
        Motor_CrawlerLiftUp();
    }
    else if(strcmp(track_mode, "retract") == 0)   // 轮式模式
    {
        Motor_CrawlerLiftDown();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x13, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_cmd_reply_msg(cmdMsg, replyMsg, 0x13, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}

bool app_ball_lock_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg)
{
    char action[16];
    if (!cmdMsg || !replyMsg)
        return false;

    if(!parse_ball_lock_cmd(cmdMsg, action))
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x14, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(action, "lock") == 0)
    {
        Motor_BallHeadLock();
    }
    else if(strcmp(action, "unlock") == 0)
    {
        Motor_BallHeadUnlock();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x14, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_cmd_reply_msg(cmdMsg, replyMsg, 0x14, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
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
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x15, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    if(strcmp(action, "start") == 0)
    {
        Motor_BrushStart(speed);
    }
    else if(strcmp(action, "stop") == 0)
    {
        Motor_BrushStop();
    }
    else
    {
        create_cmd_reply_msg(cmdMsg, replyMsg, 0x15, "failed", APP_ERROR_CODE_INVALID_FORMAT, "Invalid command");
        return false;
    }

    create_cmd_reply_msg(cmdMsg, replyMsg, 0x15, "success", APP_ERROR_CODE_NONE, "Command executed successfully");
    return true;
}


// ============================================== 主动上报命令 =================================================================
bool app_query_pressure_handle(app_reply_msg_t *pressureMsg)
{
    int32_t weight = 0;
    PressureSensor_Status status = PressureSensor_ReadWeight(&weight);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return create_pressure_reply_msg(pressureMsg, 0.0f, false);
    }
    return create_pressure_reply_msg(pressureMsg, (float)weight, true);
}



bool app_query_safety_edge_handle(app_reply_msg_t *safetyEdgeMsg, bool *detect)
{
    if ((safetyEdgeMsg == NULL) || (detect == NULL))
    {
        return false;
    }

    *detect = false;

    int edge_states = SafetyEdge_ReadAll();
    if(edge_states == 0x0F) // 0x0F表示四个边缘都未检测到
    {
        return true;
    }

    bool collision_detected = edge_states != 0;
    char position[16]; 
    if(edge_states & 0x01) {
        snprintf(position, sizeof(position), "front");
    } else if(edge_states & 0x02) {
        snprintf(position, sizeof(position), "rear");
    } else if(edge_states & 0x04) {
        snprintf(position, sizeof(position), "left");
    } else if(edge_states & 0x08) {
        snprintf(position, sizeof(position), "right");
    } else {
        snprintf(position, sizeof(position), "none");
    }

    char level[16] = "none";

    if (!create_safety_edge_reply_msg(safetyEdgeMsg, collision_detected, position, level))
    {
        return false;
    }

    *detect = collision_detected;
    return true;
}


bool app_query_device_status_handle(app_reply_msg_t *deviceStatusMsg)
{
    return create_device_status_reply_msg(deviceStatusMsg);
}
