/**
 * Display module — SSD1306 OLED rendering + menu state machine
 *
 * Pages:
 *   PAGE_MAIN  — real-time sensor data (temp, hum, TVOC, eCO2, AQI)
 *   PAGE_ENS   — ENS160 bar chart history (eCO2, TVOC)
 *   PAGE_AHT   — AHT20 bar chart history (temp, humidity)
 *
 * Key mapping:
 *   K1 (PA0) — confirm / enter edit
 *   K2 (PA2) — left  / prev page
 *   K3 (PA3) — right / next page
 */
#include "display.h"
#include "ssd1306.h"
#include "sensor.h"
#include "alarm.h"
#include <stdio.h>
#include <string.h>

#define HISTORY_LEN  20   // number of history samples
#define BAR_MAX_Y    56   // bottom row for bars (64 - 8 for labels)

/* ─── History buffers ─── */
typedef struct {
    float buf[HISTORY_LEN];
    uint8_t head;       // next write position
    uint8_t count;
} history_t;

static history_t s_temp_hist;
static history_t s_hum_hist;
static history_t s_tvoc_hist;
static history_t s_eco2_hist;

static void Hist_Init(history_t *h)
{
    memset(h, 0, sizeof(history_t));
}

static void Hist_Push(history_t *h, float val)
{
    h->buf[h->head] = val;
    h->head = (h->head + 1) % HISTORY_LEN;
    if (h->count < HISTORY_LEN) h->count++;
}

static float Hist_Sample(const history_t *h, uint8_t idx)
{
    // idx=0 is oldest, idx=count-1 is newest
    if (idx >= h->count) return 0;
    uint8_t pos = (h->head + HISTORY_LEN - h->count + idx) % HISTORY_LEN;
    return h->buf[pos];
}

/* ─── Display state ─── */
static page_t s_page = PAGE_MAIN;
static uint8_t s_edit = 0;

/* ─── Drawing helpers ─── */
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

/** Draw a vertical bar chart bar.
 *  x: column (0..127)
 *  value: normalised 0..max_val
 *  max_val: normalisation reference
 *  height: max bar height in pixels */
static void DrawBar(uint8_t x, float value, float max_val, uint8_t height)
{
    uint8_t bar_h = (uint8_t)((value / max_val) * height);
    if (bar_h == 0) bar_h = 1;
    for (uint8_t dy = 0; dy < bar_h; dy++) {
        uint8_t y = BAR_MAX_Y - dy;
        SSD1306_DrawPixel(x,   y, 1);
        SSD1306_DrawPixel(x+1,  y, 1);  // 2px wide bars
    }
}

/* ─── Page renderers ─── */
static void Page_Main(const sensor_data_t *s)
{
    SSD1306_DrawString(0,  0, "== REAL-TIME ==");

    // Temperature
    SSD1306_DrawString(0,  10, "Temp:");
    if (s->aht20_valid) {
        DrawFloat(40, 10, s->temp, 1);
        SSD1306_DrawString(80, 10, "C");
    } else {
        SSD1306_DrawString(40, 10, "--.- C");
    }

    // Humidity
    SSD1306_DrawString(0,  20, "Hum:");
    if (s->aht20_valid) {
        DrawFloat(35, 20, s->humidity, 1);
        SSD1306_DrawString(75, 20, "%");
    } else {
        SSD1306_DrawString(35, 20, "--.- %");
    }

    // TVOC
    SSD1306_DrawString(0,  30, "TVOC:");
    if (s->ens160_valid) DrawUint(35, 30, s->tvoc);
    else SSD1306_DrawString(35, 30, "----");
    SSD1306_DrawString(65, 30, "ppb");

    // eCO2
    SSD1306_DrawString(0,  40, "eCO2:");
    if (s->ens160_valid) DrawUint(35, 40, s->eco2);
    else SSD1306_DrawString(35, 40, "----");
    SSD1306_DrawString(65, 40, "ppm");

    // AQI
    SSD1306_DrawString(0,  50, "AQI:");
    if (s->ens160_valid) DrawUint(30, 50, s->aqil);
    else SSD1306_DrawString(30, 50, "---");

    // Alarm indicator
    if (Alarm_IsActive()) SSD1306_DrawString(90, 50, "**ALM**");
}

