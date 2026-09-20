#ifndef __SENSOR_H
#define __SENSOR_H
#include "main.h"

/* Unified sensor data */
typedef struct {
    float  temp;       // °C  (DS18B20 or AHT20)
    float  humidity;   // %RH (AHT20)
    uint16_t tvoc;     // ppb  (ENS160)
    uint16_t eco2;     // ppm  (ENS160)
    uint8_t  aqil;     // AQI  (ENS160)
    uint8_t  ds18b20_valid;
    uint8_t  aht20_valid;
    uint8_t  ens160_valid;
} sensor_data_t;

void Sensor_Init(void);
void Sensor_TaskStartConvert(void);  // call every 1000ms: trigger DS18B20 + ENS160
void Sensor_TaskReadResult(void);   // call every 100ms: read AHT20 + ENS160 + DS18B20
sensor_data_t Sensor_GetData(void);

#endif
