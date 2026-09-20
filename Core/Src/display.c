/**
 * Display module — SSD1306 OLED rendering + menu state machine
 * Pages: MAIN (all sensors), PAGE_SET_HI, PAGE_SET_LO
 */
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

static void DrawFloat(uint8_t x, uint8_t y, float val, uint8_t decimals)
{
    char buf[16];
    if (decimals == 1) snprintf(buf, sizeof(buf), "%.1f", val);
    else if (decimals == 0) snprintf(buf, sizeof(buf), "%.0f", val);
    else snprintf(buf, sizeof(buf), "%.2f", val);
    SSD1306_DrawString(x, y, buf);
}

static void DrawUint(uint8_t x, uint8_t y, uint16_t val)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    SSD1306_DrawString(x, y, buf);
}

void Display_Refresh(void)
{
    SSD1306_Clear();
    sensor_data_t s = Sensor_GetData();

    switch (s_page) {
    case PAGE_MAIN: {
        SSD1306_DrawString(0, 0,  "--- SENSOR DATA ---");

        // Temperature
        SSD1306_DrawString(0, 10, "Temp:");
        if (s.aht20_valid) {
            DrawFloat(40, 10, s.temp, 1);
            SSD1306_DrawString(80, 10, "C");
        } else {
            SSD1306_DrawString(40, 10, "--.- C");
        }

        // Humidity
        SSD1306_DrawString(0, 20, "RH:");
        if (s.aht20_valid) {
            DrawFloat(30, 20, s.humidity, 1);
            SSD1306_DrawString(70, 20, "%");
        } else {
            SSD1306_DrawString(30, 20, "--.- %");
        }

        // TVOC
        SSD1306_DrawString(0, 30, "TVOC:");
        if (s.ens160_valid) {
            DrawUint(35, 30, s.tvoc);
            SSD1306_DrawString(65, 30, "ppb");
        } else {
            SSD1306_DrawString(35, 30, "---- ppb");
        }

        // eCO2
        SSD1306_DrawString(0, 40, "eCO2:");
        if (s.ens160_valid) {
            DrawUint(35, 40, s.eco2);
            SSD1306_DrawString(65, 40, "ppm");
        } else {
            SSD1306_DrawString(35, 40, "---- ppm");
        }

        // AQI
        SSD1306_DrawString(0, 50, "AQI:");
        if (s.ens160_valid) {
            DrawUint(30, 50, s.aqil);
        } else {
            SSD1306_DrawString(30, 50, "---");
        }

        // Alarm indicator
        if (Alarm_IsActive()) {
            SSD1306_DrawString(90, 50, "ALM!");
        }

        break;
    }
    case PAGE_SET_HI: {
        SSD1306_DrawString(0, 0,  "Set HIGH Limit");
        SSD1306_DrawString(0, 20, "HI =");
        DrawFloat(35, 20, Alarm_GetHiTh(), 1);
        SSD1306_DrawString(75, 20, "C");
        SSD1306_DrawString(0, 40, s_edit ? "[EDITING]" : "browse only");
        SSD1306_DrawString(0, 50, "SHORT:+-  LONG:save");
        break;
    }
    case PAGE_SET_LO: {
        SSD1306_DrawString(0, 0,  "Set LOW Limit");
        SSD1306_DrawString(0, 20, "LO =");
        DrawFloat(35, 20, Alarm_GetLoTh(), 1);
        SSD1306_DrawString(75, 20, "C");
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
