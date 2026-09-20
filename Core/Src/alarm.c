#include "alarm.h"
#include "sensor.h"

static float s_th_hi = 30.0f;
static float s_th_lo = 5.0f;
static uint8_t s_alarm = 0;

void Alarm_Init(void)
{
    s_alarm = 0;
}

void Alarm_AdjustHi(float delta)
{
    s_th_hi += delta;
    if (s_th_hi > 125.0f) s_th_hi = 125.0f;
    if (s_th_hi < s_th_lo + HYSTERESIS) s_th_hi = s_th_lo + HYSTERESIS;
}

void Alarm_AdjustLo(float delta)
{
    s_th_lo += delta;
    if (s_th_lo < -55.0f) s_th_lo = -55.0f;
    if (s_th_lo > s_th_hi - HYSTERESIS) s_th_lo = s_th_hi - HYSTERESIS;
}

void Alarm_SaveThresholds(void)
{
    // TODO: persist to Flash
}

float Alarm_GetHiTh(void) { return s_th_hi; }
float Alarm_GetLoTh(void) { return s_th_lo; }
uint8_t Alarm_IsActive(void) { return s_alarm; }

void Alarm_Task100ms(void)
{
    float t = Sensor_GetTemp();
    if (!s_alarm && t >= s_th_hi) {
        s_alarm = 1;
    } else if (s_alarm && t <= s_th_hi - HYSTERESIS) {
        s_alarm = 0;
    }
}

void Alarm_BuzzerTask100ms(void)
{
    static uint8_t tick = 0;
    if (!s_alarm) {
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
        tick = 0;
        return;
    }
    if (++tick >= 5) tick = 0;
    if (tick < 3)
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    else
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
}
