/**
 * ENS160 multi-gas sensor driver for STM32F1 (I2C)
 * I2C address: 0x53
 *
 * Registers:
 *   0x00 - PART_ID      (should be 0x0160)
 *   0x10 - DATA_STATUS  (bit0 = new data)
 *   0x21 - DATA_AQI     (AQI 0-5 → 0-500 index)
 *   0x22 - DATA_TVOC    (ppb)
 *   0x23 - DATA_ECO2    (ppm)
 *   0xB0 - OPMODE       (0=deep sleep, 1=idle, 2=reset, 3=normal)
 *   0x80 - COMMAND      (0xE0 = get FW version, 0x06 = start normal)
 */
#include "ens160.h"
#include <string.h>

#define ENS160_ADDR  0x53

static ens160_data_t s_data;
static uint32_t s_measure_start;

void ENS160_Init(void)
{
    // Read PART_ID to verify device
    uint8_t cmd[2] = { 0x00, 0x00 };
    uint8_t part_id[2] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, cmd, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, ENS160_ADDR << 1, part_id, 2, 100);
    (void)part_id;  // silence unused warning

    // Set OPMODE to IDLE first
    uint8_t opmode[2] = { 0xB0, 0x01 };
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, opmode, 2, 100);
    HAL_Delay(10);

    // Start normal mode
    uint8_t cmd_norm[2] = { 0xB0, 0x03 };
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, cmd_norm, 2, 100);
    HAL_Delay(50);

    memset(&s_data, 0, sizeof(s_data));
    s_measure_start = 0;
}

void ENS160_Trigger(void)
{
    // ENS160 continuously measures in normal mode.
    // Trigger just marks "start of read window" for non-blocking read.
    s_measure_start = HAL_GetTick();
}

void ENS160_ReadResult(void)
{
    if (s_measure_start == 0) return;
    if (HAL_GetTick() - s_measure_start < 50) return;

    uint8_t status_reg[1] = { 0x10 };
    uint8_t status[1] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, status_reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, ENS160_ADDR << 1, status, 1, 100);

    if (!(status[0] & 0x02)) {
        // No new data available yet
        return;
    }

    // Read AQI (0x21)
    uint8_t aqi_reg[1] = { 0x21 };
    uint8_t aqi[1] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, aqi_reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, ENS160_ADDR << 1, aqi, 1, 100);

    // Read TVOC (0x22, 0x23 — 2 bytes)
    uint8_t tvoc_reg[1] = { 0x22 };
    uint8_t tvoc[2] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, tvoc_reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, ENS160_ADDR << 1, tvoc, 2, 100);

    // Read ECO2 (0x24, 0x25 — 2 bytes)
    uint8_t eco2_reg[1] = { 0x24 };
    uint8_t eco2[2] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, ENS160_ADDR << 1, eco2_reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, ENS160_ADDR << 1, eco2, 2, 100);

    s_data.aqil   = aqi[0];
    s_data.tvoc   = (uint16_t)tvoc[0] | ((uint16_t)tvoc[1] << 8);
    s_data.eco2   = (uint16_t)eco2[0] | ((uint16_t)eco2[1] << 8);
    s_data.valid  = 1;
    s_measure_start = 0;
}

ens160_data_t ENS160_GetData(void)
{
    return s_data;
}
