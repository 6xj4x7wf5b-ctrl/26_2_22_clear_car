#include "pressure_sensor.h"

#include <string.h>

/* Modbus RTU 功能码 */
#define PRESSURE_SENSOR_MODBUS_FUNC_READ_HOLDING      0x03u
#define PRESSURE_SENSOR_MODBUS_FUNC_WRITE_SINGLE      0x06u
#define PRESSURE_SENSOR_MODBUS_MAX_REG_COUNT          125u

/* 说明书第 5 章涉及的寄存器地址 */
#define PRESSURE_SENSOR_REG_WEIGHT_START              0x0000u
#define PRESSURE_SENSOR_REG_WEIGHT_COUNT              2u
#define PRESSURE_SENSOR_REG_SAMPLE_RATE               0x000Eu
#define PRESSURE_SENSOR_REG_SLAVE_ADDR                0x000Fu
#define PRESSURE_SENSOR_REG_BAUDRATE                  0x0010u
#define PRESSURE_SENSOR_REG_TARE                      0x0015u
#define PRESSURE_SENSOR_REG_ZERO_CALIB                0x0016u
#define PRESSURE_SENSOR_REG_WRITE_PROTECT             0x0017u
#define PRESSURE_SENSOR_REG_CALIB_MODE                0x0018u
#define PRESSURE_SENSOR_REG_LINE_CALIB_FIRST          0x0019u
#define PRESSURE_SENSOR_REG_STD_WEIGHT                0x0006u
#define PRESSURE_SENSOR_REG_DIGITAL_FULL_SCALE        0x0023u
#define PRESSURE_SENSOR_REG_DIGITAL_SENSITIVITY       0x0024u
#define PRESSURE_SENSOR_REG_RANGE_COEFF               0x0026u

/* 关键控制寄存器写入值 */
#define PRESSURE_SENSOR_VALUE_DISABLE_WRITE_PROTECT   0x0001u
#define PRESSURE_SENSOR_VALUE_ZERO_CALIB              0x0001u
#define PRESSURE_SENSOR_VALUE_WEIGHT_CALIB_MODE       0x0000u
#define PRESSURE_SENSOR_VALUE_DIGITAL_CALIB_MODE      0x00FFu
#define PRESSURE_SENSOR_VALUE_TARE                    0x0001u
#define PRESSURE_SENSOR_VALUE_CLEAR_TARE              0x0002u

typedef struct
{
    UART_HandleTypeDef *huart;
    uint8_t slave_addr;
    uint32_t timeout_ms;
} PressureSensor_Handle;

/* 模块内全局配置：由 PressureSensor_Init() 按头文件默认宏重置 */
static PressureSensor_Handle g_pressure_sensor = {
    .huart = PRESSURE_SENSOR_DEFAULT_UART,
    .slave_addr = PRESSURE_SENSOR_DEFAULT_SLAVE_ADDR,
    .timeout_ms = PRESSURE_SENSOR_DEFAULT_TIMEOUT_MS,
};

