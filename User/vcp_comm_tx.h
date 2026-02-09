#pragma once

#include "vcp_comm_rx.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	VCP_COMM_TX_OK = 0,
	VCP_COMM_TX_ERR_PARAM,
	VCP_COMM_TX_ERR_LENGTH,
	VCP_COMM_TX_ERR_FORMAT,
	VCP_COMM_TX_ERR_BUSY,
	VCP_COMM_TX_ERR_USB
} VcpCommTxStatus;

VcpCommTxStatus VcpCommTx_SendFrame(const uint8_t *json, uint16_t json_len);
VcpCommTxStatus VcpCommTx_SendJsonString(const char *json);

VcpCommTxStatus VcpCommTx_SendAck(int32_t ack_msg_type_id,
																	int32_t msg_seq,
																	const char *msg_name,
																	int32_t code,
																	const char *message,
																	int64_t timestamp_ms);

VcpCommTxStatus VcpCommTx_SendReport(int32_t report_msg_type_id,
																	 const char *msg_name,
																	 const char *data_json_object,
																	 int64_t timestamp_ms);

#ifdef __cplusplus
}
#endif
