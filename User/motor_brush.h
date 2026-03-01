#pragma once

#include "motor_base.h"

/*
	 * 球头锁定电机类型： 	无刷电机
	 * 调节速度方式：	   占空比调节
*/

/* 滚刷命令，speed 范围：0.0f ~ 1.0f */
bool Motor_BrushStart(float speed);
bool Motor_BrushStop(void);

bool Motor_Brush_SpeedScale(float speed, uint32_t *ccr);