/* Modbus RTU CRC16（多项式 0xA001） */
static uint16_t pressure_sensor_modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i = 0u;
    uint8_t j = 0u;

    if (data == NULL)
    {
        return 0u;
    }

    for (i = 0u; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0u; j < 8u; j++)
        {
            if ((crc & 0x0001u) != 0u)
            {
                crc >>= 1;
                crc ^= 0xA001u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* 功能码 0x06：写单个保持寄存器 */
static PressureSensor_Status pressure_sensor_write_single_register(uint16_t reg_addr,
                                                                   uint16_t reg_value)
{
    PressureSensor_Handle *sensor = &g_pressure_sensor;
    uint8_t request[8];
    uint8_t response[8];
    uint16_t crc = 0u;
    HAL_StatusTypeDef hal_status = HAL_OK;

    if ((sensor == NULL) || (sensor->huart == NULL))
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    request[0] = sensor->slave_addr;
    request[1] = PRESSURE_SENSOR_MODBUS_FUNC_WRITE_SINGLE;
    request[2] = (uint8_t)(reg_addr >> 8);
    request[3] = (uint8_t)(reg_addr & 0xFFu);
    request[4] = (uint8_t)(reg_value >> 8);
    request[5] = (uint8_t)(reg_value & 0xFFu);
    crc = pressure_sensor_modbus_crc16(request, 6u);
    request[6] = (uint8_t)(crc & 0xFFu);
    request[7] = (uint8_t)(crc >> 8);

    hal_status = HAL_UART_Transmit(sensor->huart, request, sizeof(request), sensor->timeout_ms);
    if (hal_status == HAL_TIMEOUT)
    {
        return PRESSURE_SENSOR_STATUS_TIMEOUT;
    }
    if (hal_status != HAL_OK)
    {
        return PRESSURE_SENSOR_STATUS_UART;
    }

    hal_status = HAL_UART_Receive(sensor->huart, response, sizeof(response), sensor->timeout_ms);
    if (hal_status == HAL_TIMEOUT)
    {
        return PRESSURE_SENSOR_STATUS_TIMEOUT;
    }
    if (hal_status != HAL_OK)
    {
        return PRESSURE_SENSOR_STATUS_UART;
    }

    if ((response[0] != request[0]) ||
        (response[1] != request[1]) ||
        (response[2] != request[2]) ||
        (response[3] != request[3]) ||
        (response[4] != request[4]) ||
        (response[5] != request[5]))
    {
        return PRESSURE_SENSOR_STATUS_FRAME;
    }

    crc = pressure_sensor_modbus_crc16(response, 6u);
    if ((response[6] != (uint8_t)(crc & 0xFFu)) ||
        (response[7] != (uint8_t)(crc >> 8)))
    {
        return PRESSURE_SENSOR_STATUS_CRC;
    }

    return PRESSURE_SENSOR_STATUS_OK;
}

/* 功能码 0x03：读取保持寄存器 */
static PressureSensor_Status pressure_sensor_read_holding_registers(uint16_t reg_addr,
                                                                    uint16_t reg_count,
                                                                    uint16_t *register_buf,
                                                                    uint16_t register_buf_len)
{
    PressureSensor_Handle *sensor = &g_pressure_sensor;
    uint8_t request[8];
    uint8_t response[5u + (PRESSURE_SENSOR_MODBUS_MAX_REG_COUNT * 2u)];
    uint16_t crc = 0u;
    uint16_t expected_len = 0u;
    uint16_t i = 0u;
    HAL_StatusTypeDef hal_status = HAL_OK;

    if ((sensor == NULL) || (sensor->huart == NULL) || (register_buf == NULL))
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    if ((reg_count == 0u) ||
        (reg_count > PRESSURE_SENSOR_MODBUS_MAX_REG_COUNT) ||
        (register_buf_len < reg_count))
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    request[0] = sensor->slave_addr;
    request[1] = PRESSURE_SENSOR_MODBUS_FUNC_READ_HOLDING;
    request[2] = (uint8_t)(reg_addr >> 8);
    request[3] = (uint8_t)(reg_addr & 0xFFu);
    request[4] = (uint8_t)(reg_count >> 8);
    request[5] = (uint8_t)(reg_count & 0xFFu);
    crc = pressure_sensor_modbus_crc16(request, 6u);
    request[6] = (uint8_t)(crc & 0xFFu);
    request[7] = (uint8_t)(crc >> 8);

    expected_len = (uint16_t)(5u + (reg_count * 2u));

    hal_status = HAL_UART_Transmit(sensor->huart, request, sizeof(request), sensor->timeout_ms);
    if (hal_status == HAL_TIMEOUT)
    {
        return PRESSURE_SENSOR_STATUS_TIMEOUT;
    }
    if (hal_status != HAL_OK)
    {
        return PRESSURE_SENSOR_STATUS_UART;
    }

    hal_status = HAL_UART_Receive(sensor->huart, response, expected_len, sensor->timeout_ms);
    if (hal_status == HAL_TIMEOUT)
    {
        return PRESSURE_SENSOR_STATUS_TIMEOUT;
    }
    if (hal_status != HAL_OK)
    {
        return PRESSURE_SENSOR_STATUS_UART;
    }

    if ((response[0] != sensor->slave_addr) ||
        (response[1] != PRESSURE_SENSOR_MODBUS_FUNC_READ_HOLDING) ||
        (response[2] != (uint8_t)(reg_count * 2u)))
    {
        return PRESSURE_SENSOR_STATUS_FRAME;
    }

    crc = pressure_sensor_modbus_crc16(response, (uint16_t)(expected_len - 2u));
    if ((response[expected_len - 2u] != (uint8_t)(crc & 0xFFu)) ||
        (response[expected_len - 1u] != (uint8_t)(crc >> 8)))
    {
        return PRESSURE_SENSOR_STATUS_CRC;
    }

    for (i = 0u; i < reg_count; i++)
    {
        register_buf[i] = (uint16_t)(((uint16_t)response[3u + (i * 2u)] << 8) |
                                     (uint16_t)response[4u + (i * 2u)]);
    }

    return PRESSURE_SENSOR_STATUS_OK;
}

/* 初始化模块默认通信参数 */
void PressureSensor_Init(void)
{
    g_pressure_sensor.huart = PRESSURE_SENSOR_DEFAULT_UART;
    g_pressure_sensor.slave_addr = PRESSURE_SENSOR_DEFAULT_SLAVE_ADDR;
    g_pressure_sensor.timeout_ms = PRESSURE_SENSOR_DEFAULT_TIMEOUT_MS;
}

/* 读取当前重量（默认数据字节序 2143，对应低字在前） */
PressureSensor_Status PressureSensor_ReadWeight(int32_t *weight)
{
    uint16_t register_buf[PRESSURE_SENSOR_REG_WEIGHT_COUNT] = {0u, 0u};
    PressureSensor_Status status = PRESSURE_SENSOR_STATUS_OK;

    if (weight == NULL)
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    status = pressure_sensor_read_holding_registers(PRESSURE_SENSOR_REG_WEIGHT_START,
                                                    PRESSURE_SENSOR_REG_WEIGHT_COUNT,
                                                    register_buf,
                                                    (uint16_t)(sizeof(register_buf) / sizeof(register_buf[0])));
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    *weight = (int32_t)(((uint32_t)register_buf[1] << 16) | (uint32_t)register_buf[0]);
    return PRESSURE_SENSOR_STATUS_OK;
}

/* 关闭写保护，后续校准/参数写入依赖此步骤 */
PressureSensor_Status PressureSensor_DisableWriteProtect(void)
{
    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_WRITE_PROTECT,
                                                 PRESSURE_SENSOR_VALUE_DISABLE_WRITE_PROTECT);
}

/* 零点校准流程 */
PressureSensor_Status PressureSensor_ZeroCalibrate(void)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_ZERO_CALIB,
                                                 PRESSURE_SENSOR_VALUE_ZERO_CALIB);
}

