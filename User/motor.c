#include "motor.h"

#include "tim.h"


/*--------------------- 履带电机 -----------------------*/
static MotorHandle motor_crawler_up = {
	.htim = &htim2,
	.channel = TIM_CHANNEL_1,
	.dir_port = TIM2_CH1_DIR_GPIO_Port,
	.dir_pin = TIM2_CH1_DIR_Pin,
	.en_port = NULL,
	.en_pin = 0u
};

static MotorHandle motor_crawler_down = {
	.htim = &htim2,
	.channel = TIM_CHANNEL_2,
	.dir_port = TIM2_CH2_DIR_GPIO_Port,
	.dir_pin = TIM2_CH2_DIR_Pin,
	.en_port = NULL,
	.en_pin = 0u
};

static MotorHandle motor_crawler_left = {
	.htim = &htim2,
	.channel = TIM_CHANNEL_3,
	.dir_port = TIM2_CH3_DIR_GPIO_Port,
	.dir_pin = TIM2_CH3_DIR_Pin,
	.en_port = NULL,
	.en_pin = 0u
};

static MotorHandle motor_crawler_right = {
	.htim = &htim2,
	.channel = TIM_CHANNEL_4,
	.dir_port = TIM2_CH4_DIR_GPIO_Port,
	.dir_pin = TIM2_CH4_DIR_Pin,
	.en_port = NULL,
	.en_pin = 0u
};

/*--------------------- 履带升降电机 -----------------------*/
static MotorHandle motor_crawler_lift = {
	.htim = &htim4,
	.channel = TIM_CHANNEL_1,
	.dir_port = TIM4_CH1_DIR_GPIO_Port,
	.dir_pin = TIM4_CH1_DIR_Pin,
	.en_port = TIM4_CH1_EN_GPIO_Port,
	.en_pin = TIM4_CH1_EN_Pin
};


/*--------------------- 球头锁定电机 -----------------------*/
static MotorHandle motor_ball_head_lock = {
	.htim = &htim3,
	.channel = TIM_CHANNEL_1,
	.dir_port = TIM3_CH1_DIR_GPIO_Port,
	.dir_pin = TIM3_CH1_DIR_Pin,
	.en_port = TIM3_CH1_EN_GPIO_Port,
	.en_pin = TIM3_CH1_EN_Pin
};

/*--------------------- 滚刷电机 -----------------------*/
static MotorHandle motor_brush_left = {
	.htim = &htim8,
	.channel = TIM_CHANNEL_1,
	.dir_port = NULL,
	.dir_pin = 0u,
	.en_port = NULL,
	.en_pin = 0u
};

static MotorHandle motor_brush_right = {
	.htim = &htim8,
	.channel = TIM_CHANNEL_2,
	.dir_port = NULL,
	.dir_pin = 0u,
	.en_port = NULL,
	.en_pin = 0u
};


/*--------------------- 电机基础接口实现 -----------------------*/

uint32_t Motor_DutyToCcr(MotorHandle *motor, float duty)
{
	float clamped = duty;
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

	if (clamped < 0.0f)
	{
		clamped = 0.0f;
	}
	if (clamped > 1.0f)
	{
		clamped = 1.0f;
	}

	ccr = (uint32_t)((arr * clamped) + 0.5f);
	if (ccr >= arr)
	{
		ccr = arr - 1u;
	}

	return ccr;
}

bool Motor_SetDir(MotorHandle *motor, MotorDir dir)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	/* 方向引脚可选，没有方向引脚时直接视为成功。 */
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

