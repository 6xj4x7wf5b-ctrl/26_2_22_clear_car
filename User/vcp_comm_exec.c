#include "vcp_comm_exec.h"

#include "cJSON.h"
#include "vcp_comm_rx.h"
#include "vcp_comm_tx.h"

#include <stdbool.h>
#include <string.h>

#define VCP_MSG_CMD_MOVE_LR        0x01
#define VCP_MSG_CMD_MOVE_UD        0x02
#define VCP_MSG_CMD_MOVE_XY        0x03
#define VCP_MSG_CMD_TRACK_SWITCH   0x04
#define VCP_MSG_CMD_BALL_LOCK      0x05
#define VCP_MSG_CMD_BRUSH_CONTROL  0x06
#define VCP_MSG_CMD_QUERY_STATUS   0x07
#define VCP_MSG_CMD_QUERY_PRESSURE 0x08

#define VCP_COMM_EXEC_MSG_TYPE_MAX_LEN 16u
#define VCP_COMM_EXEC_MSG_NAME_MAX_LEN 32u
#define VCP_COMM_EXEC_CMD_TYPE_MAX_LEN 32u

typedef enum
{
	VCP_COMM_EXEC_CMD_UNKNOWN = 0,
	VCP_COMM_EXEC_CMD_MOVE_LR,
	VCP_COMM_EXEC_CMD_MOVE_UD,
	VCP_COMM_EXEC_CMD_MOVE_XY,
	VCP_COMM_EXEC_CMD_TRACK_SWITCH,
	VCP_COMM_EXEC_CMD_BALL_LOCK,
	VCP_COMM_EXEC_CMD_BRUSH_CONTROL,
	VCP_COMM_EXEC_CMD_QUERY_STATUS,
	VCP_COMM_EXEC_CMD_QUERY_PRESSURE
} VcpCommExecCmdKind;

typedef struct
{
	int32_t msg_type_id;
	int32_t msg_seq;
	int64_t timestamp;
	char msg_type[VCP_COMM_EXEC_MSG_TYPE_MAX_LEN];
	char msg_name[VCP_COMM_EXEC_MSG_NAME_MAX_LEN];
	char cmd_type[VCP_COMM_EXEC_CMD_TYPE_MAX_LEN];
	const cJSON *data_obj;
} VcpCommExecRequest;

static VcpCommExecStats s_exec_stats;

static bool VcpCommExec_Streq(const char *a, const char *b)
{
	if ((a == NULL) || (b == NULL))
	{
		return false;
	}
	return (strcmp(a, b) == 0);
}

static void VcpCommExec_CopyString(char *dst, uint16_t dst_len, const char *src)
{
	uint16_t i = 0u;
	const char *src_ptr = src;

	if ((dst == NULL) || (dst_len == 0u))
	{
		return;
	}
	if (src_ptr == NULL)
	{
		src_ptr = "";
	}

	for (i = 0u; i < (uint16_t)(dst_len - 1u); i++)
	{
		if (src_ptr[i] == '\0')
		{
			break;
		}
		dst[i] = src_ptr[i];
	}
	dst[i] = '\0';
}

static bool VcpCommExec_ReadIntField(const cJSON *obj, const char *key, int32_t *out)
{
	const cJSON *item = NULL;

	if ((obj == NULL) || (key == NULL) || (out == NULL))
	{
		return false;
	}

	item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
	if (!cJSON_IsNumber(item))
	{
		return false;
	}

	*out = item->valueint;
	return true;
}

static bool VcpCommExec_ReadInt64Field(const cJSON *obj, const char *key, int64_t *out)
{
	const cJSON *item = NULL;

	if ((obj == NULL) || (key == NULL) || (out == NULL))
	{
		return false;
	}

	item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
	if (!cJSON_IsNumber(item))
	{
		return false;
	}

	*out = (int64_t)item->valuedouble;
	return true;
}

static bool VcpCommExec_ReadStringField(const cJSON *obj,
																				const char *key,
																				char *out,
																				uint16_t out_len)
{
	const cJSON *item = NULL;

	if ((obj == NULL) || (key == NULL) || (out == NULL) || (out_len == 0u))
	{
		return false;
	}

	item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
	if ((!cJSON_IsString(item)) || (item->valuestring == NULL))
	{
		return false;
	}

	VcpCommExec_CopyString(out, out_len, item->valuestring);
	return true;
}

