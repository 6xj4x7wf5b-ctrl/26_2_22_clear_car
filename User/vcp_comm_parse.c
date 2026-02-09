#include "vcp_comm_parse.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int VcpCommParse_TokenSpan(const jsmntok_t *tokens, int token_count, int index)
{
	int i = 0;
	int next = index + 1;

	if ((tokens == NULL) || (index < 0) || (index >= token_count))
	{
		return token_count;
	}

	switch (tokens[index].type)
	{
	case JSMN_OBJECT:
	case JSMN_ARRAY:
		for (i = 0; i < tokens[index].size; i++)
		{
			next = VcpCommParse_TokenSpan(tokens, token_count, next);
		}
		return next;
	default:
		return next;
	}
}

static bool VcpCommParse_TokenEquals(const char *json, const jsmntok_t *tok, const char *str)
{
	int len = 0;

	if ((json == NULL) || (tok == NULL) || (str == NULL) || (tok->type != JSMN_STRING))
	{
		return false;
	}

	len = tok->end - tok->start;
	if (len < 0)
	{
		return false;
	}

	return ((int)strlen(str) == len) && (strncmp(json + tok->start, str, (size_t)len) == 0);
}

static bool VcpCommParse_TokenToBuffer(const char *json,
																			 const jsmntok_t *tok,
																			 char *out,
																			 uint16_t out_len)
{
	int len = 0;

	if ((json == NULL) || (tok == NULL) || (out == NULL) || (out_len == 0u))
	{
		return false;
	}

	len = tok->end - tok->start;
	if ((len < 0) || ((uint16_t)len >= out_len))
	{
		return false;
	}

	memcpy(out, json + tok->start, (size_t)len);
	out[len] = '\0';
	return true;
}

static bool VcpCommParse_IsWhitespace(char ch)
{
	return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
}

static bool VcpCommParse_RootOnly(const VcpCommParsedMessage *msg)
{
	int i = 0;
	int root_end = 0;

	if ((msg == NULL) || (msg->json == NULL) || (msg->root_token < 0) ||
			(msg->root_token >= msg->token_count))
	{
		return false;
	}

	root_end = msg->tokens[msg->root_token].end;
	if ((root_end < 0) || (root_end > (int)msg->json_len))
	{
		return false;
	}

	for (i = root_end; i < (int)msg->json_len; i++)
	{
		if (!VcpCommParse_IsWhitespace(msg->json[i]))
		{
			return false;
		}
	}

	return true;
}

static VcpCommMsgType VcpCommParse_MsgTypeFromString(const char *msg_type)
{
	if (msg_type == NULL)
	{
		return VCP_COMM_MSG_TYPE_UNKNOWN;
	}
	if (strcmp(msg_type, "cmd") == 0)
	{
		return VCP_COMM_MSG_TYPE_CMD;
	}
	if (strcmp(msg_type, "ack") == 0)
	{
		return VCP_COMM_MSG_TYPE_ACK;
	}
	if (strcmp(msg_type, "report") == 0)
	{
		return VCP_COMM_MSG_TYPE_REPORT;
	}
	return VCP_COMM_MSG_TYPE_UNKNOWN;
}

static int VcpCommParse_FindKeyInternal(const char *json,
																				const jsmntok_t *tokens,
																				int token_count,
																				int object_token,
																				const char *key)
{
	int i = 0;
	int index = object_token + 1;
	int child_count = 0;

	if ((json == NULL) || (tokens == NULL) || (key == NULL))
	{
		return -1;
	}

	if ((object_token < 0) || (object_token >= token_count))
	{
		return -1;
	}

	if (tokens[object_token].type != JSMN_OBJECT)
	{
		return -1;
	}

	child_count = tokens[object_token].size;
	if ((child_count < 0) || ((child_count & 0x01) != 0))
	{
		return -1;
	}

	for (i = 0; i < child_count; i += 2)
	{
		int key_token = index;
		int value_token = VcpCommParse_TokenSpan(tokens, token_count, key_token);
		if ((value_token <= key_token) || (value_token >= token_count))
		{
			return -1;
		}

		if (VcpCommParse_TokenEquals(json, &tokens[key_token], key))
		{
			return value_token;
		}

		index = VcpCommParse_TokenSpan(tokens, token_count, value_token);
		if ((index <= value_token) || (index > token_count))
		{
			return -1;
		}
	}

	return -1;
}

