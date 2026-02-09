#pragma once

#include "main.h"
#include <stdint.h>



#define SAFETY_EDGE_COUNT       4u
#define PROXIMITY_SWITCH_COUNT  8u

// 安全触边
GPIO_PinState SafetyEdge_Read(uint8_t index);
uint8_t SafetyEdge_ReadAll(void);

// 接近开关
GPIO_PinState ProximitySwitch_Read(uint8_t index);
uint8_t ProximitySwitch_ReadAll(void);

