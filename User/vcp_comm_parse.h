#pragma once

#include "jsmn.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VCP_COMM_PARSE_MAX_TOKENS       128u
#define VCP_COMM_PARSE_MSG_TYPE_MAX_LEN 8u
#define VCP_COMM_PARSE_MSG_NAME_MAX_LEN 32u

typedef enum
{
	VCP_COMM_MSG_TYPE_UNKNOWN = 0,
	VCP_COMM_MSG_TYPE_CMD,
	VCP_COMM_MSG_TYPE_ACK,
	VCP_COMM_MSG_TYPE_REPORT
} VcpCommMsgType;

typedef enum
{
	VCP_COMM_PARSE_OK = 0,
	VCP_COMM_PARSE_ERR_PARAM,
	VCP_COMM_PARSE_ERR_JSON,
	VCP_COMM_PARSE_ERR_FORMAT,
	VCP_COMM_PARSE_ERR_MISSING_FIELD,
	VCP_COMM_PARSE_ERR_RANGE
} VcpCommParseStatus;

typedef struct
{
	/* 协议公共头字段（与 clean_car.md 的通用消息格式一致） */
	int32_t msg_type_id;
	int32_t msg_seq;
	char msg_type[VCP_COMM_PARSE_MSG_TYPE_MAX_LEN];
	VcpCommMsgType msg_type_enum;
	char msg_name[VCP_COMM_PARSE_MSG_NAME_MAX_LEN];
	uint8_t has_msg_name;
	int64_t timestamp;
	/* data 字段在 tokens[] 中对应的对象 token 索引 */
	int data_token;
} VcpCommMessage;

typedef struct
{
	/* 原始 JSON 与 jsmn 解析上下文 */
	const char *json;
	uint16_t json_len;
	jsmntok_t tokens[VCP_COMM_PARSE_MAX_TOKENS];
	int token_count;
	int root_token;
	/* 协议层可直接消费的结构化消息 */
	VcpCommMessage message;
} VcpCommParsedMessage;

VcpCommParseStatus VcpCommParse_Message(const uint8_t *json,
																				uint16_t json_len,
																				VcpCommParsedMessage *out_msg);
const VcpCommMessage *VcpCommParse_GetMessage(const VcpCommParsedMessage *msg);

int VcpCommParse_FindKey(const VcpCommParsedMessage *msg,
													 int object_token,
													 const char *key);
int VcpCommParse_GetDataToken(const VcpCommParsedMessage *msg);

bool VcpCommParse_GetString(const VcpCommParsedMessage *msg,
														int token,
														char *out,
														uint16_t out_len);
bool VcpCommParse_GetInt32(const VcpCommParsedMessage *msg, int token, int32_t *out);
bool VcpCommParse_GetInt64(const VcpCommParsedMessage *msg, int token, int64_t *out);
bool VcpCommParse_GetFloat(const VcpCommParsedMessage *msg, int token, float *out);

bool VcpCommParse_DataGetString(const VcpCommParsedMessage *msg,
																const char *key,
																char *out,
																uint16_t out_len);
bool VcpCommParse_DataGetInt32(const VcpCommParsedMessage *msg,
															 const char *key,
															 int32_t *out);
bool VcpCommParse_DataGetFloat(const VcpCommParsedMessage *msg,
															 const char *key,
															 float *out);

#ifdef __cplusplus
}
#endif