static VcpCommExecStatus VcpCommExec_ParseRequest(const uint8_t *payload,
																									uint16_t payload_len,
																									cJSON **root_out,
																									VcpCommExecRequest *request)
{
	cJSON *root = NULL;
	const cJSON *data_obj = NULL;

	if ((payload == NULL) || (root_out == NULL) || (request == NULL))
	{
		return VCP_COMM_EXEC_ERR_PARAM;
	}
	if (payload_len == 0u)
	{
		return VCP_COMM_EXEC_ERR_JSON;
	}

	memset(request, 0, sizeof(*request));
	*root_out = NULL;

	root = cJSON_ParseWithLength((const char *)payload, (size_t)payload_len);
	if ((root == NULL) || (!cJSON_IsObject(root)))
	{
		if (root != NULL)
		{
			cJSON_Delete(root);
		}
		return VCP_COMM_EXEC_ERR_JSON;
	}

	if (!VcpCommExec_ReadIntField(root, "msg_type_id", &request->msg_type_id))
	{
		cJSON_Delete(root);
		return VCP_COMM_EXEC_ERR_JSON;
	}
	if (!VcpCommExec_ReadIntField(root, "msg_seq", &request->msg_seq))
	{
		cJSON_Delete(root);
		return VCP_COMM_EXEC_ERR_JSON;
	}
	if (!VcpCommExec_ReadStringField(root, "msg_type", request->msg_type, sizeof(request->msg_type)))
	{
		cJSON_Delete(root);
		return VCP_COMM_EXEC_ERR_JSON;
	}

	(void)VcpCommExec_ReadInt64Field(root, "timestamp", &request->timestamp);
	(void)VcpCommExec_ReadStringField(root, "msg_name", request->msg_name, sizeof(request->msg_name));

	data_obj = cJSON_GetObjectItemCaseSensitive(root, "data");
	if (cJSON_IsObject(data_obj))
	{
		request->data_obj = data_obj;
		(void)VcpCommExec_ReadStringField(data_obj, "cmd_type", request->cmd_type, sizeof(request->cmd_type));
	}

	*root_out = root;
	return VCP_COMM_EXEC_OK;
}

static VcpCommExecCmdKind VcpCommExec_MapCommand(const VcpCommExecRequest *request)
{
	if (request == NULL)
	{
		return VCP_COMM_EXEC_CMD_UNKNOWN;
	}

	switch (request->msg_type_id)
	{
	case VCP_MSG_CMD_MOVE_LR:
		return VCP_COMM_EXEC_CMD_MOVE_LR;
	case VCP_MSG_CMD_MOVE_UD:
		return VCP_COMM_EXEC_CMD_MOVE_UD;
	case VCP_MSG_CMD_MOVE_XY:
		return VCP_COMM_EXEC_CMD_MOVE_XY;
	case VCP_MSG_CMD_TRACK_SWITCH:
		return VCP_COMM_EXEC_CMD_TRACK_SWITCH;
	case VCP_MSG_CMD_BALL_LOCK:
		return VCP_COMM_EXEC_CMD_BALL_LOCK;
	case VCP_MSG_CMD_BRUSH_CONTROL:
		return VCP_COMM_EXEC_CMD_BRUSH_CONTROL;
	case VCP_MSG_CMD_QUERY_STATUS:
		return VCP_COMM_EXEC_CMD_QUERY_STATUS;
	case VCP_MSG_CMD_QUERY_PRESSURE:
		return VCP_COMM_EXEC_CMD_QUERY_PRESSURE;
	default:
		break;
	}

	if (VcpCommExec_Streq(request->cmd_type, "move_lr"))
	{
		return VCP_COMM_EXEC_CMD_MOVE_LR;
	}
	if (VcpCommExec_Streq(request->cmd_type, "move_ud"))
	{
		return VCP_COMM_EXEC_CMD_MOVE_UD;
	}
	if (VcpCommExec_Streq(request->cmd_type, "move_xy"))
	{
		return VCP_COMM_EXEC_CMD_MOVE_XY;
	}
	if (VcpCommExec_Streq(request->cmd_type, "track_switch"))
	{
		return VCP_COMM_EXEC_CMD_TRACK_SWITCH;
	}
	if (VcpCommExec_Streq(request->cmd_type, "ball_lock"))
	{
		return VCP_COMM_EXEC_CMD_BALL_LOCK;
	}
	if (VcpCommExec_Streq(request->cmd_type, "brush_control"))
	{
		return VCP_COMM_EXEC_CMD_BRUSH_CONTROL;
	}
	if (VcpCommExec_Streq(request->cmd_type, "query_status"))
	{
		return VCP_COMM_EXEC_CMD_QUERY_STATUS;
	}
	if (VcpCommExec_Streq(request->cmd_type, "query_pressure"))
	{
		return VCP_COMM_EXEC_CMD_QUERY_PRESSURE;
	}

	return VCP_COMM_EXEC_CMD_UNKNOWN;
}

