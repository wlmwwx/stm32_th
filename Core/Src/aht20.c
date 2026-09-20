/**
 * AHT20 temperature & humidity sensor driver for STM32F1 (I2C)
 * I2C address: 0x38
 *
 * Measurement cycle: trigger → wait ≥10ms → read result
 * Data format: 5 bytes: [status][humidity][humidity][temp][temp]
 */
#include "aht20.h"
#include <string.h>

#define AHT20_ADDR   0x38
#define AHT20_CMD_TRIGGER   0xAC  // trigger measurement
#define AHT20_CMD_INIT      0xE1  // initialization (if needed)
#define AHT20_CMD_SOFTRESET 0xBA

static aht20_data_t s_data;
static uint32_t s_measure_start;

void AHT20_Init(void)
{
    uint8_t cmd = AHT20_CMD_SOFTRESET;
    HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, &cmd, 1, 100);
    HAL_Delay(20);  // reset takes 20ms

    uint8_t init[2] = { AHT20_CMD_INIT, 0x08 };
    HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, init, 2, 100);
    HAL_Delay(10);

    memset(&s_data, 0, sizeof(s_data));
    s_measure_start = 0;
}

void AHT20_Trigger(void)
{
    uint8_t cmd[2] = { AHT20_CMD_TRIGGER, 0x33 };
    HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, cmd, 2, 100);
    s_measure_start = HAL_GetTick();
}

void AHT20_ReadResult(void)
{
    if (s_measure_start == 0) return;
    if (HAL_GetTick() - s_measure_start < 10) return;

    uint8_t buf[6] = {0};
    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR << 1, buf, 6, 100) != HAL_OK) {
        s_data.valid = 0;
        return;
    }

    // Check status byte[0]: bit[7]=busy, bit[3]=calibrated
    uint8_t status = buf[0];
    if ((status & 0x68) != 0x08) {
        // Not ready or not calibrated
        s_data.valid = 0;
        return;
    }

    // Humidity: bytes[1]<<12 | bytes[2]<<4 | bytes[3]>>4
    uint32_t hum_raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    s_data.humidity = (hum_raw * 100.0f) / 1048576.0f;  // 2^20 = 1048576

    // Temperature: bytes[3]<<16 | bytes[4]<<8 | bytes[5]
    uint32_t temp_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    s_data.temperature = ((temp_raw * 200.0f) / 1048576.0f) - 50.0f;

    s_data.valid = 1;
    s_measure_start = 0;
}

aht20_data_t AHT20_GetData(void)
{
    return s_data;
}
