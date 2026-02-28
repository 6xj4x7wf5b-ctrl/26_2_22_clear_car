#include "safety_edge.h"

static GPIO_TypeDef * const s_safety_edge_ports[SAFETY_EDGE_COUNT] = {
	BOUNDARY_1_GPIO_Port,
	BOUNDARY_2_GPIO_Port,
	BOUNDARY_3_GPIO_Port,
	BOUNDARY_4_GPIO_Port
};

static const uint16_t s_safety_edge_pins[SAFETY_EDGE_COUNT] = {
	BOUNDARY_1_Pin,
	BOUNDARY_2_Pin,
	BOUNDARY_3_Pin,
	BOUNDARY_4_Pin
};

GPIO_PinState SafetyEdge_Read(uint8_t index)
{
	if (index >= SAFETY_EDGE_COUNT)
	{
		return GPIO_PIN_RESET;
	}

	return HAL_GPIO_ReadPin(s_safety_edge_ports[index], s_safety_edge_pins[index]);
}

uint8_t SafetyEdge_ReadAll(void)
{
	uint8_t bits = 0u;
	uint8_t i = 0u;

	for (i = 0u; i < SAFETY_EDGE_COUNT; i++)
	{
		if (SafetyEdge_Read(i) == GPIO_PIN_SET)
		{
			bits |= (uint8_t)(1u << i);
		}
	}

	return bits;
}
