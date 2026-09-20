#ifndef __ALARM_H
#define __ALARM_H
#include "main.h"

#define HYSTERESIS 1.0f

void Alarm_Init(void);
void Alarm_AdjustHi(float delta);
void Alarm_AdjustLo(float delta);
void Alarm_SaveThresholds(void);
float Alarm_GetHiTh(void);
float Alarm_GetLoTh(void);
uint8_t Alarm_IsActive(void);
void Alarm_Task100ms(void);
void Alarm_BuzzerTask100ms(void);

#endif