static char *VcpCommExec_HandleMoveLr(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 move_lr 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleMoveUd(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 move_ud 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleMoveXy(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 move_xy 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleTrackSwitch(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 track_switch 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleBallLock(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 ball_lock 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleBrushControl(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 brush_control 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleQueryStatus(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 query_status 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static char *VcpCommExec_HandleQueryPressure(const VcpCommExecRequest *request)
{
	(void)request;
	/* TODO: 执行 query_pressure 操作并返回 cJSON_PrintUnformatted() 生成的 JSON 字符串 */
	return NULL;
}

static VcpCommExecStatus VcpCommExec_DispatchCommand(const VcpCommExecRequest *request)
{
	VcpCommExecCmdKind cmd_kind = VCP_COMM_EXEC_CMD_UNKNOWN;
	char *response_json = NULL;
	VcpCommTxStatus tx_status = VCP_COMM_TX_OK;

	if (request == NULL)
	{
		return VCP_COMM_EXEC_ERR_PARAM;
	}

	if (!VcpCommExec_Streq(request->msg_type, "cmd"))
	{
		s_exec_stats.unsupported_count++;
		return VCP_COMM_EXEC_ERR_UNSUPPORTED;
	}

	cmd_kind = VcpCommExec_MapCommand(request);
	switch (cmd_kind)
	{
	case VCP_COMM_EXEC_CMD_MOVE_LR:
		response_json = VcpCommExec_HandleMoveLr(request);
		break;
	case VCP_COMM_EXEC_CMD_MOVE_UD:
		response_json = VcpCommExec_HandleMoveUd(request);
		break;
	case VCP_COMM_EXEC_CMD_MOVE_XY:
		response_json = VcpCommExec_HandleMoveXy(request);
		break;
	case VCP_COMM_EXEC_CMD_TRACK_SWITCH:
		response_json = VcpCommExec_HandleTrackSwitch(request);
		break;
	case VCP_COMM_EXEC_CMD_BALL_LOCK:
		response_json = VcpCommExec_HandleBallLock(request);
		break;
	case VCP_COMM_EXEC_CMD_BRUSH_CONTROL:
		response_json = VcpCommExec_HandleBrushControl(request);
		break;
	case VCP_COMM_EXEC_CMD_QUERY_STATUS:
		response_json = VcpCommExec_HandleQueryStatus(request);
		break;
	case VCP_COMM_EXEC_CMD_QUERY_PRESSURE:
		response_json = VcpCommExec_HandleQueryPressure(request);
		break;
	default:
		s_exec_stats.unsupported_count++;
		return VCP_COMM_EXEC_ERR_UNSUPPORTED;
	}

	s_exec_stats.dispatch_ok_count++;

	if (response_json == NULL)
	{
		return VCP_COMM_EXEC_OK;
	}

	tx_status = VcpCommTx_SendJsonString(response_json);
	cJSON_free(response_json);

	if (tx_status == VCP_COMM_TX_OK)
	{
		s_exec_stats.tx_ok_count++;
		return VCP_COMM_EXEC_OK;
	}

	s_exec_stats.tx_fail_count++;
	return VCP_COMM_EXEC_ERR_TX;
}

void VcpCommExec_Init(void)
{
	memset(&s_exec_stats, 0, sizeof(s_exec_stats));
	VcpCommRx_Init();
}

void VcpCommExec_Task(void)
{
	VcpCommRxFrame frame;

	while (VcpCommRx_HasFrame())
	{
		cJSON *root = NULL;
		VcpCommExecRequest request;
		VcpCommExecStatus exec_status = VCP_COMM_EXEC_OK;

		if (VcpCommRx_GetFrame(&frame) != VCP_COMM_RX_OK)
		{
			s_exec_stats.dispatch_fail_count++;
			return;
		}

		s_exec_stats.rx_frame_count++;
		exec_status = VcpCommExec_ParseRequest(frame.payload, frame.payload_len, &root, &request);
		if (exec_status != VCP_COMM_EXEC_OK)
		{
			s_exec_stats.parse_fail_count++;
			continue;
		}

		s_exec_stats.parse_ok_count++;
		exec_status = VcpCommExec_DispatchCommand(&request);
		if (exec_status != VCP_COMM_EXEC_OK)
		{
			s_exec_stats.dispatch_fail_count++;
		}

		if (root != NULL)
		{
			cJSON_Delete(root);
		}
	}
}

void VcpCommExec_GetStats(VcpCommExecStats *stats)
{
	if (stats == NULL)
	{
		return;
	}

	memcpy(stats, &s_exec_stats, sizeof(*stats));
}
