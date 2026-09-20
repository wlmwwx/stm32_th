#ifndef __ENS160_H
#define __ENS160_H
#include "main.h"

typedef struct {
    uint16_t tvoc;     // ppb
    uint16_t eco2;     // ppm
    uint8_t  aqil;     // Air Quality Index (0-500)
    uint8_t  valid;     // 1=数据有效
} ens160_data_t;

void ENS160_Init(void);
void ENS160_Trigger(void);       // 触发一次测量
void ENS160_ReadResult(void);    // 读取结果（触发后等待 >50ms 调用）
ens160_data_t ENS160_GetData(void);

#endif
