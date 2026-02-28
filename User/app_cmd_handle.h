#ifndef __APP_CMD_HANDLE_H__
#define __APP_CMD_HANDLE_H__

#include "app_protocol_types.h"
#include <stdbool.h>


bool app_move_lr_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_move_ud_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_move_xy_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_track_switch_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_ball_lock_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_brush_control_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_query_status_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);
bool app_query_pressure_handle(const app_cmd_msg_t *cmdMsg, app_reply_msg_t *replyMsg);





#endif /* __APP_CMD_HANDLE_H__ */