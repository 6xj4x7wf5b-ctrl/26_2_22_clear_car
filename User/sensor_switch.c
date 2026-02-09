#include "sensor_switch.h"

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

static GPIO_TypeDef * const s_proximity_switch_ports[PROXIMITY_SWITCH_COUNT] = {
	GPIOD,
	GPIOD,
	GPIOD,
	GPIOD,
	GPIOD,
	GPIOD,
	GPIOD,
	GPIOD
};

static const uint16_t s_proximity_switch_pins[PROXIMITY_SWITCH_COUNT] = {
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_4,
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7
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

GPIO_PinState ProximitySwitch_Read(uint8_t index)
{
	if (index >= PROXIMITY_SWITCH_COUNT)
	{
		return GPIO_PIN_RESET;
	}

	return HAL_GPIO_ReadPin(s_proximity_switch_ports[index], s_proximity_switch_pins[index]);
}

uint8_t ProximitySwitch_ReadAll(void)
{
	uint8_t bits = 0u;
	uint8_t i = 0u;

	for (i = 0u; i < PROXIMITY_SWITCH_COUNT; i++)
	{
		if (ProximitySwitch_Read(i) == GPIO_PIN_SET)
		{
			bits |= (uint8_t)(1u << i);
		}
	}

	return bits;
}
