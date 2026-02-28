#ifndef __APP_CRC16_H__
#define __APP_CRC16_H__

#include <stddef.h>
#include <stdint.h>

uint16_t app_crc16_compute(const uint8_t *data, size_t length);

#endif /* __APP_CRC16_H__ */
