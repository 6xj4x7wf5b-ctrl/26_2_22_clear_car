#include "motor_base.h"

bool Motor_SetDir(MotorHandle *motor, MotorDir dir)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (motor->dir_port == NULL)
	{
		return true;
	}

	HAL_GPIO_WritePin(motor->dir_port, motor->dir_pin,
										(dir == MOTOR_DIR_CW) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	return true;
}

bool Motor_SetCCR(MotorHandle *motor, uint32_t ccr)
{
	uint32_t arr = 0u;

	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	arr = motor->htim->Instance->ARR + 1u;
	if (arr > 0u && ccr >= arr)
	{
		ccr = arr - 1u;
	}

	__HAL_TIM_SET_COMPARE(motor->htim, motor->channel, ccr);

	return true;
}

bool Motor_SetARR(MotorHandle *motor, uint32_t arr)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (!IS_TIM_PERIOD(motor->htim, arr))
	{
		return false;
	}

	__HAL_TIM_SET_AUTORELOAD(motor->htim, arr);
	__HAL_TIM_SET_COUNTER(motor->htim, 0u);
	if (HAL_TIM_GenerateEvent(motor->htim, TIM_EVENTSOURCE_UPDATE) != HAL_OK)
	{
		return false;
	}

	return true;
}

bool Motor_SetEnable(MotorHandle *motor, uint8_t enable)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (motor->en_port == NULL)
	{
		return true;
	}

	HAL_GPIO_WritePin(motor->en_port, motor->en_pin,
										(enable != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	return true;
}

bool Motor_Run(MotorHandle *motor)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (!Motor_SetEnable(motor, 1u))
	{
		return false;
	}

	return (HAL_TIM_PWM_Start(motor->htim, motor->channel) == HAL_OK);
}

bool Motor_Run_With_Reset_Enable(MotorHandle *motor)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (!Motor_SetEnable(motor, 0u))
	{
		return false;
	}

	HAL_Delay(10);  // 确保使能信号被拉低一段时间

	if (!Motor_SetEnable(motor, 1u))
	{
		return false;
	}

	return (HAL_TIM_PWM_Start(motor->htim, motor->channel) == HAL_OK);
}

bool Motor_Stop(MotorHandle *motor)
{
	HAL_StatusTypeDef hal_ret = HAL_OK;

	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	hal_ret = HAL_TIM_PWM_Stop(motor->htim, motor->channel);
	if (!Motor_SetEnable(motor, 0u))
	{
		return false;
	}

	return (hal_ret == HAL_OK);
}


uint32_t Motor_SpeedToCcr(MotorHandle *motor, float speed)
{
	uint32_t ccr = 0u;
	uint32_t arr = 0u;

	if (motor == NULL || motor->htim == NULL)
	{
		return 0u;
	}

	arr = motor->htim->Instance->ARR + 1u;

	if (arr == 0u)
	{
		return 0u;
	}

	if (speed < 0.0f)
	{
		speed = 0.0f;
	}
	if (speed > 1.0f)
	{
		speed = 1.0f;
	}

	ccr = (uint32_t)((arr * speed) + 0.5f);
	if (ccr >= arr)
	{
		ccr = arr - 1u;
	}

	return ccr;
}

uint32_t Motor_SpeedToArr(MotorHandle *motor, float speed)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return 0u;
	}

	if (speed <= 0.0f)
	{
		return 65535u;   // 速度为0时给最大ARR
	}

	if (speed >1.0f)
	{
		return 0u;    // 速度超过1时给最小ARR
	
	}

    uint32_t arr = 65535UL * 100UL / speed;

    if(arr > 65535)
        arr = 65535;

    return arr;
}