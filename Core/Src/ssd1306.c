/**
 * Minimal SSD1306 OLED driver for STM32F1 (I2C)
 * 128x64 pixels, I2C address 0x78
 */
#include "ssd1306.h"
#include "main.h"

#define SSD1306_ADDR  0x78
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

static uint8_t s_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

static void SSD1306_WriteCmd(uint8_t cmd);
static void SSD1306_WriteData(const uint8_t *data, uint16_t len);
static void SSD1306_SetPage(uint8_t page);
static void SSD1306_SetColumn(uint8_t col);

void SSD1306_Init(void)
{
    HAL_Delay(100);

    SSD1306_WriteCmd(0xAE);  // display off
    SSD1306_WriteCmd(0xD5); SSD1306_WriteCmd(0x80);  // clock divide
    SSD1306_WriteCmd(0xA8); SSD1306_WriteCmd(0x3F);  // multiplex 64
    SSD1306_WriteCmd(0xD3); SSD1306_WriteCmd(0x00);  // no display offset
    SSD1306_WriteCmd(0x40);  // start line 0
    SSD1306_WriteCmd(0xA1);  // segment remap (column 127 = seg0)
    SSD1306_WriteCmd(0xC8);  // COM scan direction (reverse)
    SSD1306_WriteCmd(0xDA); SSD1306_WriteCmd(0x12);  // COM pins
    SSD1306_WriteCmd(0x81); SSD1306_WriteCmd(0xCF);  // contrast
    SSD1306_WriteCmd(0xD9); SSD1306_WriteCmd(0xF1);  // pre-charge
    SSD1306_WriteCmd(0xDB); SSD1306_WriteCmd(0x40);  // VCOMH
    SSD1306_WriteCmd(0x8D); SSD1306_WriteCmd(0x14);  // charge pump
    SSD1306_WriteCmd(0xAF);  // display on

    SSD1306_Clear();
    SSD1306_Update();
}

void SSD1306_Clear(void)
{
    for (uint16_t i = 0; i < sizeof(s_buffer); i++)
        s_buffer[i] = 0;
}

void SSD1306_Update(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        SSD1306_WriteCmd(0xB0 + page);
        SSD1306_WriteCmd(0x02);  // column low
        SSD1306_WriteCmd(0x10);  // column high
        uint8_t col = 0;
        SSD1306_WriteData(&s_buffer[page * SSD1306_WIDTH], SSD1306_WIDTH);
    }
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    uint16_t idx = x + (y / 8) * SSD1306_WIDTH;
    if (on)
        s_buffer[idx] |= (1 << (y % 8));
    else
        s_buffer[idx] &= ~(1 << (y % 8));
}

/* ─── Font rendering (6x8 bitmap, basic ASCII 0x20-0x7F) ─── */

/* Minimal 6x8 font — partial set for demo */
extern const uint8_t ssd1306_font_6x8[];

void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str)
{
    uint8_t col = x;
    while (*str) {
        uint8_t c = (uint8_t)(*str++);
        if (c < 0x20 || c > 0x7F) c = 0x20;
        c -= 0x20;
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t bits = ssd1306_font_6x8[c * 6 + i];
            for (uint8_t j = 0; j < 8; j++) {
                SSD1306_DrawPixel(col + i, y + j, (bits >> j) & 1);
            }
        }
        col += 7;
        if (col > SSD1306_WIDTH - 7) break;
    }
}

/* ─── Internal helpers ─── */

static void SSD1306_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, SSD1306_ADDR, buf, 2, 100);
}

static void SSD1306_WriteData(const uint8_t *data, uint16_t len)
{
    uint8_t *buf = (uint8_t *)data;
    // prepend register address byte
    uint8_t tmp[128 + 1];
    tmp[0] = 0x40;
    for (uint16_t i = 0; i < len && i < 128; i++) tmp[i + 1] = data[i];
    HAL_I2C_Master_Transmit(&hi2c1, SSD1306_ADDR, tmp, len + 1, 100);
}
