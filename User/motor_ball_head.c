#include "motor_ball_head.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "motor_base.h"
#include "tim.h"
#include "proximity_switch.h"
#include <stdbool.h>

/*
	 * 球头锁定电机类型： 	57步进电机
	 * 调节速度方式：	   ARR调节（占空比固定为50%）	
*/
#define MOTOR_BALL_HEAD_PROXIMITY_SWITCH_INDEX   1	

static MotorHandle motor_ball_head_lock = {
	.htim = &htim4,
	.channel = TIM_CHANNEL_1,
	.dir_port = TIM4_CH1_DIR_GPIO_Port,		// GPIOB
	.dir_pin = TIM4_CH1_DIR_Pin,			// GPIO_PIN_6
	.en_port = TIM4_CH1_EN_GPIO_Port,		// GPIOB
	.en_pin = TIM4_CH1_EN_Pin				// GPIO_PIN_5
};



bool Motor_BallHeadLock()
{	
	Motor_SetDir(&motor_ball_head_lock, MOTOR_DIR_CW);

	Motor_Run(&motor_ball_head_lock);
	osDelay(pdMS_TO_TICKS(2000));	
	Motor_Stop(&motor_ball_head_lock);

	if (ProximitySwitch_Read(MOTOR_BALL_HEAD_PROXIMITY_SWITCH_INDEX) == GPIO_PIN_SET)
		return true;
	
	else
		return false;
}

bool Motor_BallHeadUnlock()
{
	Motor_SetDir(&motor_ball_head_lock, MOTOR_DIR_CCW);

	Motor_Run(&motor_ball_head_lock);
	osDelay(pdMS_TO_TICKS(2000));	
	Motor_Stop(&motor_ball_head_lock);

	if (ProximitySwitch_Read(MOTOR_BALL_HEAD_PROXIMITY_SWITCH_INDEX) == GPIO_PIN_SET)
		return true;
	
	else
		return false;
}

bool Motor_BallHeadSetSpeed(float speed)
{
	uint32_t arr = Motor_SpeedToArr(&motor_ball_head_lock, speed);
	return Motor_SetARR(&motor_ball_head_lock, arr);
}


