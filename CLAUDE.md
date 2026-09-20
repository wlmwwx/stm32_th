# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal STM32F103C8T6 temperature monitoring project (Blue Pill board). Uses STM32CubeMX-generated HAL code with a cooperative-scheduler architecture. Supports DS18B20, AHT20 (temp+humidity), and ENS160 (air quality) sensors on a shared I2C1 bus, with OLED display, LED heartbeat, and serial logging.

**⚠️ Important workflow**: After editing `.ioc` in STM32CubeMX and regenerating code, **all USER CODE blocks in `main.c` will be preserved**, but CubeMX may overwrite other HAL files. Always run `git status` after regeneration. The application code (all `Core/Src/*.c` files except `main.c`, `stm32f1xx_hal_msp.c`, `stm32f1xx_it.c`) lives in user files and is not touched by CubeMX regeneration.

## Build

- **IDE**: STM32CubeIDE
- **IOC**: `stm32_th.ioc` — open in STM32CubeMX to regenerate HAL code
- **Linker script**: `STM32F103C8TX_FLASH.ld`
- **Float printf**: linker flag `-u _printf_float` set in `.cproject` (required for `printf("%.1f")`)

## Hardware Configuration

| Peripheral | Pin/Config |
|---|---|
| LED (heartbeat) | PC13, GPIO_Output, toggles every 500ms |
| Button (KEY) | PA0, GPIO_Input, Pull-down, press=high |
| DS18B20 (temp) | PA1, GPIO_Output Open-Drain (one-wire) |
| UART1 (log) | USART1, 115200, PA9/PA10 |
| UART3 (aux) | USART3, 115200, PB10/PB11 |
| Buzzer PWM | TIM3 CH1, PA6, 2kHz, 50% duty |
| OLED display | I2C1, PB6=SCL, PB7=SDA, 100kHz, SSD1306 128x64 (0x78) |
| AHT20 (temp+humidity) | I2C1, PB6/PB7, 100kHz (0x38) |
| ENS160 (air quality) | I2C1, PB6/PB7, 100kHz (0x53) |
| Clock | HSE 8MHz → PLL ×9 → 72MHz SYSCLK, APB1=36MHz, APB2=72MHz |

**All three I2C devices share the same bus (PB6/PB7) — they are all I2C address-multiplexed.**

## Architecture

Periodic task scheduler using `HAL_GetTick()` timestamps — no blocking delays:

```
while(1) → Scheduler_Run() → checks task table
    ├── 10ms:  Key scan → Menu event
    ├── 100ms: Display refresh
    ├── 100ms: Sensor read (DS18B20+AHT20+ENS160)
    ├── 100ms: Alarm check + buzzer
    ├── 500ms: LED heartbeat toggle (PC13)
    ├── 1000ms: Sensor trigger (DS18B20 conv + ENS160)
    └── 2000ms: UART log report
```

## Module List

| File | Purpose |
|---|---|
| `Core/Src/scheduler.c` | Task table runner (`TASK_COUNT = 7`) |
| `Core/Src/key.c` | Button state machine (RELEASED→DEBOUNCE→PRESSED) |
| `Core/Src/sensor.c` | Unified sensor API — aggregates all 3 sensors, non-blocking |
| `Core/Src/ds18b20.c` | One-wire bit-banging driver |
| `Core/Src/aht20.c` | AHT20 I2C driver (temp+humidity) |
| `Core/Src/ens160.c` | ENS160 I2C driver (TVOC/eCO2/AQI) |
| `Core/Src/alarm.c` | Hysteresis alarm + buzzer PWM divider |
| `Core/Src/display.c` | Menu state machine + SSD1306 rendering |
| `Core/Src/ssd1306.c` | Minimal SSD1306 I2C driver |
| `Core/Src/ssd1306_font.c` | 6×8 bitmap font (ASCII 0x20–0x7F) |
| `Core/Src/log.c` | `printf` redirect to UART1 (PA9) + periodic report |
| `Core/Src/main.c` | HAL init, task table (7 tasks), scheduler loop |

