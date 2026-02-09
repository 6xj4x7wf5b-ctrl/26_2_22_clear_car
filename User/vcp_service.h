#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint32_t rx_frames;
	uint32_t parse_ok_count;
	uint32_t parse_fail_count;
	uint32_t exec_ok_count;
	uint32_t exec_fail_count;
	uint32_t tx_ok_count;
	uint32_t tx_busy_count;
	uint32_t tx_fail_count;
	uint32_t tx_drop_count;
} VcpServiceStats;

void VcpService_Init(void);
void VcpService_Task(void);
void VcpService_GetStats(VcpServiceStats *stats);

#ifdef __cplusplus
}
#endif
