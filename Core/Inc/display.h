#ifndef __DISPLAY_H
#define __DISPLAY_H
#include "main.h"
#include "key.h"

typedef enum { PAGE_MAIN = 0, PAGE_SET_HI, PAGE_SET_LO, PAGE_COUNT } page_t;

void Display_Init(void);
void Display_Refresh(void);
page_t Menu_GetPage(void);
uint8_t Menu_IsEditing(void);
void Menu_OnKey(key_evt_t evt);

#endif
