# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal STM32F103C8T6 temperature monitoring project (Blue Pill board). Uses STM32CubeMX-generated HAL code with a cooperative-scheduler architecture. The application logic is implemented as described in `docs/reference.md`.

## Build

- **IDE**: STM32CubeIDE (or arm-none-eabi-gcc + Make)
- **IOC**: `stm32_th.ioc` — open in STM32CubeMX to regenerate HAL code after changes
- **Linker script**: `STM32F103C8TX_FLASH.ld`
- **Float printf**: linker flag `-u _printf_float` is set in `.cproject` (required for `printf("%.1f")`)

## Hardware Configuration

| Peripheral | Pin/Config |
|---|---|
| LED | PC13, GPIO_Output (on=low) |
| Button (KEY) | PA0, GPIO_Input, Pull-down, press=high |
| DS18B20 (temp sensor) | PA1, GPIO_Output Open-Drain (one-wire) |
| UART1 (log) | USART1, 115200, PA9/PA10 |
| UART3 (aux) | USART3, 115200, PB10/PB11 |
| Buzzer PWM | TIM3 CH1, PA6, 2kHz, 50% duty |
| OLED display | I2C1, PB6=SCL, PB7=SDA, 100kHz, SSD1306 128x64 |
| Clock | HSE 8MHz → PLL ×9 → 72MHz SYSCLK, APB1=36MHz, APB2=72MHz |

## Architecture

Periodic task scheduler using `HAL_GetTick()` timestamps — no blocking delays:

```
while(1) → Scheduler_Run() → checks task table
    ├── 10ms:  Key scan → Menu event
    ├── 100ms: Display refresh, Alarm check, Temp read check
    ├── 1000ms: Start DS18B20 conversion
    └── 2000ms: UART log report
```

## Module List

| File | Purpose |
|---|---|
| `Core/Src/scheduler.c` | Task table runner |
| `Core/Src/key.c` | Button state machine (RELEASED→DEBOUNCE→PRESSED) |
| `Core/Src/sensor.c` | DS18B20 non-blocking read ("time as state") |
| `Core/Src/ds18b20.c` | One-wire bit-banging driver |
| `Core/Src/alarm.c` | Hysteresis alarm + buzzer PWM divider |
| `Core/Src/display.c` | Menu state machine + SSD1306 rendering |
| `Core/Src/ssd1306.c` | Minimal SSD1306 I2C driver |
| `Core/Src/ssd1306_font.c` | 6×8 bitmap font (ASCII 0x20–0x7F) |
| `Core/Src/log.c` | `printf` redirect to UART1 + periodic report |
| `Core/Src/main.c` | Clock init, GPIO init, task table, scheduler loop |

## Key Design Patterns

- **Scheduler**: `task_t` struct with `last_run`, `period_ms`, `(*func)()`. `Scheduler_Run()` uses timestamp comparison — no `HAL_Delay()`.
- **Key state machine**: 20ms debounce, 1000ms long-press, non-blocking `Key_Scan10ms()`.
- **DS18B20 non-blocking**: `Sensor_TaskStartConvert()` records `HAL_GetTick()`, `Sensor_TaskReadResult()` reads 800ms later.
- **Alarm hysteresis**: on at `th_hi`, off at `th_hi - HYSTERESIS` (1°C deadzone).
- **Buzzer PWM**: `HAL_TIM_PWM_Start/Stop()` from 100ms task with tick counter (300ms on / 200ms off).
- **OLED**: framebuffer in RAM, `SSD1306_Update()` sends full display via I2C.

## Common Pitfalls

1. **DS18B20 reads 85°C**: one-wire bit timing broken by SysTick. `ds18b20.c` wraps byte ops with `__disable_irq().__enable_irq()`.
2. **`printf("%.1f")` shows blank**: missing `-u _printf_float` in linker flags — already set in `.cproject`.
3. **ISRs calling blocking functions**: `HAL_Delay()` and `printf()` must never be called from ISR context.
4. **Alarm chattering**: missing hysteresis — `alarm.c` uses separate on/off thresholds.
5. **Button press direction**: IOC has PA0 Pull-down (press=high). If button is active-low on your board, change to Pull-up and pass `GPIO_PIN_RESET` to `Key_Init()`.

## File Structure

```
Core/
  Inc/
    main.h              ← externs for hi2c1, huart1, huart3, htim3
    scheduler.h         ← task_t, Scheduler_Run()
    key.h              ← key_t, key_evt_t, Key_*()
    sensor.h           ← Sensor_*()
    alarm.h            ← Alarm_*()
    display.h          ← page_t, Display_*, Menu_*
    menu.h             ← Menu_Init()
    ds18b20.h          ← DS18B20_*()
    ssd1306.h / ssd1306_font.h
  Src/
    main.c             ← application entry, task table, init functions
    scheduler.c / key.c / sensor.c / alarm.c / display.c
    menu.c / log.c / ds18b20.c
    ssd1306.c / ssd1306_font.c
    stm32f1xx_hal_msp.c / stm32f1xx_it.c / system_stm32f1xx.c
Drivers/
  CMSIS/               ← ARM Cortex-M core headers
  STM32F1xx_HAL_Driver/ ← HAL source
docs/
  reference.md         ← design guide (read this first)
stm32_th.ioc          ← CubeMX config (regenerates HAL)
```
