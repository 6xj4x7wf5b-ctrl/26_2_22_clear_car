#ifndef APP_PROTOCOL_TYPES_H
#define APP_PROTOCOL_TYPES_H

#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MSG_TYPE_STR_LEN   8U
#define APP_MSG_NAME_STR_LEN   32U
#define APP_MSG_DATA_JSON_LEN  192U
#define APP_ERR_MSG_STR_LEN    64U

typedef enum
{
  APP_MSG_TYPE_ID_CMD_MOVE_LR         = 0x01,
  APP_MSG_TYPE_ID_CMD_MOVE_UD         = 0x02,
  APP_MSG_TYPE_ID_CMD_MOVE_XY         = 0x03,
  APP_MSG_TYPE_ID_CMD_TRACK_SWITCH    = 0x04,
  APP_MSG_TYPE_ID_CMD_BALL_LOCK       = 0x05,
  APP_MSG_TYPE_ID_CMD_BRUSH_CONTROL   = 0x06,
  APP_MSG_TYPE_ID_CMD_QUERY_STATUS    = 0x07,
  APP_MSG_TYPE_ID_CMD_QUERY_PRESSURE  = 0x08,

  APP_MSG_TYPE_ID_ACK_MOVE_LR         = 0x10,
  APP_MSG_TYPE_ID_ACK_MOVE_UD         = 0x11,
  APP_MSG_TYPE_ID_ACK_MOVE_XY         = 0x12,
  APP_MSG_TYPE_ID_ACK_TRACK_SWITCH    = 0x13,
  APP_MSG_TYPE_ID_ACK_BALL_LOCK       = 0x14,
  APP_MSG_TYPE_ID_ACK_BRUSH_CONTROL   = 0x15,
  APP_MSG_TYPE_ID_ACK_QUERY_STATUS    = 0x16,
  APP_MSG_TYPE_ID_ACK_QUERY_PRESSURE  = 0x17,

  APP_MSG_TYPE_ID_REPORT_PRESSURE     = 0x20,
  APP_MSG_TYPE_ID_REPORT_SAFETY_EDGE  = 0x21,
  APP_MSG_TYPE_ID_REPORT_DEVICE_STAT  = 0x22,
  APP_MSG_TYPE_ID_REPORT_IMU          = 0x23
} app_msg_type_id_t;

typedef enum 
{
  APP_ERROR_CODE_NONE = 0,
  APP_ERROR_CODE_INVALID_FORMAT = 1,
  APP_ERROR_CODE_PARAM_OUT_OF_RANGE = 2,
  APP_ERROR_CODE_DEVICE_BUSY = 3,
  APP_ERROR_CODE_DEVICE_FAILURE = 4,
  APP_ERROR_CODE_SAFETY_PROTECTION_TRIGGERED = 5
} app_error_code_t;


typedef struct
{
  uint32_t msg_type_id;
  uint32_t msg_seq;
  char msg_type[APP_MSG_TYPE_STR_LEN];
  char msg_name[APP_MSG_NAME_STR_LEN];
  uint64_t timestamp;
  cJSON *data_json;;
} app_cmd_msg_t,app_reply_msg_t;


typedef struct
{
  char text[64];
} app_error_msg_t;

typedef struct
{
  uint32_t pressure;
  uint8_t safety_edge_triggered;
  uint8_t device_status;
  float imu_accel_x;
  float imu_accel_y;
  float imu_accel_z;
} app_device_status_t;

#ifdef __cplusplus
}
#endif

#endif