/* 砝码校准流程 */
PressureSensor_Status PressureSensor_WeightCalibrate(uint16_t std_weight)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_CALIB_MODE,
                                                   PRESSURE_SENSOR_VALUE_WEIGHT_CALIB_MODE);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_STD_WEIGHT,
                                                 std_weight);
}

/* 折线校准流程（最多 10 点） */
PressureSensor_Status PressureSensor_LineCalibrate(const uint16_t *points,
                                                   uint8_t point_count)
{
    uint8_t i = 0u;
    uint16_t reg_addr = 0u;
    PressureSensor_Status status = PRESSURE_SENSOR_STATUS_OK;

    if ((points == NULL) ||
        (point_count == 0u) || (point_count > PRESSURE_SENSOR_LINE_CALIB_MAX_POINTS))
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_CALIB_MODE,
                                                   (uint16_t)point_count);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    for (i = 0u; i < point_count; i++)
    {
        reg_addr = (uint16_t)(PRESSURE_SENSOR_REG_LINE_CALIB_FIRST + i);
        status = pressure_sensor_write_single_register(reg_addr, points[i]);
        if (status != PRESSURE_SENSOR_STATUS_OK)
        {
            return status;
        }
    }

    return PRESSURE_SENSOR_STATUS_OK;
}

/* 数字校准流程 */
PressureSensor_Status PressureSensor_DigitalCalibrate(uint16_t full_scale,
                                                      uint16_t sensitivity_x1000,
                                                      uint16_t range_coeff_x10000)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_CALIB_MODE,
                                                   PRESSURE_SENSOR_VALUE_DIGITAL_CALIB_MODE);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_DIGITAL_FULL_SCALE,
                                                   full_scale);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_DIGITAL_SENSITIVITY,
                                                   sensitivity_x1000);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    if (range_coeff_x10000 > 0u)
    {
        status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_RANGE_COEFF,
                                                       range_coeff_x10000);
        if (status != PRESSURE_SENSOR_STATUS_OK)
        {
            return status;
        }
    }

    return PRESSURE_SENSOR_STATUS_OK;
}

/* 修改采样速率 */
PressureSensor_Status PressureSensor_SetSampleRate(uint16_t sample_rate_hz)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_SAMPLE_RATE,
                                                 sample_rate_hz);
}

/* 修改站地址（成功后同步更新本地地址） */
PressureSensor_Status PressureSensor_SetSlaveAddress(uint8_t new_addr)
{
    PressureSensor_Status status = PRESSURE_SENSOR_STATUS_OK;

    if (new_addr == 0u)
    {
        return PRESSURE_SENSOR_STATUS_PARAM;
    }

    status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    status = pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_SLAVE_ADDR,
                                                   (uint16_t)new_addr);
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    g_pressure_sensor.slave_addr = new_addr;
    return PRESSURE_SENSOR_STATUS_OK;
}

/* 修改波特率编码 */
PressureSensor_Status PressureSensor_SetBaudrateCode(uint16_t baud_code)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_BAUDRATE,
                                                 baud_code);
}

/* 去皮 */
PressureSensor_Status PressureSensor_Tare(void)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_TARE,
                                                 PRESSURE_SENSOR_VALUE_TARE);
}

/* 取消去皮 */
PressureSensor_Status PressureSensor_ClearTare(void)
{
    PressureSensor_Status status = PressureSensor_DisableWriteProtect();
    if (status != PRESSURE_SENSOR_STATUS_OK)
    {
        return status;
    }

    return pressure_sensor_write_single_register(PRESSURE_SENSOR_REG_TARE,
                                                 PRESSURE_SENSOR_VALUE_CLEAR_TARE);
}

/* 状态码转字符串 */
const char *PressureSensor_StatusString(PressureSensor_Status status)
{
    switch (status)
    {
        case PRESSURE_SENSOR_STATUS_OK:
            return "ok";
        case PRESSURE_SENSOR_STATUS_PARAM:
            return "param";
        case PRESSURE_SENSOR_STATUS_UNSUPPORTED:
            return "unsupported";
        case PRESSURE_SENSOR_STATUS_UART:
            return "uart";
        case PRESSURE_SENSOR_STATUS_TIMEOUT:
            return "timeout";
        case PRESSURE_SENSOR_STATUS_CRC:
            return "crc";
        case PRESSURE_SENSOR_STATUS_FRAME:
            return "frame";
        default:
            return "unknown";
    }
}
