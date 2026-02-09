#pragma once

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 协议格式: [JSON字节流][CRC16(2字节, 小端)][\n] */
#define VCP_COMM_FRAME_MAX_LEN      512u
#define VCP_COMM_JSON_MAX_LEN       (VCP_COMM_FRAME_MAX_LEN - 3u)
#define VCP_COMM_FRAME_END_BYTE     ((uint8_t)'\n')
#define VCP_COMM_CRC_FIELD_LEN      2u
#define VCP_COMM_RX_QUEUE_DEPTH     8u

typedef enum
{
	VCP_COMM_RX_OK = 0,
	VCP_COMM_RX_ERR_PARAM,
	VCP_COMM_RX_ERR_EMPTY,
	VCP_COMM_RX_ERR_OVERFLOW,
	VCP_COMM_RX_ERR_FORMAT,
	VCP_COMM_RX_ERR_CRC
} VcpCommRxStatus;

typedef struct
{
	uint8_t payload[VCP_COMM_JSON_MAX_LEN];
	uint16_t payload_len;
	uint16_t crc_recv;
	uint16_t crc_calc;
} VcpCommRxFrame;

typedef struct
{
	uint8_t in_progress;
	uint8_t frame_ready;
	uint16_t index;
	uint32_t total_frames;
	uint32_t crc_error_count;
	uint32_t overflow_count;
	uint32_t format_error_count;
} VcpCommRxStats;

void VcpCommRx_Init(void);
void VcpCommRx_Reset(void);

VcpCommRxStatus VcpCommRx_Feed(const uint8_t *data, uint16_t len);
bool VcpCommRx_HasFrame(void);
VcpCommRxStatus VcpCommRx_GetFrame(VcpCommRxFrame *frame);

uint16_t VcpCommRx_Crc16(const uint8_t *data, uint16_t len);
void VcpCommRx_GetStats(VcpCommRxStats *stats);

#ifdef __cplusplus
}
#endif
