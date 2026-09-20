/**
 * Unified sensor module — aggregates DS18B20, AHT20, ENS160
 * Non-blocking read pattern: trigger → wait → read
 */
#include "sensor.h"
#include <string.h>
#include "ds18b20.h"
#include "aht20.h"
#include "ens160.h"

static sensor_data_t s_data;
static uint32_t s_ds18b20_start;

void Sensor_Init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_ds18b20_start = 0;

    DS18B20_StartConvert();
    s_ds18b20_start = HAL_GetTick();

    AHT20_Init();
    ENS160_Init();
}

void Sensor_TaskStartConvert(void)
{
    // Trigger DS18B20 conversion (takes ~800ms)
    DS18B20_StartConvert();
    s_ds18b20_start = HAL_GetTick();

    // Trigger ENS160 measurement (continuous in normal mode, just kicks it)
    ENS160_Trigger();
}

void Sensor_TaskReadResult(void)
{
    // AHT20: trigger every read cycle, read after 10ms
    static uint8_t s_aht20_triggered = 0;
    static uint32_t s_aht20_start = 0;

    if (!s_aht20_triggered) {
        AHT20_Trigger();
        s_aht20_triggered = 1;
        s_aht20_start = HAL_GetTick();
    } else if (HAL_GetTick() - s_aht20_start >= 10) {
        AHT20_ReadResult();
        aht20_data_t a = AHT20_GetData();
        if (a.valid) {
            s_data.temp = a.temperature;
            s_data.humidity = a.humidity;
            s_data.aht20_valid = 1;
        }
        s_aht20_triggered = 0;
    }

    // ENS160: read after 50ms
    ENS160_ReadResult();
    ens160_data_t e = ENS160_GetData();
    if (e.valid) {
        s_data.tvoc   = e.tvoc;
        s_data.eco2   = e.eco2;
        s_data.aqil   = e.aqil;
        s_data.ens160_valid = 1;
    }

    // DS18B20: read after 800ms
    if (s_ds18b20_start != 0 && HAL_GetTick() - s_ds18b20_start >= 800) {
        s_data.temp = DS18B20_ReadTemp();
        s_data.ds18b20_valid = 1;
        s_ds18b20_start = 0;
    }
}

sensor_data_t Sensor_GetData(void)
{
    return s_data;
}
