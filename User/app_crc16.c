#include "app_crc16.h"

uint16_t app_crc16_compute(const uint8_t *data, size_t length)
{
	uint16_t crc = 0xFFFFu;
	size_t i = 0u;
	uint8_t bit = 0u;

	if (data == NULL)
	{
		return 0u;
	}

	for (i = 0u; i < length; i++)
	{
		crc ^= data[i];
		for (bit = 0u; bit < 8u; bit++)
		{
			if ((crc & 0x0001u) != 0u)
			{
				crc >>= 1;
				crc ^= 0xA001u;
			}
			else
			{
				crc >>= 1;
			}
		}
	}

	return crc;
}
