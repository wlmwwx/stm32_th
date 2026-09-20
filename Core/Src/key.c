#include "key.h"

#define DEBOUNCE_MS   20
#define LONG_PRESS_MS 1000

void Key_Init(key_t *k, GPIO_TypeDef *port, uint16_t pin, GPIO_PinState press_level)
{
    k->port = port;
    k->pin = pin;
    k->press_level = press_level;
    k->state = KS_RELEASED;
    k->timer_ms = 0;
    k->long_fired = 0;
    k->evt = KEY_EVT_NONE;
}

void Key_Scan10ms(key_t *k)
{
    uint8_t pressed_now = (HAL_GPIO_ReadPin(k->port, k->pin) == k->press_level);

    switch (k->state) {
    case KS_RELEASED:
        if (pressed_now) {
            k->state = KS_DEBOUNCE;
            k->timer_ms = 0;
        }
        break;

    case KS_DEBOUNCE:
        if (!pressed_now) {
            k->state = KS_RELEASED;
            break;
        }
        k->timer_ms += 10;
        if (k->timer_ms >= DEBOUNCE_MS) {
            k->state = KS_PRESSED;
            k->timer_ms = 0;
            k->long_fired = 0;
            k->evt = KEY_EVT_SHORT;
        }
        break;

    case KS_PRESSED:
        if (!pressed_now) {
            k->state = KS_RELEASED;
            break;
        }
        k->timer_ms += 10;
        if (!k->long_fired && k->timer_ms >= LONG_PRESS_MS) {
            k->long_fired = 1;
            k->evt = KEY_EVT_LONG;
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

uint8_t Key_IsPressed(const key_t *k)
{
    return k->state == KS_PRESSED;
}
