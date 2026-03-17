#include "imu_jy901s.h"

#include "main.h"

#include <stdbool.h>
#include <string.h>

#define IMU_JY901S_FRAME_TYPE_ACCEL         0x51u
#define IMU_JY901S_FRAME_TYPE_GYRO          0x52u
#define IMU_JY901S_FRAME_TYPE_QUATERNION    0x59u
#define IMU_JY901S_SCALE_DIVISOR            32768.0f
#define IMU_JY901S_ACCEL_RANGE_G            16.0f
#define IMU_JY901S_GYRO_RANGE_DPS           2000.0f
#define IMU_JY901S_GRAVITY_MPS2             9.80665f
#define IMU_JY901S_DEG_TO_RAD               (3.14159265358979323846f / 180.0f)

typedef struct
{
    uint8_t ring_buffer[IMU_JY901S_RING_BUFFER_SIZE];
    uint16_t ring_head;
    uint16_t ring_tail;

    uint8_t frame_buffer[IMU_JY901S_FRAME_SIZE];
    uint8_t frame_index;
} IMU_JY901S_Context;

static IMU_JY901S_Context s_imu_context;
static IMU_JY901S_Data s_imu_data;

static uint16_t imu_jy901s_ring_next(uint16_t index)
{
    return (uint16_t)((index + 1u) % IMU_JY901S_RING_BUFFER_SIZE);
}

static bool imu_jy901s_ring_push(IMU_JY901S_Context *context, uint8_t byte)
{
    uint16_t next_head = 0u;

    if (context == NULL)
    {
        return false;
    }

    next_head = imu_jy901s_ring_next(context->ring_head);
    if (next_head == context->ring_tail)
    {
        return false;
    }

    context->ring_buffer[context->ring_head] = byte;
    context->ring_head = next_head;
    return true;
}

static bool imu_jy901s_ring_pop(IMU_JY901S_Context *context, uint8_t *out_byte)
{
    if ((context == NULL) || (out_byte == NULL))
    {
        return false;
    }

    if (context->ring_head == context->ring_tail)
    {
        return false;
    }

    *out_byte = context->ring_buffer[context->ring_tail];
    context->ring_tail = imu_jy901s_ring_next(context->ring_tail);
    return true;
}

static int16_t imu_jy901s_to_int16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[1] << 8u) | (uint16_t)data[0]);
}

static bool imu_jy901s_checksum_ok(const uint8_t *frame)
{
    uint8_t sum = 0u;
    uint8_t i = 0u;

    if (frame == NULL)
    {
        return false;
    }

    for (i = 0u; i < (IMU_JY901S_FRAME_SIZE - 1u); i++)
    {
        sum = (uint8_t)(sum + frame[i]);
    }

    return (sum == frame[IMU_JY901S_FRAME_SIZE - 1u]);
}

static void imu_jy901s_parser_resync(IMU_JY901S_Context *context)
{
    uint8_t i = 0u;

    if (context == NULL)
    {
        return;
    }

    for (i = 1u; i < context->frame_index; i++)
    {
        if (context->frame_buffer[i] == IMU_JY901S_FRAME_HEAD)
        {
            const uint8_t remain = (uint8_t)(context->frame_index - i);
            memmove(context->frame_buffer, &context->frame_buffer[i], remain);
            context->frame_index = remain;
            return;
        }
    }

    context->frame_index = 0u;
}

static void imu_jy901s_update_data(IMU_JY901S_Data *data, const uint8_t *frame)
{
    const int16_t raw_x = imu_jy901s_to_int16(&frame[2]);
    const int16_t raw_y = imu_jy901s_to_int16(&frame[4]);
    const int16_t raw_z = imu_jy901s_to_int16(&frame[6]);
    const int16_t raw_t = imu_jy901s_to_int16(&frame[8]);

    if ((data == NULL) || (frame == NULL))
    {
        return;
    }

    switch (frame[1])
    {
    case IMU_JY901S_FRAME_TYPE_ACCEL:
        data->linear_acceleration_x = (((float)raw_x / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_ACCEL_RANGE_G) * IMU_JY901S_GRAVITY_MPS2;
        data->linear_acceleration_y = (((float)raw_y / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_ACCEL_RANGE_G) * IMU_JY901S_GRAVITY_MPS2;
        data->linear_acceleration_z = (((float)raw_z / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_ACCEL_RANGE_G) * IMU_JY901S_GRAVITY_MPS2;
        break;

    case IMU_JY901S_FRAME_TYPE_GYRO:
        data->angular_velocity_x = (((float)raw_x / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_GYRO_RANGE_DPS) * IMU_JY901S_DEG_TO_RAD;
        data->angular_velocity_y = (((float)raw_y / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_GYRO_RANGE_DPS) * IMU_JY901S_DEG_TO_RAD;
        data->angular_velocity_z = (((float)raw_z / IMU_JY901S_SCALE_DIVISOR) * IMU_JY901S_GYRO_RANGE_DPS) * IMU_JY901S_DEG_TO_RAD;
        break;

    case IMU_JY901S_FRAME_TYPE_QUATERNION:
        data->quaternion_w = (float)raw_x / IMU_JY901S_SCALE_DIVISOR;
        data->quaternion_x = (float)raw_y / IMU_JY901S_SCALE_DIVISOR;
        data->quaternion_y = (float)raw_z / IMU_JY901S_SCALE_DIVISOR;
        data->quaternion_z = (float)raw_t / IMU_JY901S_SCALE_DIVISOR;
        break;

    default:
        return;
    }

    data->update_tick_ms = HAL_GetTick();
}

void IMU_JY901S_Init(void)
{
    memset(&s_imu_context, 0, sizeof(s_imu_context));
    memset(&s_imu_data, 0, sizeof(s_imu_data));
}

uint16_t IMU_JY901S_Feed(const uint8_t *src, uint16_t len)
{
    uint16_t pushed = 0u;

    if (src == NULL)
    {
        return 0u;
    }

    while (pushed < len)
    {
        if (!imu_jy901s_ring_push(&s_imu_context, src[pushed]))
        {
            break;
        }
        pushed++;
    }

    return pushed;
}

uint16_t IMU_JY901S_Process(void)
{
    uint16_t parsed = 0u;
    uint8_t byte = 0u;

    while (imu_jy901s_ring_pop(&s_imu_context, &byte))
    {
        if ((s_imu_context.frame_index == 0u) && (byte != IMU_JY901S_FRAME_HEAD))
        {
            continue;
        }

        s_imu_context.frame_buffer[s_imu_context.frame_index++] = byte;

        if (s_imu_context.frame_index < IMU_JY901S_FRAME_SIZE)
        {
            continue;
        }

        if (!imu_jy901s_checksum_ok(s_imu_context.frame_buffer))
        {
            imu_jy901s_parser_resync(&s_imu_context);
            continue;
        }

        imu_jy901s_update_data(&s_imu_data, s_imu_context.frame_buffer);
        s_imu_context.frame_index = 0u;
        parsed++;
    }

    return parsed;
}

const IMU_JY901S_Data *IMU_JY901S_GetData(void)
{
    return &s_imu_data;
}
