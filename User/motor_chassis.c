#include "motor_chassis.h"

#include "tim.h"

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

bool Motor_ChassisMoveUp(float speed)
{
	speed = 1.0f - speed;

	uint32_t ccr_left = Motor_SpeedToCcr(&motor_crawler_left, speed);
	uint32_t ccr_right = Motor_SpeedToCcr(&motor_crawler_right, speed);

	bool ok_left = Motor_SetDir(&motor_crawler_left, MOTOR_DIR_CW);
	bool ok_right = Motor_SetDir(&motor_crawler_right, MOTOR_DIR_CCW);

	ok_left = Motor_SetCCR(&motor_crawler_left, ccr_left);
	ok_right = Motor_SetCCR(&motor_crawler_right, ccr_right);

	ok_left = Motor_Run(&motor_crawler_left);
	ok_right = Motor_Run(&motor_crawler_right);
	return (ok_left && ok_right);
}

bool Motor_ChassisMoveDown(float speed)
{
	speed = 1.0f - speed;

	uint32_t ccr_left = Motor_SpeedToCcr(&motor_crawler_left, speed);
	uint32_t ccr_right = Motor_SpeedToCcr(&motor_crawler_right, speed);

	bool ok_left = Motor_SetDir(&motor_crawler_left, MOTOR_DIR_CCW);
	bool ok_right = Motor_SetDir(&motor_crawler_right, MOTOR_DIR_CW);

	ok_left = Motor_SetCCR(&motor_crawler_left, ccr_left);
	ok_right = Motor_SetCCR(&motor_crawler_right, ccr_right);

	ok_left = Motor_Run(&motor_crawler_left);
	ok_right = Motor_Run(&motor_crawler_right);
	return (ok_left && ok_right);
}

bool Motor_ChassisMoveLeft(float speed)
{
	speed = 1.0f - speed;

	uint32_t ccr_up = Motor_SpeedToCcr(&motor_crawler_up, speed);
	uint32_t ccr_down = Motor_SpeedToCcr(&motor_crawler_down, speed);

	bool ok_up = Motor_SetDir(&motor_crawler_up, MOTOR_DIR_CCW);
	bool ok_down = Motor_SetDir(&motor_crawler_down, MOTOR_DIR_CW);

	ok_up = Motor_SetCCR(&motor_crawler_up, ccr_up);
	ok_down = Motor_SetCCR(&motor_crawler_down, ccr_down);

	ok_up = Motor_Run(&motor_crawler_up);
	ok_down = Motor_Run(&motor_crawler_down);
	return (ok_up && ok_down);
}

bool Motor_ChassisMoveRight(float speed)
{
	speed = 1.0f - speed;

	uint32_t ccr_up = Motor_SpeedToCcr(&motor_crawler_up, speed);
	uint32_t ccr_down = Motor_SpeedToCcr(&motor_crawler_down, speed);

	bool ok_up = Motor_SetDir(&motor_crawler_up, MOTOR_DIR_CW);
	bool ok_down = Motor_SetDir(&motor_crawler_down, MOTOR_DIR_CCW);

	ok_up = Motor_SetCCR(&motor_crawler_up, ccr_up);
	ok_down = Motor_SetCCR(&motor_crawler_down, ccr_down);

	ok_up = Motor_Run(&motor_crawler_up);
	ok_down = Motor_Run(&motor_crawler_down);
	return (ok_up && ok_down);
}

bool Motor_ChassisStopMove(void)
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
