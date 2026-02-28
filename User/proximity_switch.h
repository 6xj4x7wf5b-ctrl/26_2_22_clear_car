#pragma once

#include "main.h"
#include <stdint.h>

#define PROXIMITY_SWITCH_COUNT 8u

GPIO_PinState ProximitySwitch_Read(uint8_t index);
uint8_t ProximitySwitch_ReadAll(void);
