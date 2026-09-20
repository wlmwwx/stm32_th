#ifndef __KEY_H
#define __KEY_H
#include "main.h"

typedef enum { KEY_EVT_NONE = 0, KEY_EVT_SHORT, KEY_EVT_LONG } key_evt_t;
typedef enum { KS_RELEASED = 0, KS_DEBOUNCE, KS_PRESSED } key_state_t;

typedef enum { KEY_ID_K1 = 0, KEY_ID_K2, KEY_ID_K3, KEY_COUNT } key_id_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState press_level;
    key_state_t state;
    uint16_t timer_ms;
    uint8_t long_fired;
    key_evt_t evt;
} key_t;

void Key_Init(key_t *k, GPIO_TypeDef *port, uint16_t pin, GPIO_PinState press_level);
void Key_Scan10ms(key_t *k);
key_evt_t Key_GetEvent(key_t *k);
uint8_t Key_IsPressed(key_id_t id);
uint8_t Key_IsPressedAny(void);

#endif
