

## 一、整体架构：前后台 + 协作式调度

裸机没有操作系统，本质上就是一个大 `while(1)` 循环（后台），加中断（前台）。我们在此基础上做一层**按周期调度的任务表**：

```
        ┌─────────────────────────────┐
        │  SysTick 中断（HAL 自带 1ms） │
        └──────────────┬──────────────┘
                       │ 只更新计数
        ┌──────────────▼──────────────┐
while(1)│  调度器：检查各任务是否到期    │ ← 主循环
        │  ┌─────┬─────┬─────┬─────┐ │
        │  │10ms │100ms│500ms│2000ms│ │  ← 按键扫描/显示/传感器/上报
        │  └─────┴─────┴─────┴─────┘ │
        └─────────────────────────────┘
```

**为什么这样做**：如果按初学者写法（`while(1){ 读温度; HAL_Delay(1000); 刷屏; ... }`），整个循环周期不可控，按键响应迟钝、报警延迟。任务表让每个功能的周期各自独立、互不等。

### 调度器代码（核心框架，先建立这个）

```c
/* scheduler.h */
#ifndef __SCHED_H
#define __SCHED_H
#include "main.h"

typedef struct {
    uint32_t last_run;
    uint32_t period_ms;
    void (*func)(void);
} task_t;

#define TASK_COUNT (sizeof(s_tasks)/sizeof(s_tasks[0]))

extern task_t s_tasks[];   // 定义在 main.c

static inline void Scheduler_Run(void)
{
    uint32_t now = HAL_GetTick();          // HAL 的 1ms 节拍，直接用
    for (uint32_t i = 0; i < TASK_COUNT; i++) {
        if (now - s_tasks[i].last_run >= s_tasks[i].period_ms) {
            s_tasks[i].last_run = now;
            s_tasks[i].func();
        }
    }
}
#endif
```

这里 `HAL_GetTick()` 就是 SysTick 中断维护的毫秒计数，框架已经帮你做好了"前台"，你只管在后台轮询。

**局限也要知道**：这种调度没有优先级，某个任务跑太久会拖延后面的任务。这个项目所有任务都是微秒级完成，所以没问题——以后学 FreeRTOS 就是在解决这个问题。

## 二、按键状态机（本项目的教学核心）

按键处理的经典错误写法：

```c
// ❌ 错误示范
if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
    HAL_Delay(20);                       // 阻塞整个系统 20ms！
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
        // 处理按键
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0));  // 更糟：死等松开
    }
}
```

错误在于：消抖靠阻塞延时，按住不放时整个系统停摆。正确做法是**状态机 + 周期采样**——每 10ms 看一眼引脚，靠"状态 + 计时"判断，全程无阻塞。

### 状态转移图

```
        读到按下电平          持续20ms仍按下
RELEASED ──────────► DEBOUNCE ──────────────► PRESSED
   ▲                    │ 期间读到松开            │ 期间读到松开
   │                    ▼                       │ 持续1s未松开
   └────────────────────┴───────────────────────┤ 发出长按事件
                                                ▼
                                          （保持到松开回 RELEASED）
```

### 完整代码

```c
/* key.h */
#ifndef __KEY_H
#define __KEY_H
#include "main.h"

typedef enum { KEY_EVT_NONE = 0, KEY_EVT_SHORT, KEY_EVT_LONG } key_evt_t;

typedef enum { KS_RELEASED = 0, KS_DEBOUNCE, KS_PRESSED } key_state_t;

typedef struct {
    GPIO_TypeDef   *port;
    uint16_t        pin;
    GPIO_PinState   press_level;  // 按下时的电平（按原理图定，Blue Pill 常见 PA0 按下为高）
    key_state_t     state;
    uint16_t        timer_ms;     // 当前状态已持续的时间
    uint8_t         long_fired;   // 长按事件是否已发出（防止长按期间重复触发）
    key_evt_t       evt;          // 对外输出的事件
} key_t;

void      Key_Init(key_t *k, GPIO_TypeDef *port, uint16_t pin, GPIO_PinState press_level);
void      Key_Scan10ms(key_t *k);        // 调度器每 10ms 调一次
key_evt_t Key_GetEvent(key_t *k);        // 取走事件（取后清空）
uint8_t   Key_IsPressed(const key_t *k); // 查询"当前是否按住"（编辑模式连发用）
#endif
```

