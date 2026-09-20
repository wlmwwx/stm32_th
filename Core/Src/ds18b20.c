/**
 * DS18B20 one-wire driver for STM32F1
 *
 * Pin: PA1 (configured as open-drain output)
 * Timing-critical: wrap bit operations with __disable_irq/__enable_irq
 */
#include "ds18b20.h"
#include "main.h"

#define DQ_PORT GPIOA
#define DQ_PIN  GPIO_PIN_1

static void DQ_Low(void);
static void DQ_Release(void);
static uint8_t DQ_ReadBit(void);
static void DQ_WriteBit(uint8_t bit);
static void DQ_WriteByte(uint8_t byte);
static uint8_t DQ_ReadByte(void);
static void DQ_EnterCritical(void);
static void DQ_ExitCritical(void);
static void delay_us(uint32_t us);

static uint8_t s_irq_enabled;

/* ─── Low-level pin ops ─── */

static void DQ_EnterCritical(void)
{
    s_irq_enabled = __get_PRIMASK();
    __disable_irq();
}

static void DQ_ExitCritical(void)
{
    if (!s_irq_enabled) __enable_irq();
}

static void DQ_Low(void)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = DQ_PIN;
    init.Mode = GPIO_MODE_OUTPUT_OD;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DQ_PORT, &init);
    HAL_GPIO_WritePin(DQ_PORT, DQ_PIN, GPIO_PIN_RESET);
}

static void DQ_Release(void)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = DQ_PIN;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DQ_PORT, &init);
}

static uint8_t DQ_ReadBit(void)
{
    DQ_Low();
    delay_us(2);
    DQ_Release();
    delay_us(10);
    uint8_t bit = HAL_GPIO_ReadPin(DQ_PORT, DQ_PIN) == GPIO_PIN_SET ? 1 : 0;
    delay_us(45);
    return bit;
}

static void DQ_WriteBit(uint8_t bit)
{
    DQ_Low();
    delay_us(bit ? 2 : 60);
    DQ_Release();
    if (bit) delay_us(45);
    else delay_us(5);
}

static void DQ_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        DQ_WriteBit(byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t DQ_ReadByte(void)
{
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;
        if (DQ_ReadBit()) byte |= 0x80;
    }
    return byte;
}

/* ─── µs delay (blocking, using DWT cycle count) ─── */
#define TICK_US 72  /* 72 cycles per µs at 72 MHz */

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * TICK_US;
    while ((DWT->CYCCNT - start) < ticks) __NOP();
}

/* ─── public API ─── */

void DS18B20_StartConvert(void)
{
    DQ_EnterCritical();

    DQ_Release();
    delay_us(500);

    DQ_WriteByte(0xCC);  // skip ROM
    DQ_WriteByte(0x44);  // start conversion

    DQ_Release();

    DQ_ExitCritical();
}

float DS18B20_ReadTemp(void)
{
    DQ_EnterCritical();

    DQ_Release();
    delay_us(500);

    DQ_WriteByte(0xCC);  // skip ROM
    DQ_WriteByte(0xBE);  // read scratchpad

    uint8_t t_lsb = DQ_ReadByte();
    uint8_t t_msb = DQ_ReadByte();

    DQ_Release();
    DQ_ExitCritical();

    int16_t raw = (int16_t)(t_msb << 8) | t_lsb;
    return raw / 16.0f;
}
