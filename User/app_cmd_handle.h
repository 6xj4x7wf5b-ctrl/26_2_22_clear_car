#ifndef __APP_CMD_HANDLE_H__
#define __APP_CMD_HANDLE_H__

#include "app_protocol_types.h"
#include <stdbool.h>

// 创建 Message  -- 被动回复类
bool create_cmd_reply_msg(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg, int msg_type_id, const char *result, int error_code, const char *error_message);
// 创建 Message  -- 主动上报类
bool create_pressure_reply_msg(app_reply_msg_t *pressureMsg, float weight, bool status);
bool create_imu_reply_msg(app_reply_msg_t *imuMsg,
                          float quat_w, float quat_x, float quat_y, float quat_z,
                          float angular_velocity_x, float angular_velocity_y, float angular_velocity_z,
                          float linear_acceleration_x, float linear_acceleration_y, float linear_acceleration_z);
bool create_safety_edge_reply_msg(app_reply_msg_t *safetyEdgeMsg, bool detect, char* position, char* level);
bool create_device_status_reply_msg(app_reply_msg_t *deviceStatusMsg);

// 被动回复命令
bool app_move_lr_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_move_ud_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_move_xy_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_track_switch_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_ball_lock_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_brush_control_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);

// 主动上报命令
bool app_query_imu_handle(app_reply_msg_t *imuMsg);
bool app_query_pressure_handle(app_reply_msg_t *pressureMsg);
bool app_query_safety_edge_handle(app_reply_msg_t *safetyEdgeMsg, bool *detect);
bool app_query_device_status_handle(app_reply_msg_t *deviceStatusMsg);




#endif /* __APP_CMD_HANDLE_H__ */