```c
/* key.c */
#include "key.h"

#define DEBOUNCE_MS    20
#define LONG_PRESS_MS  1000

void Key_Init(key_t *k, GPIO_TypeDef *port, uint16_t pin, GPIO_PinState press_level)
{
    k->port = port;  k->pin = pin;  k->press_level = press_level;
    k->state = KS_RELEASED;  k->timer_ms = 0;
    k->long_fired = 0;  k->evt = KEY_EVT_NONE;
}

void Key_Scan10ms(key_t *k)
{
    uint8_t pressed_now = (HAL_GPIO_ReadPin(k->port, k->pin) == k->press_level);

    switch (k->state) {
    case KS_RELEASED:
        if (pressed_now) { k->state = KS_DEBOUNCE; k->timer_ms = 0; }
        break;

    case KS_DEBOUNCE:                     // 消抖期：给抖动一个"证明自己"的机会
        if (!pressed_now) { k->state = KS_RELEASED; break; }
        k->timer_ms += 10;
        if (k->timer_ms >= DEBOUNCE_MS) { // 持续 20ms 都是按下 → 确认
            k->state = KS_PRESSED;
            k->timer_ms = 0;
            k->long_fired = 0;
            k->evt = KEY_EVT_SHORT;       // 设计选择：确认按下即发短按
        }
        break;

    case KS_PRESSED:
        if (!pressed_now) { k->state = KS_RELEASED; break; }
        k->timer_ms += 10;
        if (!k->long_fired && k->timer_ms >= LONG_PRESS_MS) {
            k->long_fired = 1;
            k->evt = KEY_EVT_LONG;        // 长按只发一次
        }
        break;
    }
}

key_evt_t Key_GetEvent(key_t *k)
{
    key_evt_t e = k->evt;
    k->evt = KEY_EVT_NONE;
    return e;
}

uint8_t Key_IsPressed(const key_t *k) { return k->state == KS_PRESSED; }
```

**三个值得品味的设计决策**：

1. **事件在状态转移点产生**，而不是读到电平就产生——电平是物理现实（会抖），事件是逻辑抽象（已确认）。
2. **`long_fired` 标志**防止长按期间每 10ms 重复发事件。
3. **当前设计是"按下即发短按"**。如果想区分"单击"和"长按"（常见需求：按下别急着报，松开时若没触发过长按才算单击），就把 `KEY_EVT_SHORT` 挪到 RELEASED 转移处、且 `long_fired==0` 时才发。这是状态机改写练习的好题目。

## 三、菜单状态机（第二个状态机）

界面需要管理"当前在哪页、是否在编辑"，这又是一个状态机（确切说是两层：页码 × 模式）：

```c
/* menu.c */
#include "key.h"
#include "display.h"
#include "alarm.h"

typedef enum { PAGE_MAIN = 0, PAGE_SET_HI, PAGE_SET_LO, PAGE_COUNT } page_t;

static page_t s_page = PAGE_MAIN;
static uint8_t s_edit = 0;   // 0=浏览 1=编辑

void Menu_OnKey(key_evt_t evt)
{
    if (s_edit) {                                    // ── 编辑模式 ──
        switch (evt) {
        case KEY_EVT_SHORT:
            if (s_page == PAGE_SET_HI) Alarm_AdjustHi(+0.5f);   // 短按=加
            else                       Alarm_AdjustLo(+0.5f);
            Display_Refresh();                              // 数值变了立刻刷新
            break;
        case KEY_EVT_LONG:
            s_edit = 0;                                     // 长按=保存退出
            Alarm_SaveThresholds();                         // 可先做 RAM 版，再升级 Flash 存储
            Display_Refresh();
            break;
        default: break;
        }
        return;
    }

    switch (evt) {                                   // ── 浏览模式 ──
    case KEY_EVT_SHORT:
        s_page = (page_t)((s_page + 1) % PAGE_COUNT);       // 短按=翻页
        Display_Refresh();
        break;
    case KEY_EVT_LONG:
        if (s_page != PAGE_MAIN) s_edit = 1;               // 长按=进入编辑
        break;
    default: break;
    }
}

page_t Menu_GetPage(void) { return s_page; }
uint8_t  Menu_IsEditing(void) { return s_edit; }
```