static void Page_ENS(const sensor_data_t *s)
{
    SSD1306_DrawString(0,  0, "== ENS160 HIST ==");
    SSD1306_DrawString(0,  8, "eCO2 (ppm)");

    uint8_t n = s_eco2_hist.count;
    uint8_t bars = n > 63 ? 63 : n;  // 2px per bar → max 63 bars in 126px
    for (uint8_t i = 0; i < bars; i++) {
        float val = Hist_Sample(&s_eco2_hist, i);
        uint8_t x = 1 + i * 2;
        DrawBar(x, val, 5000.0f, 28);  // 5000ppm max
    }

    SSD1306_DrawString(0, 38, "TVOC (ppb)");
    for (uint8_t i = 0; i < bars; i++) {
        float val = Hist_Sample(&s_tvoc_hist, i);
        uint8_t x = 1 + i * 2;
        DrawBar(x, val, 1000.0f, 25);  // 1000ppb max
    }
}

static void Page_AHT(const sensor_data_t *s)
{
    SSD1306_DrawString(0,  0, "== AHT20 HIST ==");
    SSD1306_DrawString(0,  8, "Temp (C)");

    uint8_t n = s_temp_hist.count;
    uint8_t bars = n > 63 ? 63 : n;
    for (uint8_t i = 0; i < bars; i++) {
        float val = Hist_Sample(&s_temp_hist, i);
        uint8_t x = 1 + i * 2;
        DrawBar(x, val, 50.0f, 28);  // 50°C max
    }

    SSD1306_DrawString(0, 38, "Humidity (%)");
    for (uint8_t i = 0; i < bars; i++) {
        float val = Hist_Sample(&s_hum_hist, i);
        uint8_t x = 1 + i * 2;
        DrawBar(x, val, 100.0f, 25);  // 100% max
    }
}

/* ─── Public API ─── */
void Display_Init(void)
{
    SSD1306_Init();
    Hist_Init(&s_temp_hist);
    Hist_Init(&s_hum_hist);
    Hist_Init(&s_tvoc_hist);
    Hist_Init(&s_eco2_hist);
}

void Display_Refresh(void)
{
    sensor_data_t s = Sensor_GetData();

    SSD1306_Clear();

    switch (s_page) {
    case PAGE_MAIN: Page_Main(&s); break;
    case PAGE_ENS:  Page_ENS(&s);  break;
    case PAGE_AHT:   Page_AHT(&s);  break;
    default: break;
    }

    SSD1306_Update();
}

/** Called every 100ms to update history.
 *  Call this from the 100ms display task. */
void Display_UpdateHistory(const sensor_data_t *s)
{
    if (s->aht20_valid) {
        Hist_Push(&s_temp_hist, s->temp);
        Hist_Push(&s_hum_hist, s->humidity);
    }
    if (s->ens160_valid) {
        Hist_Push(&s_tvoc_hist, (float)s->tvoc);
        Hist_Push(&s_eco2_hist, (float)s->eco2);
    }
}

page_t Menu_GetPage(void) { return s_page; }

/**
 * K1 — confirm / enter edit
 * K2 — left (previous page)
 * K3 — right (next page)
 */
void Menu_OnKey(key_id_t key_id, key_evt_t evt)
{
    if (evt == KEY_EVT_NONE) return;

    if (key_id == KEY_ID_K2 && evt == KEY_EVT_SHORT) {
        // K2: left → previous page
        s_page = (page_t)((s_page + PAGE_COUNT - 1) % PAGE_COUNT);
        s_edit = 0;
        Display_Refresh();
        return;
    }

    if (key_id == KEY_ID_K3 && evt == KEY_EVT_SHORT) {
        // K3: right → next page
        s_page = (page_t)((s_page + 1) % PAGE_COUNT);
        s_edit = 0;
        Display_Refresh();
        return;
    }

    if (s_edit) {
        if (key_id == KEY_ID_K1 && evt == KEY_EVT_SHORT) {
            s_edit = 0;
            Alarm_SaveThresholds();
            Display_Refresh();
        } else if (key_id == KEY_ID_K2 && evt == KEY_EVT_SHORT) {
            if (s_page == PAGE_MAIN) Alarm_AdjustHi(+0.5f);
        } else if (key_id == KEY_ID_K3 && evt == KEY_EVT_SHORT) {
            if (s_page == PAGE_MAIN) Alarm_AdjustLo(-0.5f);
        }
        (void)Display_Refresh;
        return;
    }

    if (key_id == KEY_ID_K1 && evt == KEY_EVT_LONG) {
        if (s_page == PAGE_MAIN) {
            s_edit = 1;
            Display_Refresh();
        }
    }
}
