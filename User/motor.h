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






/* 机体移动命令，speed 范围：0.0f ~ 1.0f */
bool Motor_MoveUp(float speed);
bool Motor_MoveDown(float speed);
bool Motor_MoveLeft(float speed);
bool Motor_MoveRight(float speed);
bool Motor_StopMove(void);

/* 履带升出/下降执行机构命令，duty 范围：0.0f ~ 1.0f */
bool Motor_CrawlerLiftUp(float duty);
bool Motor_CrawlerLiftDown(float duty);
bool Motor_CrawlerLiftStop(void);

/* 球头锁定执行机构命令，duty 范围：0.0f ~ 1.0f */
bool Motor_BallHeadLock(float duty);
bool Motor_BallHeadUnlock(float duty);
bool Motor_BallHeadStop(void);

/* 滚刷命令，dir 为逻辑方向，speed 范围：0.0f ~ 1.0f */
bool Motor_BrushStart(float speed);
bool Motor_BrushStop(void);



/* 底层单电机基础接口。 */
bool Motor_Run(MotorHandle *motor, MotorDir dir, float ccr);
bool Motor_Stop(MotorHandle *motor);

bool Motor_SetDir(MotorHandle *motor, MotorDir dir);
bool Motor_SetCCR(MotorHandle *motor, uint32_t ccr);
bool Motor_SetEnable(MotorHandle *motor, uint8_t enable);

uint32_t Motor_DutyToCcr(MotorHandle *motor, float duty);