**加分项**：进入编辑模式后，结合 `Key_IsPressed()` 可以实现"按住不放连续加减"——按住超过 500ms 后每 100ms 加一次。这样只需要一个按键就能完成全部设置，体会状态机组合的威力。

## 四、传感器：非阻塞读取（容易被忽视的难点）

DS18B20 有个特性：**发起温度转换后，需要最多 750ms 才能读出结果**。阻塞写法是 `StartConvert(); HAL_Delay(800); Read();`——又阻塞。正确做法是拆成两个不同时刻的任务：

```c
/* sensor.c */
#include "ds18b20.h"   // 底层位操作驱动，移植现成代码即可，后面讲坑

static float    s_temp = 0.0f;
static uint32_t s_convert_start = 0;

void Sensor_TaskStartConvert(void)          // 每 1000ms 调用
{
    DS18B20_StartConvert();
    s_convert_start = HAL_GetTick();
}

void Sensor_TaskReadResult(void)            // 每 100ms 调用，到了时间才真读
{
    if (s_convert_start != 0 &&
        HAL_GetTick() - s_convert_start >= 800) {
        s_temp = DS18B20_ReadTemp();
        s_convert_start = 0;                // 标记已取，避免重复读
    }
}

float Sensor_GetTemp(void) { return s_temp; }
```

这就是所谓**"把时间当状态用"**：用一个时间戳记住"我在等"，而不是"停在那里等"。所有异步外设（包括以后接触的网络、Flash 擦写）都是这个模式。

## 五、报警：滞回 + 非阻塞蜂鸣

### 滞回（hysteresis）——必须掌握的概念

如果阈值设 30°C，写法是 `if (t > 30) alarm=1; else alarm=0;`，那么温度在 29.9 和 30.0 之间波动时，蜂鸣器会疯狂抖动。解决方法是开和关用不同的阈值，中间留一段"死区"：

```c
/* alarm.c */
#include "sensor.h"

#define HYSTERESIS 1.0f    // 滞回死区

static float s_th_hi = 30.0f, s_th_lo = 5.0f;
static uint8_t s_alarm = 0;

void Alarm_Task100ms(void)
{
    float t = Sensor_GetTemp();
    if (!s_alarm && t >= s_th_hi)                s_alarm = 1;   // 超上限→开
    else if (s_alarm && t <= s_th_hi - HYSTERESIS) s_alarm = 0; // 回落 1 度才关
    // 下限同理
}
```

### 蜂鸣节奏：响 300ms、停 200ms 的"哔-哔-"声

也不能用 `while(1){ beep; delay; }` 阻塞，继续用计数分频：

```c
void Alarm_BuzzerTask100ms(void)
{
    static uint8_t tick = 0;
    if (!s_alarm) {
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
        tick = 0;
        return;
    }
    if (++tick >= 5) tick = 0;          // 500ms 一个周期
    if (tick < 3)                       // 周期内前 300ms 响
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    else
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
}
```

PWM 参数（TIM3 CH1 → PA6）：72MHz / (PSC=71) = 1MHz 计数，ARR=500 → 2kHz 方波，比较值 250 即 50% 占空比。CubeMX 里勾 TIM3 → Channel1 → PWM Generation CH1，代码里 `__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 250)`。

## 六、串口：printf 重定向 + 周期上报

```c
/* log.c */
#include <stdio.h>

int _write(int fd, char *ptr, int len)   // newlib 的底层写函数（GCC/CubeIDE）
{
    (void)fd;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, 100);
    return len;
}

void Log_TaskReport(void)                 // 每 2000ms
{
    printf("T:%.1f,HI:%.1f,LO:%.1f,ALM:%d\r\n",
           Sensor_GetTemp(), Alarm_GetHiTh(), Alarm_GetLoTh(), Alarm_IsActive());
}
```

**CubeIDE 的经典坑**：默认链接的 newlib 不含浮点 printf，`%.1f` 会输出乱码或空白。解决：Project → Properties → C/C++ Build → Settings → MCU GCC Linker → 勾选 **"Use float with printf"**（即加 `-u _printf_float`）。

（此处在低频率、短帧场景用阻塞发送可接受。数据量大时应升级为 UART + DMA + 环形缓冲区发送，那是进阶练习。）

