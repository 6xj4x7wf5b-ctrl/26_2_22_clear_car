#include "vcp_service.h"

#include "motor.h"
#include "sensor_switch.h"
#include "vcp_comm_parse.h"
#include "vcp_comm_rx.h"
#include "vcp_comm_tx.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define VCP_MSG_CMD_MOVE_LR        0x01
#define VCP_MSG_CMD_MOVE_UD        0x02
#define VCP_MSG_CMD_MOVE_XY        0x03
#define VCP_MSG_CMD_TRACK_SWITCH   0x04
#define VCP_MSG_CMD_BALL_LOCK      0x05
#define VCP_MSG_CMD_BRUSH_CONTROL  0x06
#define VCP_MSG_CMD_QUERY_STATUS   0x07
#define VCP_MSG_CMD_QUERY_PRESSURE 0x08

#define VCP_MSG_ACK_MOVE_LR        0x10
#define VCP_MSG_ACK_MOVE_UD        0x11
#define VCP_MSG_ACK_MOVE_XY        0x12
#define VCP_MSG_ACK_TRACK_SWITCH   0x13
#define VCP_MSG_ACK_BALL_LOCK      0x14
#define VCP_MSG_ACK_BRUSH_CONTROL  0x15
#define VCP_MSG_ACK_QUERY_STATUS   0x16
#define VCP_MSG_ACK_QUERY_PRESSURE 0x17
#define VCP_MSG_ACK_GENERIC        0x1F

#define VCP_MSG_REPORT_PRESSURE      0x20
#define VCP_MSG_REPORT_SAFETY_EDGE   0x21
#define VCP_MSG_REPORT_DEVICE_STATUS 0x22

#define VCP_SERVICE_TX_QUEUE_SIZE     8u
#define VCP_SERVICE_MSG_NAME_MAX_LEN  32u
#define VCP_SERVICE_MSG_TEXT_MAX_LEN  96u
#define VCP_SERVICE_REPORT_DATA_MAX   224u
#define VCP_SERVICE_DEFAULT_DUTY      0.70f

#define VCP_SERVICE_CODE_OK              0
#define VCP_SERVICE_CODE_PARSE_FAILED    (-1001)
#define VCP_SERVICE_CODE_UNSUPPORTED_CMD (-1002)
#define VCP_SERVICE_CODE_INVALID_PARAM   (-1003)
#define VCP_SERVICE_CODE_EXEC_FAILED     (-1004)

typedef enum
{
	VCP_SERVICE_CMD_NONE = 0,
	VCP_SERVICE_CMD_MOVE_LR,
	VCP_SERVICE_CMD_MOVE_UD,
	VCP_SERVICE_CMD_MOVE_XY,
	VCP_SERVICE_CMD_TRACK_SWITCH,
	VCP_SERVICE_CMD_BALL_LOCK,
	VCP_SERVICE_CMD_BRUSH_CONTROL,
	VCP_SERVICE_CMD_QUERY_STATUS,
	VCP_SERVICE_CMD_QUERY_PRESSURE
} VcpServiceCmdKind;

typedef enum
{
	VCP_AXIS_STOP = 0,
	VCP_AXIS_NEGATIVE,
	VCP_AXIS_POSITIVE
} VcpAxisDirection;

typedef enum
{
	VCP_TRACK_MODE_STOP = 0,
	VCP_TRACK_MODE_CRAWLER,
	VCP_TRACK_MODE_WHEEL
} VcpTrackMode;

typedef enum
{
	VCP_BALL_ACTION_STOP = 0,
	VCP_BALL_ACTION_LOCK,
	VCP_BALL_ACTION_UNLOCK
} VcpBallAction;

typedef enum
{
	VCP_BRUSH_ACTION_STOP = 0,
	VCP_BRUSH_ACTION_START
} VcpBrushAction;