## Sensor Data (sensor_data_t)

```c
typedef struct {
    float  temp;        // °C  (AHT20 primary, DS18B20 fallback)
    float  humidity;     // %RH (AHT20)
    uint16_t tvoc;     // ppb  (ENS160)
    uint16_t eco2;     // ppm  (ENS160)
    uint8_t  aqil;     // AQI  (ENS160, 0–500)
    uint8_t  ds18b20_valid;
    uint8_t  aht20_valid;
    uint8_t  ens160_valid;
} sensor_data_t;
```

## Key Design Patterns

- **Scheduler**: `task_t` struct with `last_run`, `period_ms`, `(*func)()`. `Scheduler_Run()` uses timestamp comparison — no `HAL_Delay()`.
- **Key state machine**: 20ms debounce, 1000ms long-press, non-blocking `Key_Scan10ms()`.
- **Sensor non-blocking**: each sensor uses trigger → wait → read pattern managed by `Sensor_TaskReadResult()`.
- **Alarm hysteresis**: on at `th_hi`, off at `th_hi - HYSTERESIS` (1°C deadzone).
- **OLED**: framebuffer in RAM, `SSD1306_Update()` sends full display via I2C.
- **I2C shared bus**: OLED (0x78), AHT20 (0x38), ENS160 (0x53) all on I2C1, each addressed independently.

## Common Pitfalls

1. **DS18B20 reads 85°C**: one-wire bit timing broken by SysTick. `ds18b20.c` wraps byte ops with `__disable_irq().__enable_irq()`.
2. **`printf("%.1f")` shows blank**: missing `-u _printf_float` in linker flags — already set in `.cproject`.
3. **ISRs calling blocking functions**: `HAL_Delay()` and `printf()` must never be called from ISR context.
4. **Alarm chattering**: missing hysteresis — `alarm.c` uses separate on/off thresholds.
5. **Button press direction**: PA0 is Pull-down (press=high). If button is active-low on your board, change to Pull-up and pass `GPIO_PIN_RESET` to `Key_Init()`.
6. **CubeMX regeneration removes I2C driver**: if `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c` is missing after regeneration, run: `git checkout HEAD -- Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_hal_i2c.h Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_ll_i2c.h`
7. **AHT20/ENS160 show invalid data**: check I2C wiring (PB6=SCL, PB7=SDA), ensure pull-ups are present (typically 4.7kΩ on module boards).

## File Structure

```
Core/
  Inc/
    main.h              ← externs for hi2c1, huart1, huart3, htim3
    scheduler.h         ← task_t, TASK_COUNT (7), Scheduler_Run()
    key.h              ← key_t, key_evt_t, Key_*()
    sensor.h           ← sensor_data_t, Sensor_*()
    alarm.h            ← Alarm_*()
    display.h          ← page_t, Display_*, Menu_*
    menu.h             ← Menu_Init()
    ds18b20.h          ← DS18B20_*()
    aht20.h            ← aht20_data_t, AHT20_*()
    ens160.h            ← ens160_data_t, ENS160_*()
    ssd1306.h / ssd1306_font.h
  Src/
    main.c             ← application entry, task table, init functions (USER CODE blocks)
    scheduler.c / key.c / sensor.c / alarm.c / display.c
    menu.c / log.c / ds18b20.c / aht20.c / ens160.c
    ssd1306.c / ssd1306_font.c
    stm32f1xx_hal_msp.c / stm32f1xx_it.c / system_stm32f1xx.c
Drivers/
  CMSIS/               ← ARM Cortex-M core headers
  STM32F1xx_HAL_Driver/ ← HAL source (I2C driver may need restore after CubeMX regen)
docs/
  reference.md         ← design guide
stm32_th.ioc          ← CubeMX config (regenerates HAL)
```
