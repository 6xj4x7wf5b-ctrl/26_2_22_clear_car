#ifndef APP_PROTOCOL_CODEC_H
#define APP_PROTOCOL_CODEC_H

#include "app_protocol_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 此函数将JSON字符串解析为app_cmd_msg_t结构体，返回0表示成功，非0表示失败
int app_protocol_decode_cmd_msg(const char *json_str, app_cmd_msg_t *out_msg);

// 此函数将app_cmd_msg_t结构体编码为JSON字符串，返回0表示成功，非0表示失败
int app_protocol_encode_reply_msg(const app_reply_msg_t *msg, char *out_json, size_t out_json_size);


void app_protocol_free_cmd_msg(app_cmd_msg_t *msg);
void app_protocol_free_reply_msg(app_reply_msg_t *msg);


#ifdef __cplusplus
}
#endif

#endif
