#pragma once

#include "main.h"
#include <stdint.h>

#define SAFETY_EDGE_COUNT 4u

GPIO_PinState SafetyEdge_Read(uint8_t index);
uint8_t SafetyEdge_ReadAll(void);
