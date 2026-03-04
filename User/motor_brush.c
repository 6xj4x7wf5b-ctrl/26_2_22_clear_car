#include "motor_brush.h"

#include "motor_base.h"
#include "tim.h"
#include <stdint.h>

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

static float speed_scale(float speed)
{
	// 如果你想限制输入范围
    if (speed < 0.0) speed = 0.0;
    if (speed > 1.0) speed = 1.0;

    return 0.055 + 0.042 * speed;
}

bool Motor_BrushStart(float speed)
{
	speed = speed_scale(speed);

	uint32_t ccr_left = Motor_SpeedToCcr(&motor_brush_left, speed);
	uint32_t ccr_right = Motor_SpeedToCcr(&motor_brush_right, speed);


	bool ok_left = Motor_SetCCR(&motor_brush_left, ccr_left);
	bool ok_right = Motor_SetCCR(&motor_brush_right, ccr_right);

	ok_left = Motor_Run(&motor_brush_left);
	ok_right = Motor_Run(&motor_brush_right);
	return (ok_left && ok_right);
}

bool Motor_BrushStop(void)
{
	// bool ok_left = Motor_Stop(&motor_brush_left);
	// bool ok_right = Motor_Stop(&motor_brush_right);
	bool ok = Motor_BrushStart(0.0f);

	return ok;
}

