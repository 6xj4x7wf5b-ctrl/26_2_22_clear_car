#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
	MOTOR_DIR_CW = 0,
	MOTOR_DIR_CCW = 1
} MotorDir;

typedef struct
{
	TIM_HandleTypeDef *htim;
	uint32_t channel;
	GPIO_TypeDef *dir_port;
	uint16_t dir_pin;
	GPIO_TypeDef *en_port;
	uint16_t en_pin;
} MotorHandle;

bool Motor_Run(MotorHandle *motor);
bool Motor_Stop(MotorHandle *motor);

bool Motor_Run_With_Reset_Enable(MotorHandle *motor);		// 用于低电平使能

bool Motor_SetDir(MotorHandle *motor, MotorDir dir);
bool Motor_SetEnable(MotorHandle *motor, uint8_t enable);

bool Motor_SetCCR(MotorHandle *motor, uint32_t ccr);
bool Motor_SetARR(MotorHandle *motor, uint32_t arr);

uint32_t Motor_SpeedToCcr(MotorHandle *motor, float speed);
uint32_t Motor_SpeedToArr(MotorHandle *motor, float speed);

