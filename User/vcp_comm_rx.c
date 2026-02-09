#include "vcp_comm_rx.h"

#include <string.h>

typedef struct
{
	uint8_t buf[VCP_COMM_FRAME_MAX_LEN];
	uint16_t index;
	uint8_t drop_until_end;

	// 环形接收帧队列
	VcpCommRxFrame queue[VCP_COMM_RX_QUEUE_DEPTH];
	uint8_t queue_head;
	uint8_t queue_tail;
	uint8_t queue_count;

	VcpCommRxStats stats;
} VcpCommRxContext;

static VcpCommRxContext s_rx;


/*--------------------- Enter/Exit Critical Section -----------------------*/

static uint32_t VcpCommRx_EnterCritical(void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	return primask;
}

static void VcpCommRx_ExitCritical(uint32_t primask)
{
	if (primask == 0u)
	{
		__enable_irq();
	}
}


static void VcpCommRx_UpdateReadyFlag(void)
{
	s_rx.stats.frame_ready = (s_rx.queue_count > 0u) ? 1u : 0u;
}


uint16_t VcpCommRx_Crc16(const uint8_t *data, uint16_t len)
{
	uint16_t crc = 0xFFFFu;
	uint16_t i = 0u;

	if (data == NULL)
	{
		return 0u;
	}

	for (i = 0u; i < len; i++)
	{
		uint8_t bit = 0u;
		crc ^= data[i];
		for (bit = 0u; bit < 8u; bit++)
		{
			if ((crc & 0x0001u) != 0u)
			{
				crc = (uint16_t)((crc >> 1u) ^ 0xA001u);
			}
			else
			{
				crc >>= 1u;
			}
		}
	}

	return crc;
}

void VcpCommRx_Init(void)
{
	memset(&s_rx, 0, sizeof(s_rx));
}

void VcpCommRx_Reset(void)
{
	uint32_t primask = VcpCommRx_EnterCritical();

	s_rx.index = 0u;
	s_rx.drop_until_end = 0u;
	s_rx.queue_head = 0u;
	s_rx.queue_tail = 0u;
	s_rx.queue_count = 0u;
	s_rx.stats.in_progress = 0u;
	s_rx.stats.index = 0u;
	VcpCommRx_UpdateReadyFlag();

	VcpCommRx_ExitCritical(primask);
}

static VcpCommRxStatus VcpCommRx_QueuePush(const uint8_t *payload,
																 uint16_t payload_len,
																 uint16_t crc_recv,
																 uint16_t crc_calc)
{
	VcpCommRxFrame *slot = NULL;
	uint32_t primask = VcpCommRx_EnterCritical();

	if (s_rx.queue_count >= VCP_COMM_RX_QUEUE_DEPTH)
	{
		s_rx.stats.overflow_count++;
		VcpCommRx_ExitCritical(primask);
		return VCP_COMM_RX_ERR_OVERFLOW;
	}

	slot = &s_rx.queue[s_rx.queue_tail];
	memcpy(slot->payload, payload, payload_len);
	slot->payload_len = payload_len;
	slot->crc_recv = crc_recv;
	slot->crc_calc = crc_calc;

	s_rx.queue_tail++;
	if (s_rx.queue_tail >= VCP_COMM_RX_QUEUE_DEPTH)
	{
		s_rx.queue_tail = 0u;
	}
	s_rx.queue_count++;
	s_rx.stats.total_frames++;
	VcpCommRx_UpdateReadyFlag();

	VcpCommRx_ExitCritical(primask);
	return VCP_COMM_RX_OK;
}

