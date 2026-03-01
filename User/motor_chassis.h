#pragma once

#include "motor_base.h"

/* 机体（底盘）移动命令，speed 范围：0.0f ~ 1.0f */
bool Motor_ChassisMoveUp(float speed);
bool Motor_ChassisMoveDown(float speed);
bool Motor_ChassisMoveLeft(float speed);
bool Motor_ChassisMoveRight(float speed);
bool Motor_ChassisStopMove(void);