int VcpCommParse_FindKey(const VcpCommParsedMessage *msg, int object_token, const char *key)
{
	if (msg == NULL)
	{
		return -1;
	}

	return VcpCommParse_FindKeyInternal(msg->json, msg->tokens, msg->token_count, object_token, key);
}

bool VcpCommParse_GetString(const VcpCommParsedMessage *msg,
														int token,
														char *out,
														uint16_t out_len)
{
	if ((msg == NULL) || (token < 0) || (token >= msg->token_count) || (out == NULL))
	{
		return false;
	}

	if (msg->tokens[token].type != JSMN_STRING)
	{
		return false;
	}

	return VcpCommParse_TokenToBuffer(msg->json, &msg->tokens[token], out, out_len);
}

bool VcpCommParse_GetInt32(const VcpCommParsedMessage *msg, int token, int32_t *out)
{
	char num_buf[24];
	char *endptr = NULL;
	long value = 0;

	if ((msg == NULL) || (out == NULL) || (token < 0) || (token >= msg->token_count))
	{
		return false;
	}

	if (msg->tokens[token].type != JSMN_PRIMITIVE)
	{
		return false;
	}

	if (!VcpCommParse_TokenToBuffer(msg->json, &msg->tokens[token], num_buf, (uint16_t)sizeof(num_buf)))
	{
		return false;
	}

	errno = 0;
	value = strtol(num_buf, &endptr, 10);
	if ((endptr == num_buf) || (*endptr != '\0') || (errno == ERANGE) ||
			(value < (long)INT32_MIN) || (value > (long)INT32_MAX))
	{
		return false;
	}

	*out = (int32_t)value;
	return true;
}

bool VcpCommParse_GetInt64(const VcpCommParsedMessage *msg, int token, int64_t *out)
{
	char num_buf[32];
	char *endptr = NULL;
	long long value = 0;

	if ((msg == NULL) || (out == NULL) || (token < 0) || (token >= msg->token_count))
	{
		return false;
	}

	if (msg->tokens[token].type != JSMN_PRIMITIVE)
	{
		return false;
	}

	if (!VcpCommParse_TokenToBuffer(msg->json, &msg->tokens[token], num_buf, (uint16_t)sizeof(num_buf)))
	{
		return false;
	}

	errno = 0;
	value = strtoll(num_buf, &endptr, 10);
	if ((endptr == num_buf) || (*endptr != '\0') || (errno == ERANGE) ||
			(value < (long long)INT64_MIN) || (value > (long long)INT64_MAX))
	{
		return false;
	}

	*out = (int64_t)value;
	return true;
}

bool VcpCommParse_GetFloat(const VcpCommParsedMessage *msg, int token, float *out)
{
	char num_buf[24];
	char *endptr = NULL;
	float value = 0.0f;

	if ((msg == NULL) || (out == NULL) || (token < 0) || (token >= msg->token_count))
	{
		return false;
	}

	if (msg->tokens[token].type != JSMN_PRIMITIVE)
	{
		return false;
	}

	if (!VcpCommParse_TokenToBuffer(msg->json, &msg->tokens[token], num_buf, (uint16_t)sizeof(num_buf)))
	{
		return false;
	}

	errno = 0;
	value = strtof(num_buf, &endptr);
	if ((endptr == num_buf) || (*endptr != '\0') || (errno == ERANGE) ||
			(value != value) || (value > FLT_MAX) || (value < -FLT_MAX))
	{
		return false;
	}

	*out = value;
	return true;
}

