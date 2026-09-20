#ifndef __DISPLAY_H
#define __DISPLAY_H
#include "main.h"
#include "key.h"
#include "sensor.h"

typedef enum { PAGE_MAIN = 0, PAGE_ENS, PAGE_AHT, PAGE_COUNT } page_t;

void Display_Init(void);
void Display_Refresh(void);
void Display_UpdateHistory(const sensor_data_t *s);
page_t Menu_GetPage(void);
void Menu_OnKey(key_id_t key_id, key_evt_t evt);

#endif
