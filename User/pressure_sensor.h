#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include "usart.h"

#include <stdint.h>

/* 默认通信配置（可按现场接线与设备参数修改） */
#define PRESSURE_SENSOR_DEFAULT_UART               (&huart1)
#define PRESSURE_SENSOR_DEFAULT_SLAVE_ADDR         0x01u
#define PRESSURE_SENSOR_DEFAULT_TIMEOUT_MS         100u
/* 折线校准最多支持 10 点（依据说明书） */
#define PRESSURE_SENSOR_LINE_CALIB_MAX_POINTS      10u

/* 接口返回状态码 */
typedef enum
{
    PRESSURE_SENSOR_STATUS_OK = 0,
    PRESSURE_SENSOR_STATUS_PARAM = 1,
    PRESSURE_SENSOR_STATUS_UNSUPPORTED = 2,
    PRESSURE_SENSOR_STATUS_UART = 3,
    PRESSURE_SENSOR_STATUS_TIMEOUT = 4,
    PRESSURE_SENSOR_STATUS_CRC = 5,
    PRESSURE_SENSOR_STATUS_FRAME = 6
} PressureSensor_Status;

/* 使用头文件默认宏初始化模块内部参数 */
void PressureSensor_Init(void);

/* 读取当前重量（说明书 5.1） */
PressureSensor_Status PressureSensor_ReadWeight(int32_t *weight);
/* 关闭写保护（后续写寄存器前建议先调用） */
PressureSensor_Status PressureSensor_DisableWriteProtect(void);
/* 零点校准（说明书 5.2） */
PressureSensor_Status PressureSensor_ZeroCalibrate(void);
/* 砝码校准：std_weight 为标准砝码值（说明书 5.3） */
PressureSensor_Status PressureSensor_WeightCalibrate(uint16_t std_weight);
/* 折线校准：points 为砝码点数组，point_count 为点数（说明书 5.4） */
PressureSensor_Status PressureSensor_LineCalibrate(const uint16_t *points,
                                                   uint8_t point_count);
/* 数字校准（说明书 5.5）：灵敏度单位 x1000，量程系数单位 x10000 */
PressureSensor_Status PressureSensor_DigitalCalibrate(uint16_t full_scale,
                                                      uint16_t sensitivity_x1000,
                                                      uint16_t range_coeff_x10000);
/* 修改采样速率（说明书 5.6） */
PressureSensor_Status PressureSensor_SetSampleRate(uint16_t sample_rate_hz);
/* 修改站地址（说明书 5.7） */
PressureSensor_Status PressureSensor_SetSlaveAddress(uint8_t new_addr);
/* 修改波特率编码值（说明书 5.8） */
PressureSensor_Status PressureSensor_SetBaudrateCode(uint16_t baud_code);
/* 去皮（说明书 5.9） */
PressureSensor_Status PressureSensor_Tare(void);
/* 取消去皮（说明书 5.9） */
PressureSensor_Status PressureSensor_ClearTare(void);

/* 状态码转可读字符串 */
const char *PressureSensor_StatusString(PressureSensor_Status status);

#endif /* PRESSURE_SENSOR_H */