VcpCommParseStatus VcpCommParse_Message(const uint8_t *json,
																				uint16_t json_len,
																				VcpCommParsedMessage *out_msg)
{
	jsmn_parser parser;
	int ret = 0;
	int token = -1;

	if ((json == NULL) || (json_len == 0u) || (out_msg == NULL))
	{
		return VCP_COMM_PARSE_ERR_PARAM;
	}

	memset(out_msg, 0, sizeof(*out_msg));
	out_msg->json = (const char *)json;
	out_msg->json_len = json_len;
	out_msg->root_token = -1;
	out_msg->message.data_token = -1;

	jsmn_init(&parser);
	ret = jsmn_parse(&parser,
									 out_msg->json,
									 (size_t)json_len,
									 out_msg->tokens,
									 VCP_COMM_PARSE_MAX_TOKENS);
	if (ret < 0)
	{
		return VCP_COMM_PARSE_ERR_JSON;
	}

	out_msg->token_count = ret;
	if ((ret <= 0) || (out_msg->tokens[0].type != JSMN_OBJECT))
	{
		return VCP_COMM_PARSE_ERR_FORMAT;
	}
	out_msg->root_token = 0;
	if (!VcpCommParse_RootOnly(out_msg))
	{
		return VCP_COMM_PARSE_ERR_FORMAT;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "msg_type_id");
	if ((token < 0) || (!VcpCommParse_GetInt32(out_msg, token, &out_msg->message.msg_type_id)))
	{
		return VCP_COMM_PARSE_ERR_MISSING_FIELD;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "msg_seq");
	if ((token < 0) || (!VcpCommParse_GetInt32(out_msg, token, &out_msg->message.msg_seq)))
	{
		return VCP_COMM_PARSE_ERR_MISSING_FIELD;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "msg_type");
	if ((token < 0) ||
			(!VcpCommParse_GetString(out_msg,
															 token,
															 out_msg->message.msg_type,
															 sizeof(out_msg->message.msg_type))))
	{
		return VCP_COMM_PARSE_ERR_MISSING_FIELD;
	}
	out_msg->message.msg_type_enum = VcpCommParse_MsgTypeFromString(out_msg->message.msg_type);
	if (out_msg->message.msg_type_enum == VCP_COMM_MSG_TYPE_UNKNOWN)
	{
		return VCP_COMM_PARSE_ERR_RANGE;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "timestamp");
	if ((token < 0) || (!VcpCommParse_GetInt64(out_msg, token, &out_msg->message.timestamp)))
	{
		return VCP_COMM_PARSE_ERR_MISSING_FIELD;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "msg_name");
	if (token >= 0)
	{
		if (!VcpCommParse_GetString(out_msg,
																token,
																out_msg->message.msg_name,
																sizeof(out_msg->message.msg_name)))
		{
			return VCP_COMM_PARSE_ERR_RANGE;
		}
		out_msg->message.has_msg_name = 1u;
	}

	token = VcpCommParse_FindKey(out_msg, out_msg->root_token, "data");
	if (token < 0)
	{
		return VCP_COMM_PARSE_ERR_MISSING_FIELD;
	}
	if (out_msg->tokens[token].type != JSMN_OBJECT)
	{
		return VCP_COMM_PARSE_ERR_FORMAT;
	}
	out_msg->message.data_token = token;

	return VCP_COMM_PARSE_OK;
}

const VcpCommMessage *VcpCommParse_GetMessage(const VcpCommParsedMessage *msg)
{
	if (msg == NULL)
	{
		return NULL;
	}
	return &msg->message;
}

int VcpCommParse_GetDataToken(const VcpCommParsedMessage *msg)
{
	if (msg == NULL)
	{
		return -1;
	}
	return msg->message.data_token;
}

bool VcpCommParse_DataGetString(const VcpCommParsedMessage *msg,
																const char *key,
																char *out,
																uint16_t out_len)
{
	int token = -1;

	if ((msg == NULL) || (key == NULL))
	{
		return false;
	}

	token = VcpCommParse_FindKey(msg, msg->message.data_token, key);
	if (token < 0)
	{
		return false;
	}

	return VcpCommParse_GetString(msg, token, out, out_len);
}

bool VcpCommParse_DataGetInt32(const VcpCommParsedMessage *msg,
															 const char *key,
															 int32_t *out)
{
	int token = -1;

	if ((msg == NULL) || (key == NULL))
	{
		return false;
	}

	token = VcpCommParse_FindKey(msg, msg->message.data_token, key);
	if (token < 0)
	{
		return false;
	}

	return VcpCommParse_GetInt32(msg, token, out);
}

bool VcpCommParse_DataGetFloat(const VcpCommParsedMessage *msg,
															 const char *key,
															 float *out)
{
	int token = -1;

	if ((msg == NULL) || (key == NULL))
	{
		return false;
	}

	token = VcpCommParse_FindKey(msg, msg->message.data_token, key);
	if (token < 0)
	{
		return false;
	}

	return VcpCommParse_GetFloat(msg, token, out);
}
