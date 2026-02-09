#include "vcp_comm_tx.h"

#include "usbd_cdc_if.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_tx_frame[VCP_COMM_FRAME_MAX_LEN];

static VcpCommTxStatus VcpCommTx_SendPacket(const char *json, uint16_t json_len)
{
	uint16_t crc = 0u;
	uint8_t usb_ret = USBD_OK;

	if ((json == NULL) || (json_len == 0u))
	{
		return VCP_COMM_TX_ERR_PARAM;
	}

	if (json_len > VCP_COMM_JSON_MAX_LEN)
	{
		return VCP_COMM_TX_ERR_LENGTH;
	}

	memcpy(s_tx_frame, json, json_len);
	crc = VcpCommRx_Crc16(s_tx_frame, json_len);
	s_tx_frame[json_len] = (uint8_t)(crc & 0x00FFu);
	s_tx_frame[json_len + 1u] = (uint8_t)((crc >> 8u) & 0x00FFu);
	s_tx_frame[json_len + 2u] = VCP_COMM_FRAME_END_BYTE;

	usb_ret = CDC_Transmit_FS(s_tx_frame, (uint16_t)(json_len + 3u));
	if (usb_ret == USBD_BUSY)
	{
		return VCP_COMM_TX_ERR_BUSY;
	}
	if (usb_ret != USBD_OK)
	{
		return VCP_COMM_TX_ERR_USB;
	}

	return VCP_COMM_TX_OK;
}

VcpCommTxStatus VcpCommTx_SendFrame(const uint8_t *json, uint16_t json_len)
{
	if (json == NULL)
	{
		return VCP_COMM_TX_ERR_PARAM;
	}
	return VcpCommTx_SendPacket((const char *)json, json_len);
}

VcpCommTxStatus VcpCommTx_SendJsonString(const char *json)
{
	size_t len = 0u;

	if (json == NULL)
	{
		return VCP_COMM_TX_ERR_PARAM;
	}

	len = strlen(json);
	if (len > 0xFFFFu)
	{
		return VCP_COMM_TX_ERR_LENGTH;
	}

	return VcpCommTx_SendPacket(json, (uint16_t)len);
}

static bool VcpCommTx_EscapeString(const char *src, char *dst, uint16_t dst_len)
{
	uint16_t wr = 0u;
	uint16_t i = 0u;

	if ((src == NULL) || (dst == NULL) || (dst_len == 0u))
	{
		return false;
	}

	for (i = 0u; src[i] != '\0'; i++)
	{
		char ch = src[i];
		const char *rep = NULL;

		switch (ch)
		{
		case '\"':
			rep = "\\\"";
			break;
		case '\\':
			rep = "\\\\";
			break;
		case '\r':
			rep = "\\r";
			break;
		case '\n':
			rep = "\\n";
			break;
		case '\t':
			rep = "\\t";
			break;
		default:
			rep = NULL;
			break;
		}

		if (rep != NULL)
		{
			uint16_t j = 0u;
			for (j = 0u; rep[j] != '\0'; j++)
			{
				if ((wr + 1u) >= dst_len)
				{
					return false;
				}
				dst[wr++] = rep[j];
			}
		}
		else
		{
			if (((uint8_t)ch) < 0x20u)
			{
				return false;
			}
			if ((wr + 1u) >= dst_len)
			{
				return false;
			}
			dst[wr++] = ch;
		}
	}

	dst[wr] = '\0';
	return true;
}

VcpCommTxStatus VcpCommTx_SendAck(int32_t ack_msg_type_id,
																	int32_t msg_seq,
																	const char *msg_name,
																	int32_t code,
																	const char *message,
																	int64_t timestamp_ms)
{
	char escaped_name[48];
	char escaped_msg[96];
	char json[VCP_COMM_JSON_MAX_LEN + 1u];
	const char *name_src = (msg_name != NULL) ? msg_name : "ack";
	const char *message_src = (message != NULL) ? message : "";
	int len = 0;

	if (timestamp_ms < 0)
	{
		timestamp_ms = (int64_t)HAL_GetTick();
	}

	if (!VcpCommTx_EscapeString(name_src, escaped_name, sizeof(escaped_name)))
	{
		return VCP_COMM_TX_ERR_FORMAT;
	}
	if (!VcpCommTx_EscapeString(message_src, escaped_msg, sizeof(escaped_msg)))
	{
		return VCP_COMM_TX_ERR_FORMAT;
	}

	len = snprintf(json,
								 sizeof(json),
								 "{\"msg_type_id\":%ld,\"msg_seq\":%ld,\"msg_type\":\"ack\",\"msg_name\":\"%s\","
								 "\"timestamp\":%lld,\"data\":{\"code\":%ld,\"message\":\"%s\"}}",
								 (long)ack_msg_type_id,
								 (long)msg_seq,
								 escaped_name,
								 (long long)timestamp_ms,
								 (long)code,
								 escaped_msg);
	if ((len <= 0) || ((uint16_t)len > VCP_COMM_JSON_MAX_LEN))
	{
		return VCP_COMM_TX_ERR_LENGTH;
	}

	return VcpCommTx_SendPacket(json, (uint16_t)len);
}

VcpCommTxStatus VcpCommTx_SendReport(int32_t report_msg_type_id,
																	 const char *msg_name,
																	 const char *data_json_object,
																	 int64_t timestamp_ms)
{
	char escaped_name[48];
	char json[VCP_COMM_JSON_MAX_LEN + 1u];
	const char *name_src = (msg_name != NULL) ? msg_name : "report";
	const char *data_src = (data_json_object != NULL) ? data_json_object : "{}";
	int len = 0;

	if (timestamp_ms < 0)
	{
		timestamp_ms = (int64_t)HAL_GetTick();
	}

	if (!VcpCommTx_EscapeString(name_src, escaped_name, sizeof(escaped_name)))
	{
		return VCP_COMM_TX_ERR_FORMAT;
	}

	len = snprintf(json,
								 sizeof(json),
								 "{\"msg_type_id\":%ld,\"msg_seq\":0,\"msg_type\":\"report\",\"msg_name\":\"%s\","
								 "\"timestamp\":%lld,\"data\":%s}",
								 (long)report_msg_type_id,
								 escaped_name,
								 (long long)timestamp_ms,
								 data_src);
	if ((len <= 0) || ((uint16_t)len > VCP_COMM_JSON_MAX_LEN))
	{
		return VCP_COMM_TX_ERR_LENGTH;
	}

	return VcpCommTx_SendPacket(json, (uint16_t)len);
}
