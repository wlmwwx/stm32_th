#include "display.h"
#include "ssd1306.h"
#include "sensor.h"
#include "alarm.h"
#include <stdio.h>

static page_t s_page = PAGE_MAIN;
static uint8_t s_edit = 0;

void Display_Init(void)
{
    SSD1306_Init();
}

static void DrawFloat(uint8_t x, uint8_t y, float val)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", val);
    SSD1306_DrawString(x, y, buf);
}

void Display_Refresh(void)
{
    SSD1306_Clear();

    switch (s_page) {
    case PAGE_MAIN: {
        float t = Sensor_GetTemp();
        uint8_t alm = Alarm_IsActive();

        // Temperature big display — use pixel drawing for large font
        // 48px font would be ideal; use 6x8 stacked for now
        SSD1306_DrawString(0, 0, "Temp:");
        DrawFloat(40, 0, t);

        SSD1306_DrawString(0, 10, alm ? "** ALARM **" : "  Normal   ");
        SSD1306_DrawString(0, 20, "HI:");
        DrawFloat(20, 20, Alarm_GetHiTh());
        SSD1306_DrawString(70, 20, "LO:");
        DrawFloat(90, 20, Alarm_GetLoTh());

        SSD1306_DrawString(0, 30, "--- MENU ---");
        SSD1306_DrawString(0, 40, "1:MAIN 2:HI 3:LO");
        SSD1306_DrawString(0, 50, "LONG press:set");
        break;
    }
    case PAGE_SET_HI: {
        float hi = Alarm_GetHiTh();
        SSD1306_DrawString(0, 0, "Set HIGH Limit");
        SSD1306_DrawString(0, 20, "HI =");
        DrawFloat(40, 20, hi);
        SSD1306_DrawString(0, 40, s_edit ? "[EDITING]" : "browse only");
        SSD1306_DrawString(0, 50, "SHORT:+-  LONG:save");
        break;
    }
    case PAGE_SET_LO: {
        float lo = Alarm_GetLoTh();
        SSD1306_DrawString(0, 0, "Set LOW Limit");
        SSD1306_DrawString(0, 20, "LO =");
        DrawFloat(40, 20, lo);
        SSD1306_DrawString(0, 40, s_edit ? "[EDITING]" : "browse only");
        SSD1306_DrawString(0, 50, "SHORT:+-  LONG:save");
        break;
    }
    default:
        break;
    }

    SSD1306_Update();
}

page_t Menu_GetPage(void) { return s_page; }
uint8_t Menu_IsEditing(void) { return s_edit; }

void Menu_OnKey(key_evt_t evt)
{
    if (s_edit) {
        switch (evt) {
        case KEY_EVT_SHORT:
            if (s_page == PAGE_SET_HI) Alarm_AdjustHi(+0.5f);
            else                       Alarm_AdjustLo(+0.5f);
            Display_Refresh();
            break;
        case KEY_EVT_LONG:
            s_edit = 0;
            Alarm_SaveThresholds();
            Display_Refresh();
            break;
        default:
            break;
        }
        return;
    }

    switch (evt) {
    case KEY_EVT_SHORT:
        s_page = (page_t)((s_page + 1) % PAGE_COUNT);
        Display_Refresh();
        break;
    case KEY_EVT_LONG:
        if (s_page != PAGE_MAIN) s_edit = 1;
        break;
    default:
        break;
    }
}
