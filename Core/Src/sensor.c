#include "sensor.h"
#include "ds18b20.h"   // external one-wire driver

static float s_temp = 0.0f;
static uint32_t s_convert_start = 0;

void Sensor_Init(void)
{
    s_temp = 0.0f;
    s_convert_start = 0;
}

void Sensor_TaskStartConvert(void)
{
    DS18B20_StartConvert();
    s_convert_start = HAL_GetTick();
}

void Sensor_TaskReadResult(void)
{
    if (s_convert_start != 0 &&
        HAL_GetTick() - s_convert_start >= 800) {
        s_temp = DS18B20_ReadTemp();
        s_convert_start = 0;
    }
}

float Sensor_GetTemp(void)
{
    return s_temp;
}
