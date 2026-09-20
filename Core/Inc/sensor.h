#ifndef __SENSOR_H
#define __SENSOR_H
#include "main.h"

void Sensor_Init(void);
void Sensor_TaskStartConvert(void);   // call every 1000ms
void Sensor_TaskReadResult(void);    // call every 100ms
float Sensor_GetTemp(void);

#endif