static VcpCommRxStatus VcpCommRx_FinalizeFrame(void)
{
	uint16_t payload_len = 0u;
	uint16_t crc_recv = 0u;
	uint16_t crc_calc = 0u;

	if (s_rx.index < (VCP_COMM_CRC_FIELD_LEN + 1u))
	{
		s_rx.stats.format_error_count++;
		return VCP_COMM_RX_ERR_FORMAT;
	}

	payload_len = (uint16_t)(s_rx.index - VCP_COMM_CRC_FIELD_LEN - 1u);
	crc_recv = (uint16_t)s_rx.buf[payload_len];
	crc_recv |= (uint16_t)((uint16_t)s_rx.buf[payload_len + 1u] << 8u);
	crc_calc = VcpCommRx_Crc16(s_rx.buf, payload_len);

	if (crc_recv != crc_calc)
	{
		s_rx.stats.crc_error_count++;
		return VCP_COMM_RX_ERR_CRC;
	}

	return VcpCommRx_QueuePush(s_rx.buf, payload_len, crc_recv, crc_calc);
}

VcpCommRxStatus VcpCommRx_Feed(const uint8_t *data, uint16_t len)
{
	uint16_t i = 0u;
	VcpCommRxStatus ret = VCP_COMM_RX_OK;

	if ((data == NULL) && (len > 0u))
	{
		return VCP_COMM_RX_ERR_PARAM;
	}

	for (i = 0u; i < len; i++)
	{
		uint8_t ch = data[i];

		if (s_rx.drop_until_end != 0u)
		{
			if (ch == VCP_COMM_FRAME_END_BYTE)
			{
				s_rx.drop_until_end = 0u;
			}
			continue;
		}

		if (s_rx.index >= VCP_COMM_FRAME_MAX_LEN)
		{
			s_rx.stats.overflow_count++;
			s_rx.drop_until_end = 1u;
			s_rx.index = 0u;
			s_rx.stats.in_progress = 0u;
			s_rx.stats.index = 0u;
			if (ret == VCP_COMM_RX_OK)
			{
				ret = VCP_COMM_RX_ERR_OVERFLOW;
			}
			continue;
		}

		s_rx.buf[s_rx.index] = ch;
		s_rx.index++;
		s_rx.stats.in_progress = 1u;
		s_rx.stats.index = s_rx.index;

		if (ch == VCP_COMM_FRAME_END_BYTE)
		{
			VcpCommRxStatus frame_ret = VcpCommRx_FinalizeFrame();
			s_rx.index = 0u;
			s_rx.stats.in_progress = 0u;
			s_rx.stats.index = 0u;

			if ((frame_ret != VCP_COMM_RX_OK) && (ret == VCP_COMM_RX_OK))
			{
				ret = frame_ret;
			}
		}
	}

	return ret;
}

bool VcpCommRx_HasFrame(void)
{
	bool has_frame = false;
	uint32_t primask = VcpCommRx_EnterCritical();
	has_frame = (s_rx.queue_count > 0u);
	VcpCommRx_ExitCritical(primask);
	return has_frame;
}

VcpCommRxStatus VcpCommRx_GetFrame(VcpCommRxFrame *frame)
{
	uint32_t primask = 0u;

	if (frame == NULL)
	{
		return VCP_COMM_RX_ERR_PARAM;
	}

	primask = VcpCommRx_EnterCritical();
	if (s_rx.queue_count == 0u)
	{
		VcpCommRx_ExitCritical(primask);
		return VCP_COMM_RX_ERR_EMPTY;
	}

	memcpy(frame, &s_rx.queue[s_rx.queue_head], sizeof(*frame));
	s_rx.queue_head++;
	if (s_rx.queue_head >= VCP_COMM_RX_QUEUE_DEPTH)
	{
		s_rx.queue_head = 0u;
	}
	s_rx.queue_count--;
	VcpCommRx_UpdateReadyFlag();

	VcpCommRx_ExitCritical(primask);
	return VCP_COMM_RX_OK;
}

void VcpCommRx_GetStats(VcpCommRxStats *stats)
{
	uint32_t primask = 0u;

	if (stats == NULL)
	{
		return;
	}

	primask = VcpCommRx_EnterCritical();
	memcpy(stats, &s_rx.stats, sizeof(*stats));
	VcpCommRx_ExitCritical(primask);
}
