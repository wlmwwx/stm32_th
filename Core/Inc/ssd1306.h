#ifndef __SSD1306_H
#define __SSD1306_H
#include "main.h"

void SSD1306_Init(void);
void SSD1306_Clear(void);
void SSD1306_Update(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on);
void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str);

#endif
