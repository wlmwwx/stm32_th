#ifndef __AHT20_H
#define __AHT20_H
#include "main.h"

typedef struct {
    float temperature;  // 摄氏度
    float humidity;    // %RH
    uint8_t valid;      // 1=数据有效
} aht20_data_t;

void AHT20_Init(void);
void AHT20_Trigger(void);       // 触发一次测量
void AHT20_ReadResult(void);     // 读取结果（触发后等待 >10ms 调用）
aht20_data_t AHT20_GetData(void);

#endif