bool Motor_SetEnable(MotorHandle *motor, uint8_t enable)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	/* 使能引脚可选，没有使能引脚时直接视为成功。 */
	if (motor->en_port == NULL)
	{
		return true;
	}

	HAL_GPIO_WritePin(motor->en_port, motor->en_pin,
										(enable != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	return true;
}

bool Motor_Run(MotorHandle *motor, MotorDir dir, float ccr)
{
	if (motor == NULL || motor->htim == NULL)
	{
		return false;
	}

	if (!Motor_SetDir(motor, dir))
	{
		return false;
	}

	if (!Motor_SetCCR(motor, (uint32_t)ccr))
	{
		return false;
	}

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

	if (!Motor_SetCCR(motor, 0u))
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


/*--------------------- 机体移动命令接口实现 -----------------------*/
bool Motor_MoveUp(float speed)
{
	uint32_t ccr_up = Motor_DutyToCcr(&motor_crawler_up, speed);
	uint32_t ccr_down = Motor_DutyToCcr(&motor_crawler_down, speed);
	bool ok_up = false;
	bool ok_down = false;

	ok_up = Motor_Run(&motor_crawler_up, MOTOR_DIR_CW, (float)ccr_up);
	ok_down = Motor_Run(&motor_crawler_down, MOTOR_DIR_CW, (float)ccr_down);
	return (ok_up && ok_down);
}

bool Motor_MoveDown(float speed)
{
	uint32_t ccr_up = Motor_DutyToCcr(&motor_crawler_up, speed);
	uint32_t ccr_down = Motor_DutyToCcr(&motor_crawler_down, speed);
	bool ok_up = false;
	bool ok_down = false;

	ok_up = Motor_Run(&motor_crawler_up, MOTOR_DIR_CCW, (float)ccr_up);
	ok_down = Motor_Run(&motor_crawler_down, MOTOR_DIR_CCW, (float)ccr_down);
	return (ok_up && ok_down);
}

bool Motor_MoveLeft(float speed)
{
	uint32_t ccr_left = Motor_DutyToCcr(&motor_crawler_left, speed);
	uint32_t ccr_right = Motor_DutyToCcr(&motor_crawler_right, speed);
	bool ok_left = false;
	bool ok_right = false;

	ok_left = Motor_Run(&motor_crawler_left, MOTOR_DIR_CW, (float)ccr_left);
	ok_right = Motor_Run(&motor_crawler_right, MOTOR_DIR_CW, (float)ccr_right);
	return (ok_left && ok_right);
}

bool Motor_MoveRight(float speed)
{
	uint32_t ccr_left = Motor_DutyToCcr(&motor_crawler_left, speed);
	uint32_t ccr_right = Motor_DutyToCcr(&motor_crawler_right, speed);
	bool ok_left = false;
	bool ok_right = false;

	ok_left = Motor_Run(&motor_crawler_left, MOTOR_DIR_CCW, (float)ccr_left);
	ok_right = Motor_Run(&motor_crawler_right, MOTOR_DIR_CCW, (float)ccr_right);
	return (ok_left && ok_right);
}

bool Motor_StopMove(void)
{
	bool ok_up = false;
	bool ok_down = false;
	bool ok_left = false;
	bool ok_right = false;

	ok_up = Motor_Stop(&motor_crawler_up);
	ok_down = Motor_Stop(&motor_crawler_down);
	ok_left = Motor_Stop(&motor_crawler_left);
	ok_right = Motor_Stop(&motor_crawler_right);
	return (ok_up && ok_down && ok_left && ok_right);
}

bool Motor_CrawlerLiftUp(float duty)
{
	uint32_t ccr = Motor_DutyToCcr(&motor_crawler_lift, duty);
	return Motor_Run(&motor_crawler_lift, MOTOR_DIR_CW, (float)ccr);
}

bool Motor_CrawlerLiftDown(float duty)
{
	uint32_t ccr = Motor_DutyToCcr(&motor_crawler_lift, duty);
	return Motor_Run(&motor_crawler_lift, MOTOR_DIR_CCW, (float)ccr);
}

bool Motor_CrawlerLiftStop(void)
{
	return Motor_Stop(&motor_crawler_lift);
}

bool Motor_BallHeadLock(float duty)
{
	uint32_t ccr = Motor_DutyToCcr(&motor_ball_head_lock, duty);
	return Motor_Run(&motor_ball_head_lock, MOTOR_DIR_CW, (float)ccr);
}

bool Motor_BallHeadUnlock(float duty)
{
	uint32_t ccr = Motor_DutyToCcr(&motor_ball_head_lock, duty);
	return Motor_Run(&motor_ball_head_lock, MOTOR_DIR_CCW, (float)ccr);
}

bool Motor_BallHeadStop(void)
{
	return Motor_Stop(&motor_ball_head_lock);
}

bool Motor_BrushStart(float speed)
{
	uint32_t ccr_left = Motor_DutyToCcr(&motor_brush_left, speed);
	uint32_t ccr_right = Motor_DutyToCcr(&motor_brush_right, speed);
	bool ok_left = false;
	bool ok_right = false;

	ok_left = Motor_Run(&motor_brush_left, MOTOR_DIR_CW, (float)ccr_left);
	ok_right = Motor_Run(&motor_brush_right, MOTOR_DIR_CW, (float)ccr_right);
	return (ok_left && ok_right);
}

bool Motor_BrushStop(void)
{
	bool ok_left = false;
	bool ok_right = false;

	ok_left = Motor_Stop(&motor_brush_left);
	ok_right = Motor_Stop(&motor_brush_right);
	return (ok_left && ok_right);
}
