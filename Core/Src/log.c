#include "log.h"
#include "sensor.h"
#include "alarm.h"
#include <stdio.h>

int _write(int fd, char *ptr, int len)
{
    (void)fd;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, 100);
    return len;
}

void Log_Init(void)
{
    printf("stm32_th started\r\n");
}

void Log_TaskReport(void)
{
    printf("T:%.1f,HI:%.1f,LO:%.1f,ALM:%d\r\n",
           Sensor_GetData().temp, Alarm_GetHiTh(), Alarm_GetLoTh(), Alarm_IsActive());
}
