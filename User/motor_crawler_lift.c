#include "motor_crawler_lift.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "motor_base.h"
#include "tim.h"
#include "proximity_switch.h"
#include <stdbool.h>

/*
	 * 履带升降电机类型： 	28步进电机
	 * 调节速度方式：	   ARR调节（占空比固定为50%）	
*/
#define MOTOR_CRAWLER_LIFT_PROXIMITY_SWITCH_INDEX   1	

static MotorHandle motor_crawler_lift = {
	.htim = &htim3,
	.channel = TIM_CHANNEL_1,
	.dir_port = TIM3_CH1_DIR_GPIO_Port,
	.dir_pin = TIM3_CH1_DIR_Pin,
	.en_port = TIM3_CH1_EN_GPIO_Port,
	.en_pin = TIM3_CH1_EN_Pin
};

bool Motor_CrawlerLiftUp()
{
	Motor_SetDir(&motor_crawler_lift, MOTOR_DIR_CW);

	Motor_Run_With_Reset_Enable(&motor_crawler_lift);
	osDelay(pdMS_TO_TICKS(2000));	
	Motor_Stop(&motor_crawler_lift);

	if (ProximitySwitch_Read(MOTOR_CRAWLER_LIFT_PROXIMITY_SWITCH_INDEX) == GPIO_PIN_SET)
		return true;
	
	else
		return false;
}

bool Motor_CrawlerLiftDown()
{
	Motor_SetDir(&motor_crawler_lift, MOTOR_DIR_CCW);

	Motor_Run_With_Reset_Enable(&motor_crawler_lift);
	osDelay(pdMS_TO_TICKS(2000));	
	Motor_Stop(&motor_crawler_lift);

	if (ProximitySwitch_Read(MOTOR_CRAWLER_LIFT_PROXIMITY_SWITCH_INDEX) == GPIO_PIN_SET)
		return true;
	
	else
		return false;
}

bool Motor_CrawlerLiftSetSpeed(float speed)
{
	uint32_t arr = Motor_SpeedToArr(&motor_crawler_lift, speed);
	return Motor_SetARR(&motor_crawler_lift, arr);
}