## 七、组装：main 里把它们串起来

```c
/* main.c 的用户代码区（CubeMX 生成代码的 USER CODE 段内） */
#include "scheduler.h"
#include "key.h"
#include "sensor.h"
#include "alarm.h"
#include "menu.h"
#include "log.h"
#include "display.h"

key_t g_key;

void Task_KeyScan(void)  { Key_Scan10ms(&g_key);  Menu_OnKey(Key_GetEvent(&g_key)); }
void Task_Display(void)  { Display_Task(); }            // 内部判断是否需要刷新
void Task_StartConv(void){ Sensor_TaskStartConvert(); }
void Task_ReadTemp(void) { Sensor_TaskReadResult(); }
void Task_Alarm(void)    { Alarm_Task100ms(); Alarm_BuzzerTask100ms(); }
void Task_Log(void)      { Log_TaskReport(); }

task_t s_tasks[] = {
    { 0,   10,  Task_KeyScan    },   // 10ms:  按键
    { 0,   100, Task_Display    },   // 100ms: 显示（OLED 慢，不用更勤）
    { 0,   1000,Task_StartConv  },   // 1s:    发起温度转换
    { 0,   100, Task_ReadTemp   },   // 100ms: 检查转换是否完成
    { 0,   100, Task_Alarm      },   // 100ms: 报警判断+蜂鸣
    { 0,   2000,Task_Log        },   // 2s:    串口上报
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM3_Init();
    MX_I2C1_Init();
    /* USER CODE BEGIN 2 */
    Display_Init();
    Key_Init(&g_key, KEY_GPIO_Port, KEY_Pin, GPIO_PIN_SET);  // PA0 按下为高
    /* USER CODE END 2 */
    while (1) { Scheduler_Run(); }
}
```

**CubeMX 配置清单**（勾完生成代码）：

| 功能 | 配置 |
|---|---|
| LED | PC13 → GPIO_Output |
| 按键 | PA0 → GPIO_Input（Pull-down，若你的板子是按下接地则 Pull-up） |
| 串口 | USART1 → Asynchronous，115200，PA9/PA10 |
| 蜂鸣器 | TIM3 → CH1 → PWM Generation，PA6 |
| OLED | I2C1 → PB6/PB7，100kHz（用现成 SSD1306 驱动库） |
| 时钟 | HSE 8MHz → PLL → 72MHz |
| ds18b20 | Pa1|

OLED 和 DS18B20 的底层驱动直接移植成熟代码（SSD1306 用 U8g2 或精简版、DS18B20 参考正点原子/江协），**学习重点不在这里**——重点是理解上面这些架构代码。

## 八、这个项目里最常见的 5 个坑

1. **DS18B20 读数全是 85°C 或乱码**：位操作的微秒级时序被 SysTick 中断打断了。在读写字节函数外面包 `__disable_irq(); ... __enable_irq();`。（为何整个字节操作期间关中断而不是一位？这是值得想清楚的思考题。）
2. **`printf` 浮点输出空白**：没勾 `-u _printf_float`。
3. **中断回调里调用 `HAL_Delay()` 或 `printf()`**：`HAL_Delay` 依赖 SysTick 中断，中断优先级不低于 SysTick 时会**死锁**。中断里只置标志位，处理放主循环——这正是本项目架构做的事。
4. **按键事件丢失**：两次扫描之间产生了新事件把旧的覆盖了。本项目按键事件消费频率（10ms）远高于产生频率，安全；若任务更重，应改环形缓冲队列。
5. **报警抖动**：没做滞回，蜂鸣器随温度在阈值附近频繁通断。

---

**建议的动手顺序**：先把调度器骨架 + 按键状态机跑起来（只用串口 printf 验证事件，不接屏），再加传感器，最后加显示和报警。每加一个模块前，先想清楚它放进任务表的周期是多少、是否阻塞——这个思考过程本身就是裸机架构能力的训练。

做到这里，你的代码里已经有了状态机、软定时、事件驱动、滞回这些"内功"，再去学 FreeRTOS 时，任务、信号量、队列这些概念对你来说就只是"把手动管理换成现成机制"而已。有任何模块想再往深挖（比如 Flash 存储阈值、环形缓冲串口），随时问。