typedef struct
{
	VcpServiceCmdKind kind;
	int32_t msg_type_id;
	int32_t msg_seq;
	int64_t timestamp;
	char msg_name[VCP_SERVICE_MSG_NAME_MAX_LEN];
	uint8_t has_msg_name;
	union
	{
		struct
		{
			VcpAxisDirection dir;
			float speed;
		} move_lr;
		struct
		{
			VcpAxisDirection dir;
			float speed;
		} move_ud;
		struct
		{
			VcpAxisDirection lr_dir;
			float lr_speed;
			VcpAxisDirection ud_dir;
			float ud_speed;
		} move_xy;
		struct
		{
			VcpTrackMode mode;
			float duty;
		} track_switch;
		struct
		{
			VcpBallAction action;
			float duty;
		} ball_lock;
		struct
		{
			VcpBrushAction action;
			float speed;
		} brush_control;
	} param;
} VcpServiceCommand;

typedef enum
{
	VCP_SERVICE_TX_EVENT_ACK = 0,
	VCP_SERVICE_TX_EVENT_REPORT
} VcpServiceTxEventType;

typedef struct
{
	VcpServiceTxEventType type;
	int32_t msg_type_id;
	int32_t msg_seq;
	int64_t timestamp;
	char msg_name[VCP_SERVICE_MSG_NAME_MAX_LEN];
	int32_t code;
	char text[VCP_SERVICE_MSG_TEXT_MAX_LEN];
	char data_json[VCP_SERVICE_REPORT_DATA_MAX];
} VcpServiceTxEvent;

