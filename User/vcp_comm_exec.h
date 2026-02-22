#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	VCP_COMM_EXEC_OK = 0,
	VCP_COMM_EXEC_ERR_PARAM,
	VCP_COMM_EXEC_ERR_RX,
	VCP_COMM_EXEC_ERR_JSON,
	VCP_COMM_EXEC_ERR_UNSUPPORTED,
	VCP_COMM_EXEC_ERR_TX
} VcpCommExecStatus;

typedef struct
{
	uint32_t rx_frame_count;
	uint32_t parse_ok_count;
	uint32_t parse_fail_count;
	uint32_t dispatch_ok_count;
	uint32_t dispatch_fail_count;
	uint32_t unsupported_count;
	uint32_t tx_ok_count;
	uint32_t tx_fail_count;
} VcpCommExecStats;

void VcpCommExec_Init(void);
void VcpCommExec_Task(void);
void VcpCommExec_GetStats(VcpCommExecStats *stats);

#ifdef __cplusplus
}
#endif
