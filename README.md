# stm32_th — Multi-Sensor Environmental Monitor

Temperature, humidity, and air quality monitor built on a Blue Pill (STM32F103C8T6) with bare-metal cooperative scheduler.

## Features

- **Temperature** — DS18B20 (one-wire) + AHT20 (I2C)
- **Humidity** — AHT20 (I2C)
- **Air Quality** — ENS160 (TVOC, eCO₂, AQI)
- **Display** — SSD1306 OLED 128×64 (I2C)
- **Alert** — Temperature alarm with buzzer and hysteresis
- **Logging** — Serial output over UART1 (115200 baud)
- **Heartbeat** — LED blink every 500ms

## Hardware

| Module | Interface | Address/Pin |
|---|---|---|
| SSD1306 OLED | I2C1 | 0x78 / PB6=SDL, PB7=SDA |
| AHT20 | I2C1 | 0x38 / same bus |
| ENS160 | I2C1 | 0x53 / same bus |
| DS18B20 | GPIO | PA1 (open-drain) |
| LED | GPIO | PC13 |
| Button | GPIO | PA0 (Pull-down) |
| Buzzer | TIM3 CH1 | PA6 |
| UART1 | USART1 | PA9=TX, PA10=RX |

## Build

1. Open `stm32_th.ioc` in **STM32CubeMX** and regenerate code if needed
2. Open the project in **STM32CubeIDE**
3. Build and flash to device

Or with arm-none-eabi-gcc:
```bash
make -j8
```

## Wiring

```
Blue Pill          OLED / Sensors (all I2C)
────────          ──────────────────────
PB6 (SCL)  ────  PB6 (SCL)
PB7 (SDA)  ────  PB7 (SDA)
              ────  AHT20  SCL/SDA
              ────  ENS160 SCL/SDA

PA1        ────  DS18B20 DQ (with 4.7kΩ pull-up to 3.3V)

PA9 (TX)   ────  USB-UART RX (for serial log)
PA10 (RX)  ────  USB-UART TX

PA6        ────  Buzzer (+)
PC13       ────  LED (built-in)
PA0        ────  Button (+)
```

## Serial Output

Connect PA9 to USB-UART TX at 115200 baud 8N1:

```
T:25.3,HI:30.0,LO:5.0,ALM:0
```

Format: `T:<temp>,HI:<hi>,LO:<lo>,ALM:<0|1>`

## Architecture

Cooperative scheduler — no RTOS, no blocking delays:

| Period | Task |
|---|---|
| 10ms | Key scan → menu |
| 100ms | Display refresh |
| 100ms | Sensor read |
| 100ms | Alarm + buzzer |
| 500ms | LED toggle |
| 1000ms | Sensor trigger |
| 2000ms | Serial report |

Each sensor uses **trigger → wait → read** to avoid blocking. Sensor state is stored in `sensor.c` and queried by display/log/alarm modules independently.

## Button

- **Short press** — cycle through pages: MAIN → HI → LO
- **Long press** — enter edit mode (on HI/LO page)
- **Short press (edit)** — ±0.5°C
- **Long press (edit)** — save and exit