typedef struct
{
	VcpServiceTxEvent events[VCP_SERVICE_TX_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} VcpServiceTxQueue;

typedef struct
{
	VcpCommRxFrame pending_frame;
	VcpCommParsedMessage parsed_msg;
	uint8_t has_pending_frame;
	VcpServiceCommand pending_command;
	uint8_t has_pending_command;
	VcpServiceTxQueue tx_queue;
	VcpServiceStats stats;
} VcpServiceContext;

static VcpServiceContext s_service;

static bool VcpService_Streq(const char *a, const char *b)
{
	if ((a == NULL) || (b == NULL))
	{
		return false;
	}
	return (strcmp(a, b) == 0);
}

static void VcpService_CopyString(char *dst, uint16_t dst_len, const char *src)
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

static float VcpService_Clamp01(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	if (value > 1.0f)
	{
		return 1.0f;
	}
	return value;
}

static bool VcpService_TxQueuePush(const VcpServiceTxEvent *event)
{
	VcpServiceTxQueue *q = &s_service.tx_queue;
	if (event == NULL)
	{
		return false;
	}
	if (q->count >= VCP_SERVICE_TX_QUEUE_SIZE)
	{
		return false;
	}

	memcpy(&q->events[q->tail], event, sizeof(*event));
	q->tail = (uint8_t)((q->tail + 1u) % VCP_SERVICE_TX_QUEUE_SIZE);
	q->count++;
	return true;
}

static bool VcpService_TxQueueFront(VcpServiceTxEvent *event)
{
	VcpServiceTxQueue *q = &s_service.tx_queue;
	if ((q->count == 0u) || (event == NULL))
	{
		return false;
	}
	memcpy(event, &q->events[q->head], sizeof(*event));
	return true;
}

static void VcpService_TxQueuePop(void)
{
	VcpServiceTxQueue *q = &s_service.tx_queue;
	if (q->count == 0u)
	{
		return;
	}
	q->head = (uint8_t)((q->head + 1u) % VCP_SERVICE_TX_QUEUE_SIZE);
	q->count--;
}

static int32_t VcpService_AckTypeId(int32_t cmd_msg_type_id)
{
	switch (cmd_msg_type_id)
	{
	case VCP_MSG_CMD_MOVE_LR:
		return VCP_MSG_ACK_MOVE_LR;
	case VCP_MSG_CMD_MOVE_UD:
		return VCP_MSG_ACK_MOVE_UD;
	case VCP_MSG_CMD_MOVE_XY:
		return VCP_MSG_ACK_MOVE_XY;
	case VCP_MSG_CMD_TRACK_SWITCH:
		return VCP_MSG_ACK_TRACK_SWITCH;
	case VCP_MSG_CMD_BALL_LOCK:
		return VCP_MSG_ACK_BALL_LOCK;
	case VCP_MSG_CMD_BRUSH_CONTROL:
		return VCP_MSG_ACK_BRUSH_CONTROL;
	case VCP_MSG_CMD_QUERY_STATUS:
		return VCP_MSG_ACK_QUERY_STATUS;
	case VCP_MSG_CMD_QUERY_PRESSURE:
		return VCP_MSG_ACK_QUERY_PRESSURE;
	default:
		return VCP_MSG_ACK_GENERIC;
	}
}

static const char *VcpService_AckName(int32_t cmd_msg_type_id)
{
	switch (cmd_msg_type_id)
	{
	case VCP_MSG_CMD_MOVE_LR:
		return "ack_move_lr";
	case VCP_MSG_CMD_MOVE_UD:
		return "ack_move_ud";
	case VCP_MSG_CMD_MOVE_XY:
		return "ack_move_xy";
	case VCP_MSG_CMD_TRACK_SWITCH:
		return "ack_track_switch";
	case VCP_MSG_CMD_BALL_LOCK:
		return "ack_ball_lock";
	case VCP_MSG_CMD_BRUSH_CONTROL:
		return "ack_brush_control";
	case VCP_MSG_CMD_QUERY_STATUS:
		return "ack_query_status";
	case VCP_MSG_CMD_QUERY_PRESSURE:
		return "ack_query_pressure";
	default:
		return "ack_generic";
	}
}

static bool VcpService_EnqueueAck(const VcpServiceCommand *cmd, int32_t code, const char *text)
{
	VcpServiceTxEvent event;
	memset(&event, 0, sizeof(event));

	if (cmd == NULL)
	{
		return false;
	}

	event.type = VCP_SERVICE_TX_EVENT_ACK;
	event.msg_type_id = VcpService_AckTypeId(cmd->msg_type_id);
	event.msg_seq = cmd->msg_seq;
	event.timestamp = (int64_t)HAL_GetTick();
	event.code = code;
	VcpService_CopyString(event.msg_name, sizeof(event.msg_name), VcpService_AckName(cmd->msg_type_id));
	VcpService_CopyString(event.text, sizeof(event.text), text);
	return VcpService_TxQueuePush(&event);
}

static bool VcpService_EnqueueReport(int32_t report_msg_type_id,
																		 const char *report_name,
																		 const char *data_json)
{
	VcpServiceTxEvent event;
	memset(&event, 0, sizeof(event));
	event.type = VCP_SERVICE_TX_EVENT_REPORT;
	event.msg_type_id = report_msg_type_id;
	event.msg_seq = 0;
	event.timestamp = (int64_t)HAL_GetTick();
	VcpService_CopyString(event.msg_name, sizeof(event.msg_name), report_name);
	VcpService_CopyString(event.data_json, sizeof(event.data_json), data_json);
	return VcpService_TxQueuePush(&event);
}

static bool VcpService_ParseAxis(const char *value,
																 const char *positive_word,
																 const char *negative_word,
																 VcpAxisDirection *out_dir)
{
	if ((value == NULL) || (positive_word == NULL) || (negative_word == NULL) || (out_dir == NULL))
	{
		return false;
	}

	if (VcpService_Streq(value, "stop"))
	{
		*out_dir = VCP_AXIS_STOP;
		return true;
	}
	if (VcpService_Streq(value, positive_word))
	{
		*out_dir = VCP_AXIS_POSITIVE;
		return true;
	}
	if (VcpService_Streq(value, negative_word))
	{
		*out_dir = VCP_AXIS_NEGATIVE;
		return true;
	}
	return false;
}

static bool VcpService_ReadFloatField(const VcpCommParsedMessage *msg,
																			const char *key,
																			float *out_value,
																			uint8_t required)
{
	int token = -1;
	float value = 0.0f;

	if ((msg == NULL) || (key == NULL) || (out_value == NULL))
	{
		return false;
	}

	token = VcpCommParse_FindKey(msg, msg->message.data_token, key);
	if (token < 0)
	{
		return (required == 0u);
	}
	if (!VcpCommParse_GetFloat(msg, token, &value))
	{
		return false;
	}
	*out_value = value;
	return true;
}

static bool VcpService_DecodeMoveLr(const VcpCommParsedMessage *msg,
																		VcpServiceCommand *cmd)
{
	char direction[12];
	float speed = 0.0f;

	if (!VcpCommParse_DataGetString(msg, "direction", direction, sizeof(direction)))
	{
		return false;
	}
	if (!VcpService_ParseAxis(direction, "left", "right", &cmd->param.move_lr.dir))
	{
		return false;
	}
	if (!VcpService_ReadFloatField(msg, "speed", &speed, (cmd->param.move_lr.dir == VCP_AXIS_STOP) ? 0u : 1u))
	{
		return false;
	}
	cmd->param.move_lr.speed = VcpService_Clamp01(speed);
	return true;
}

static bool VcpService_DecodeMoveUd(const VcpCommParsedMessage *msg,
																		VcpServiceCommand *cmd)
{
	char direction[12];
	float speed = 0.0f;

	if (!VcpCommParse_DataGetString(msg, "direction", direction, sizeof(direction)))
	{
		return false;
	}
	if (!VcpService_ParseAxis(direction, "up", "down", &cmd->param.move_ud.dir))
	{
		return false;
	}
	if (!VcpService_ReadFloatField(msg, "speed", &speed, (cmd->param.move_ud.dir == VCP_AXIS_STOP) ? 0u : 1u))
	{
		return false;
	}
	cmd->param.move_ud.speed = VcpService_Clamp01(speed);
	return true;
}

static bool VcpService_DecodeMoveXy(const VcpCommParsedMessage *msg,
																		VcpServiceCommand *cmd)
{
	char lr_direction[12];
	char ud_direction[12];
	float lr_speed = 0.0f;
	float ud_speed = 0.0f;

	if (!VcpCommParse_DataGetString(msg, "lr_direction", lr_direction, sizeof(lr_direction)))
	{
		return false;
	}
	if (!VcpCommParse_DataGetString(msg, "ud_direction", ud_direction, sizeof(ud_direction)))
	{
		return false;
	}
	if (!VcpService_ParseAxis(lr_direction, "left", "right", &cmd->param.move_xy.lr_dir))
	{
		return false;
	}
	if (!VcpService_ParseAxis(ud_direction, "up", "down", &cmd->param.move_xy.ud_dir))
	{
		return false;
	}

	if (!VcpService_ReadFloatField(msg, "lr_speed", &lr_speed, (cmd->param.move_xy.lr_dir == VCP_AXIS_STOP) ? 0u : 1u))
	{
		return false;
	}
	if (!VcpService_ReadFloatField(msg, "ud_speed", &ud_speed, (cmd->param.move_xy.ud_dir == VCP_AXIS_STOP) ? 0u : 1u))
	{
		return false;
	}

	cmd->param.move_xy.lr_speed = VcpService_Clamp01(lr_speed);
	cmd->param.move_xy.ud_speed = VcpService_Clamp01(ud_speed);
	return true;
}

static bool VcpService_DecodeTrackSwitch(const VcpCommParsedMessage *msg,
																				 VcpServiceCommand *cmd)
{
	char mode[16];
	float duty = VCP_SERVICE_DEFAULT_DUTY;

	if (!VcpCommParse_DataGetString(msg, "track_mode", mode, sizeof(mode)))
	{
		return false;
	}
	if (VcpService_Streq(mode, "stop"))
	{
		cmd->param.track_switch.mode = VCP_TRACK_MODE_STOP;
	}
	else if (VcpService_Streq(mode, "crawler"))
	{
		cmd->param.track_switch.mode = VCP_TRACK_MODE_CRAWLER;
	}
	else if (VcpService_Streq(mode, "wheel"))
	{
		cmd->param.track_switch.mode = VCP_TRACK_MODE_WHEEL;
	}
	else
	{
		return false;
	}

	(void)VcpService_ReadFloatField(msg, "duty", &duty, 0u);
	cmd->param.track_switch.duty = VcpService_Clamp01(duty);
	return true;
}

static bool VcpService_DecodeBallLock(const VcpCommParsedMessage *msg,
																			VcpServiceCommand *cmd)
{
	char action[16];
	float duty = VCP_SERVICE_DEFAULT_DUTY;

	if (!VcpCommParse_DataGetString(msg, "action", action, sizeof(action)))
	{
		return false;
	}
	if (VcpService_Streq(action, "stop"))
	{
		cmd->param.ball_lock.action = VCP_BALL_ACTION_STOP;
	}
	else if (VcpService_Streq(action, "lock"))
	{
		cmd->param.ball_lock.action = VCP_BALL_ACTION_LOCK;
	}
	else if (VcpService_Streq(action, "unlock"))
	{
		cmd->param.ball_lock.action = VCP_BALL_ACTION_UNLOCK;
	}
	else
	{
		return false;
	}

	(void)VcpService_ReadFloatField(msg, "duty", &duty, 0u);
	cmd->param.ball_lock.duty = VcpService_Clamp01(duty);
	return true;
}

static bool VcpService_DecodeBrushControl(const VcpCommParsedMessage *msg,
																					VcpServiceCommand *cmd)
{
	char action[16];
	float speed = 0.0f;

	if (!VcpCommParse_DataGetString(msg, "action", action, sizeof(action)))
	{
		return false;
	}
	if (VcpService_Streq(action, "stop"))
	{
		cmd->param.brush_control.action = VCP_BRUSH_ACTION_STOP;
	}
	else if (VcpService_Streq(action, "start"))
	{
		cmd->param.brush_control.action = VCP_BRUSH_ACTION_START;
	}
	else
	{
		return false;
	}

	if (!VcpService_ReadFloatField(msg, "speed", &speed, (cmd->param.brush_control.action == VCP_BRUSH_ACTION_START) ? 1u : 0u))
	{
		return false;
	}
	cmd->param.brush_control.speed = VcpService_Clamp01(speed);
	return true;
}

static bool VcpService_DecodeCommand(const VcpCommParsedMessage *msg,
																		 VcpServiceCommand *cmd,
																		 int32_t *err_code,
																		 const char **err_text)
{
	if ((msg == NULL) || (cmd == NULL) || (err_code == NULL) || (err_text == NULL))
	{
		return false;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->msg_type_id = msg->message.msg_type_id;
	cmd->msg_seq = msg->message.msg_seq;
	cmd->timestamp = msg->message.timestamp;
	cmd->has_msg_name = msg->message.has_msg_name;
	if (msg->message.has_msg_name != 0u)
	{
		VcpService_CopyString(cmd->msg_name, sizeof(cmd->msg_name), msg->message.msg_name);
	}

	if (msg->message.msg_type_enum != VCP_COMM_MSG_TYPE_CMD)
	{
		*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
		*err_text = "msg_type must be cmd";
		return false;
	}

	switch (msg->message.msg_type_id)
	{
	case VCP_MSG_CMD_MOVE_LR:
		cmd->kind = VCP_SERVICE_CMD_MOVE_LR;
		if (!VcpService_DecodeMoveLr(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid move_lr payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_MOVE_UD:
		cmd->kind = VCP_SERVICE_CMD_MOVE_UD;
		if (!VcpService_DecodeMoveUd(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid move_ud payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_MOVE_XY:
		cmd->kind = VCP_SERVICE_CMD_MOVE_XY;
		if (!VcpService_DecodeMoveXy(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid move_xy payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_TRACK_SWITCH:
		cmd->kind = VCP_SERVICE_CMD_TRACK_SWITCH;
		if (!VcpService_DecodeTrackSwitch(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid track_switch payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_BALL_LOCK:
		cmd->kind = VCP_SERVICE_CMD_BALL_LOCK;
		if (!VcpService_DecodeBallLock(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid ball_lock payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_BRUSH_CONTROL:
		cmd->kind = VCP_SERVICE_CMD_BRUSH_CONTROL;
		if (!VcpService_DecodeBrushControl(msg, cmd))
		{
			*err_code = VCP_SERVICE_CODE_INVALID_PARAM;
			*err_text = "invalid brush_control payload";
			return false;
		}
		return true;

	case VCP_MSG_CMD_QUERY_STATUS:
		cmd->kind = VCP_SERVICE_CMD_QUERY_STATUS;
		return true;

	case VCP_MSG_CMD_QUERY_PRESSURE:
		cmd->kind = VCP_SERVICE_CMD_QUERY_PRESSURE;
		return true;

	default:
		*err_code = VCP_SERVICE_CODE_UNSUPPORTED_CMD;
		*err_text = "unsupported msg_type_id";
		return false;
	}
}

static bool VcpService_ExecMoveLr(const VcpServiceCommand *cmd)
{
	switch (cmd->param.move_lr.dir)
	{
	case VCP_AXIS_POSITIVE:
		return Motor_MoveLeft(cmd->param.move_lr.speed);
	case VCP_AXIS_NEGATIVE:
		return Motor_MoveRight(cmd->param.move_lr.speed);
	case VCP_AXIS_STOP:
		return Motor_MoveLeft(0.0f);
	default:
		return false;
	}
}

static bool VcpService_ExecMoveUd(const VcpServiceCommand *cmd)
{
	switch (cmd->param.move_ud.dir)
	{
	case VCP_AXIS_POSITIVE:
		return Motor_MoveUp(cmd->param.move_ud.speed);
	case VCP_AXIS_NEGATIVE:
		return Motor_MoveDown(cmd->param.move_ud.speed);
	case VCP_AXIS_STOP:
		return Motor_MoveUp(0.0f);
	default:
		return false;
	}
}

static bool VcpService_ExecMoveXy(const VcpServiceCommand *cmd)
{
	bool lr_ok = false;
	bool ud_ok = false;

	switch (cmd->param.move_xy.lr_dir)
	{
	case VCP_AXIS_POSITIVE:
		lr_ok = Motor_MoveLeft(cmd->param.move_xy.lr_speed);
		break;
	case VCP_AXIS_NEGATIVE:
		lr_ok = Motor_MoveRight(cmd->param.move_xy.lr_speed);
		break;
	case VCP_AXIS_STOP:
		lr_ok = Motor_MoveLeft(0.0f);
		break;
	default:
		lr_ok = false;
		break;
	}

	switch (cmd->param.move_xy.ud_dir)
	{
	case VCP_AXIS_POSITIVE:
		ud_ok = Motor_MoveUp(cmd->param.move_xy.ud_speed);
		break;
	case VCP_AXIS_NEGATIVE:
		ud_ok = Motor_MoveDown(cmd->param.move_xy.ud_speed);
		break;
	case VCP_AXIS_STOP:
		ud_ok = Motor_MoveUp(0.0f);
		break;
	default:
		ud_ok = false;
		break;
	}

	return (lr_ok && ud_ok);
}

static bool VcpService_ExecTrackSwitch(const VcpServiceCommand *cmd)
{
	switch (cmd->param.track_switch.mode)
	{
	case VCP_TRACK_MODE_CRAWLER:
		return Motor_CrawlerLiftDown(cmd->param.track_switch.duty);
	case VCP_TRACK_MODE_WHEEL:
		return Motor_CrawlerLiftUp(cmd->param.track_switch.duty);
	case VCP_TRACK_MODE_STOP:
		return Motor_CrawlerLiftStop();
	default:
		return false;
	}
}

static bool VcpService_ExecBallLock(const VcpServiceCommand *cmd)
{
	switch (cmd->param.ball_lock.action)
	{
	case VCP_BALL_ACTION_LOCK:
		return Motor_BallHeadLock(cmd->param.ball_lock.duty);
	case VCP_BALL_ACTION_UNLOCK:
		return Motor_BallHeadUnlock(cmd->param.ball_lock.duty);
	case VCP_BALL_ACTION_STOP:
		return Motor_BallHeadStop();
	default:
		return false;
	}
}

static bool VcpService_ExecBrushControl(const VcpServiceCommand *cmd)
{
	switch (cmd->param.brush_control.action)
	{
	case VCP_BRUSH_ACTION_START:
		return Motor_BrushStart(cmd->param.brush_control.speed);
	case VCP_BRUSH_ACTION_STOP:
		return Motor_BrushStop();
	default:
		return false;
	}
}

static bool VcpService_ExecAndReport(const VcpServiceCommand *cmd,
																		 int32_t *code,
																		 const char **text)
{
	char report_json[VCP_SERVICE_REPORT_DATA_MAX];
	bool ok = false;
	uint8_t safety = 0u;
	uint8_t proximity = 0u;
	VcpCommRxStats rx_stats;
	int written = 0;

	if ((cmd == NULL) || (code == NULL) || (text == NULL))
	{
		return false;
	}

	switch (cmd->kind)
	{
	case VCP_SERVICE_CMD_MOVE_LR:
		ok = VcpService_ExecMoveLr(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "move_lr ok" : "move_lr failed";
		return ok;

	case VCP_SERVICE_CMD_MOVE_UD:
		ok = VcpService_ExecMoveUd(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "move_ud ok" : "move_ud failed";
		return ok;

	case VCP_SERVICE_CMD_MOVE_XY:
		ok = VcpService_ExecMoveXy(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "move_xy ok" : "move_xy failed";
		return ok;

	case VCP_SERVICE_CMD_TRACK_SWITCH:
		ok = VcpService_ExecTrackSwitch(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "track_switch ok" : "track_switch failed";
		return ok;

	case VCP_SERVICE_CMD_BALL_LOCK:
		ok = VcpService_ExecBallLock(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "ball_lock ok" : "ball_lock failed";
		return ok;

	case VCP_SERVICE_CMD_BRUSH_CONTROL:
		ok = VcpService_ExecBrushControl(cmd);
		*code = ok ? VCP_SERVICE_CODE_OK : VCP_SERVICE_CODE_EXEC_FAILED;
		*text = ok ? "brush_control ok" : "brush_control failed";
		return ok;

	case VCP_SERVICE_CMD_QUERY_STATUS:
		VcpCommRx_GetStats(&rx_stats);
		safety = SafetyEdge_ReadAll();
		proximity = ProximitySwitch_ReadAll();
		written = snprintf(report_json,
											 sizeof(report_json),
											 "{\"safety_edge_bits\":%u,\"proximity_bits\":%u,"
											 "\"rx_total_frames\":%lu,\"rx_crc_errors\":%lu,"
											 "\"rx_overflow\":%lu,\"rx_format_errors\":%lu}",
											 (unsigned int)safety,
											 (unsigned int)proximity,
											 (unsigned long)rx_stats.total_frames,
											 (unsigned long)rx_stats.crc_error_count,
											 (unsigned long)rx_stats.overflow_count,
											 (unsigned long)rx_stats.format_error_count);
		if ((written > 0) && ((size_t)written < sizeof(report_json)))
		{
			if (!VcpService_EnqueueReport(VCP_MSG_REPORT_DEVICE_STATUS, "device_status", report_json))
			{
				s_service.stats.tx_drop_count++;
			}
			if (!VcpService_EnqueueReport(VCP_MSG_REPORT_SAFETY_EDGE, "safety_edge", "{\"event\":\"queried\"}"))
			{
				s_service.stats.tx_drop_count++;
			}
		}
		*code = VCP_SERVICE_CODE_OK;
		*text = "query_status ok";
		return true;

	case VCP_SERVICE_CMD_QUERY_PRESSURE:
		if (!VcpService_EnqueueReport(VCP_MSG_REPORT_PRESSURE, "pressure", "{\"pressure\":0,\"valid\":0}"))
		{
			s_service.stats.tx_drop_count++;
		}
		*code = VCP_SERVICE_CODE_OK;
		*text = "query_pressure ok";
		return true;

	default:
		*code = VCP_SERVICE_CODE_UNSUPPORTED_CMD;
		*text = "unsupported command";
		return false;
	}
}

static void VcpService_RxTask(void)
{
	if (s_service.has_pending_frame != 0u)
	{
		return;
	}
	if (!VcpCommRx_HasFrame())
	{
		return;
	}

	if (VcpCommRx_GetFrame(&s_service.pending_frame) == VCP_COMM_RX_OK)
	{
		s_service.has_pending_frame = 1u;
		s_service.stats.rx_frames++;
	}
}

static void VcpService_ParseTask(void)
{
	int32_t err_code = VCP_SERVICE_CODE_PARSE_FAILED;
	const char *err_text = "parse failed";

	if (s_service.has_pending_frame == 0u)
	{
		return;
	}
	if (s_service.has_pending_command != 0u)
	{
		return;
	}

	if (VcpCommParse_Message(s_service.pending_frame.payload,
														 s_service.pending_frame.payload_len,
														 &s_service.parsed_msg) != VCP_COMM_PARSE_OK)
	{
		s_service.stats.parse_fail_count++;
		s_service.has_pending_frame = 0u;
		return;
	}

	if (VcpService_DecodeCommand(&s_service.parsed_msg, &s_service.pending_command, &err_code, &err_text))
	{
		s_service.has_pending_command = 1u;
		s_service.stats.parse_ok_count++;
	}
	else
	{
		s_service.stats.parse_fail_count++;
		if (VcpService_EnqueueAck(&s_service.pending_command, err_code, err_text) == false)
		{
			s_service.stats.tx_drop_count++;
		}
	}

	s_service.has_pending_frame = 0u;
}

static void VcpService_ExecTask(void)
{
	int32_t code = VCP_SERVICE_CODE_EXEC_FAILED;
	const char *text = "exec failed";
	bool exec_ok = false;

	if (s_service.has_pending_command == 0u)
	{
		return;
	}

	exec_ok = VcpService_ExecAndReport(&s_service.pending_command, &code, &text);
	if (exec_ok)
	{
		s_service.stats.exec_ok_count++;
	}
	else
	{
		s_service.stats.exec_fail_count++;
	}

	if (!VcpService_EnqueueAck(&s_service.pending_command, code, text))
	{
		s_service.stats.tx_drop_count++;
	}

	s_service.has_pending_command = 0u;
}

static void VcpService_TxTask(void)
{
	VcpServiceTxEvent event;
	VcpCommTxStatus status = VCP_COMM_TX_ERR_PARAM;

	if (!VcpService_TxQueueFront(&event))
	{
		return;
	}

	if (event.type == VCP_SERVICE_TX_EVENT_ACK)
	{
		status = VcpCommTx_SendAck(event.msg_type_id,
														 event.msg_seq,
														 event.msg_name,
														 event.code,
														 event.text,
														 event.timestamp);
	}
	else
	{
		status = VcpCommTx_SendReport(event.msg_type_id,
																event.msg_name,
																event.data_json,
																event.timestamp);
	}

	if (status == VCP_COMM_TX_ERR_BUSY)
	{
		s_service.stats.tx_busy_count++;
		return;
	}

	VcpService_TxQueuePop();

	if (status == VCP_COMM_TX_OK)
	{
		s_service.stats.tx_ok_count++;
	}
	else
	{
		s_service.stats.tx_fail_count++;
	}
}

void VcpService_Init(void)
{
	memset(&s_service, 0, sizeof(s_service));
	VcpCommRx_Init();
}

void VcpService_Task(void)
{
	VcpService_RxTask();
	VcpService_ParseTask();
	VcpService_ExecTask();
	VcpService_TxTask();
}

void VcpService_GetStats(VcpServiceStats *stats)
{
	if (stats == NULL)
	{
		return;
	}
	memcpy(stats, &s_service.stats, sizeof(*stats));
}
