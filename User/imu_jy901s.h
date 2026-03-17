#ifndef IMU_JY901S_H
#define IMU_JY901S_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_JY901S_FRAME_HEAD               0x55u
#define IMU_JY901S_FRAME_SIZE               11u
#define IMU_JY901S_RING_BUFFER_SIZE         256u

typedef struct
{
    uint32_t update_tick_ms;

    float quaternion_w;
    float quaternion_x;
    float quaternion_y;
    float quaternion_z;

    float angular_velocity_x;
    float angular_velocity_y;
    float angular_velocity_z;

    float linear_acceleration_x;
    float linear_acceleration_y;
    float linear_acceleration_z;
} IMU_JY901S_Data;

void IMU_JY901S_Init(void);
uint16_t IMU_JY901S_Feed(const uint8_t *src, uint16_t len);
uint16_t IMU_JY901S_Process(void);
const IMU_JY901S_Data *IMU_JY901S_GetData(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_JY901S_H */
