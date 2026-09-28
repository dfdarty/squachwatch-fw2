// SquachWatch on the FREE-WILi 2: the board.
//
// Runs SquachWatch's own setup() and loop() (src/main.cpp, unchanged) on the
// FW2's display processor, and supplies what an ESP32 board would:
//  - the screen: SquachWatch draws with TFT_eSPI into the simulator's
//    in-memory panel (sim/TFT_eSPI.h) at 480x320, the resolution of its
//    3.5" CYD build and of the FW2's ST7796; after every loop() the rows
//    that changed go to the LCD by DMA;
//  - touch: the FW2's capacitive panel, handed to SquachWatch's XPT2046
//    code as the raw readings its factory calibration maps back to the same
//    point (as the simulator's live harness does);
//  - the grey, yellow and green buttons press SCAN, LOG and DESK on the
//    button bar;
//  - the clock from the FW2's real-time clock, and back to it when
//    SquachWatch learns the time;
//  - Serial to the FW2's diagnostics log; the heap in PSRAM; the SD card
//    and radios over WiliBSP's OneWili link (sd_fw2.cpp, radio_fw2.cpp).
// HOME held for 5 s leaves, PAGE held for 5 s shows the About screen, as in
// every WiliBSP app.
extern "C" {
#include "fw2.h"
#include "platform/diag.h"
#include "platform/psram.h"
#include "onewili.h"
#include "onewili_fwgui.h"
#include "onewili_sd.h"
#include "input/app_recovery_onewili.h"
}
#include "pico/stdlib.h"
#include "pico/rand.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "sim_touch.h"
#include "theme.h"
#include "clock.h"
#include "state.h"
#include "fw2_state_names.h"
#include <Preferences.h>
#include "fw2_platform.h"
#include <errno.h>
#include <malloc.h>
#include <time.h>

void setup();
void loop();
extern TFT_eSPI tft;
extern uint8_t  screenRotation;
extern AppState state;
void fw2_nvs_flush(bool all);

#define LCD_W ST7796_W
#define LCD_H ST7796_H

// ---- the link to MAIN -------------------------------------------------------------
static ow_device s_dev;              // ~37 KB of buffers: static, never on the stack
static bool s_link, s_card;
ow_device* fw2_link() { return s_link ? &s_dev : nullptr; }
bool fw2_card_ok() { return s_card; }

// ---- heap: in PSRAM on the board ----------------------------------------------------
// SquachWatch allocates its whole-screen sprite and the panel buffer
// (300 KB each at 480x320), more than the SRAM beside the stack. newlib's
// malloc grows through _sbrk; this one hands out a PSRAM arena.
#if defined(__arm__)
#define FW2_HEAP_BYTES (5u * 1024u * 1024u)
static uint8_t __uninitialized_psram("sqw_heap") s_heap[FW2_HEAP_BYTES];
static size_t s_brk;
extern "C" void* _sbrk(ptrdiff_t incr) {
    if ((incr > 0 && s_brk + (size_t)incr > FW2_HEAP_BYTES) || (incr < 0 && (size_t)(-incr) > s_brk)) {
        errno = ENOMEM;
        return (void*)-1;
    }
    void* p = s_heap + s_brk;
    s_brk += (size_t)incr;
    return p;
}
uint32_t fw2_heap_size() { return FW2_HEAP_BYTES; }
uint32_t fw2_heap_free() {
    struct mallinfo mi = mallinfo();
    return (uint32_t)(mi.fordblks + (FW2_HEAP_BYTES - s_brk));
}
uint32_t fw2_heap_largest() { return (uint32_t)(FW2_HEAP_BYTES - s_brk); }
#else
// The emulator: the host's allocator, and figures that say "plenty".
uint32_t fw2_heap_size() { return 5u * 1024u * 1024u; }
uint32_t fw2_heap_free() { return 4u * 1024u * 1024u; }
uint32_t fw2_heap_largest() { return 4u * 1024u * 1024u; }
#endif

// ---- time, randomness, log ----------------------------------------------------------
uint32_t fw2_millis() { return (uint32_t)(time_us_64() / 1000u); }
uint64_t fw2_micros() { return time_us_64(); }
uint32_t fw2_random32() { return get_rand_32(); }

static char   s_line[240];
static size_t s_lineLen;
void fw2_log(const char* s) {
    for (; s && *s; s++) {
        if (*s == '\n' || s_lineLen == sizeof s_line - 1) {
            s_line[s_lineLen] = 0;
            DIAG("%s\n", s_line);
            s_lineLen = 0;
            if (*s == '\n') continue;
        }
        if (*s != '\r') s_line[s_lineLen++] = *s;
    }
}

// ---- input -------------------------------------------------------------------------
// SquachWatch's pollTouch() maps XPT2046 readings through its factory
// calibration (200..3800 on both axes). The inverse of that mapping turns a
// point on the screen into the reading that lands there, so SquachWatch's
// own touch code, gestures and long presses run unchanged. (The same
// arithmetic as the simulator's live harness, sim/main_live.cpp.)
static const long RAW_MIN = 200, RAW_MAX = 3800;
static long imap(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
static void screenToRaw(int sx, int sy) {
    const int w = tft.width(), h = tft.height();
    const bool landscape = (screenRotation % 2) == 1, flipped = screenRotation >= 2;
    long px, py;
    if (!landscape) {
        px = flipped ? imap(sx, w, 0, RAW_MIN, RAW_MAX) : imap(sx, 0, w, RAW_MIN, RAW_MAX);
        py = flipped ? imap(sy, h, 0, RAW_MIN, RAW_MAX) : imap(sy, 0, h, RAW_MIN, RAW_MAX);
    } else {
        py = flipped ? imap(sx, w, 0, RAW_MIN, RAW_MAX) : imap(sx, 0, w, RAW_MIN, RAW_MAX);
        px = flipped ? imap(sy, 0, h, RAW_MIN, RAW_MAX) : imap(sy, h, 0, RAW_MIN, RAW_MAX);
    }
    SimTouch::rawX = (uint16_t)(px < 0 ? 0 : px > 4095 ? 4095 : px);
    SimTouch::rawY = (uint16_t)(py < 0 ? 0 : py > 4095 ? 4095 : py);
}

static int s_barButton = -1;         // a colour button held: 0 SCAN, 1 LOG, 2 DESK

static void pollInput() {
    uartkbd_event_t ev;
    while (uartkbd_next_event(&ev)) {
        if (ev.btn <= UARTKBD_BTN_GREEN) {
            if (ev.pressed) s_barButton = (int)ev.btn;
            else if (s_barButton == (int)ev.btn) s_barButton = -1;
        }
    }
    uint16_t x, y;
    if (ft6336_poll(&x, &y)) {
        SimTouch::down = true;
        screenToRaw(x * tft.width() / LCD_W, y * tft.height() / LCD_H);
    } else if (s_barButton >= 0) {
        const Theme::ButtonBarGeom g = Theme::computeButtonBar(tft.width(), tft.height());
        SimTouch::down = true;
        screenToRaw(g.x[s_barButton] + g.w[s_barButton] / 2, g.y + g.h / 2);
    } else {
        SimTouch::down = false;
    }
}

// ---- the everyday chores, from the loop and from inside delay() ---------------------
static bool s_inService;
static void service() {
    if (s_inService) return;                 // a radio callback that delays doesn't nest
    s_inService = true;
    fw2_app_recovery_task();
    pollInput();
    fw2_radio_pump();
    fw2_nvs_flush(false);
    agentio_task();
    s_inService = false;
}

void fw2_delay(uint32_t ms) {
    const uint64_t end = time_us_64() + (uint64_t)ms * 1000u;
    do {
        service();
        if (!ms) break;
        sleep_us(500);
    } while (time_us_64() < end);
}

// ---- the screen ----------------------------------------------------------------------
// The panel wants big-endian RGB565; SquachWatch's buffer is native. Rows go
// through two SRAM bands, byte-swapped, one filling while the other is sent.
#define BAND_ROWS 16
static uint16_t s_band[2][LCD_W * BAND_ROWS];
static uint32_t s_rowHash[LCD_H];
static bool     s_first = true;

static uint32_t hashRow(const uint16_t* p, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static void present() {
    const int w = tft.width(), h = tft.height();
    if (w != LCD_W || h != LCD_H) return;          // only the landscape shape (see README)
    const uint16_t* px = tft.pixelsRGB565().data();
    int k = 0;
    for (int y = 0; y < h;) {
        // the next run of changed rows, at most one band
        const uint32_t hy = hashRow(px + y * w, w);
        if (!s_first && hy == s_rowHash[y]) { y++; continue; }
        int n = 0;
        while (y + n < h && n < BAND_ROWS) {
            const uint32_t hh = n ? hashRow(px + (y + n) * w, w) : hy;
            if (!s_first && n && hh == s_rowHash[y + n]) break;
            s_rowHash[y + n] = hh;
            n++;
        }
        uint16_t* dst = s_band[k];
        const uint16_t* src = px + y * w;
        for (int i = 0; i < n * w; i++) dst[i] = (uint16_t)((src[i] >> 8) | (src[i] << 8));
        while (st7796_flush_busy()) tight_loop_contents();
        st7796_flush_async(0, (uint16_t)y, (uint16_t)(w - 1), (uint16_t)(y + n - 1), dst, NULL);
        k ^= 1;
        y += n;
    }
    if (s_first) {
        s_first = false;
        while (st7796_flush_busy()) tight_loop_contents();
        board_backlight_set(1);
    }
}

// After the About screen (PAGE held 5 s) the whole screen is sent again.
static void redrawAll() { s_first = true; }

// ---- the clock -------------------------------------------------------------------------
// The FW2's real-time clock keeps local time. SquachWatch keeps UTC and a
// zone (Clock::applyZone sets TZ), so the board's reading goes through
// mktime() in that zone, and back through localtime() when SquachWatch
// learns the time itself.
static bool s_rtcLoading;
static void rtcWrite(uint32_t epoch) {
    if (s_rtcLoading || !s_link) return;
    time_t t = (time_t)epoch;
    struct tm lt;
    localtime_r(&t, &lt);
    ow_hardware_set_time(&s_dev, lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
}
static void clockFromBoard() {
    int32_t y, mo, d, wd, h, mi, se;
    if (!s_link || ow_hardware_get_time(&s_dev, &y, &mo, &d, &wd, &h, &mi, &se) != OW_OK || y < 2026) {
        DIAG("squachwatch: the board clock isn't set\n");
        return;
    }
    struct tm lt = {};
    lt.tm_year = y - 1900; lt.tm_mon = mo - 1; lt.tm_mday = d;
    lt.tm_hour = h; lt.tm_min = mi; lt.tm_sec = se; lt.tm_isdst = -1;
    const time_t e = mktime(&lt);
    if (e <= 0) return;
    s_rtcLoading = true;
    Clock::setEpoch((uint32_t)e);
    s_rtcLoading = false;
    DIAG("squachwatch: clock from the board, %04d-%02d-%02d %02d:%02d\n", (int)y, (int)mo, (int)d, (int)h, (int)mi);
}

// ---- start -----------------------------------------------------------------------------
int main(void) {
    board_init();                     // before the OneWili link: uart_init reads clk_peri
    fw2_app_recovery_init();
    st7796_init();
    fw2_app_about_use_lcd_restore(redrawAll);
    ft6336_init();
    agentio_init();
    st7796_fill_screen(0x0000);

    if (fw2_app_recovery_open_onewili(&s_dev) == OW_OK && fw2_app_recovery_wrap_sd() == OW_OK) {
        s_link = true;
        ow_sd_mkdir(&s_dev, "/appdata");         // fail harmlessly when they exist
        const ow_status st = ow_sd_mkdir(&s_dev, FW2_APP_DIR);
        bool dir = false;
        uint32_t size = 0;
        s_card = st == OW_OK || ow_sd_stat(&s_dev, FW2_APP_DIR, &dir, &size) == OW_OK;
        if (s_card) ow_sd_mkdir(&s_dev, FW2_APP_DIR "/nvs");
        fw2_app_recovery_task();
    }
    DIAG("squachwatch: link %s, card %s\n", s_link ? "up" : "down", s_card ? "ok" : "none");

    // SquachWatch's 3.5" layout: the simulator's panel starts at 320x240.
    tft = TFT_eSPI(LCD_W, LCD_H);
    // The first-boot colour check is for the CYD family's panel variants.
    // The FW2's panel is one known panel, right at SquachWatch's defaults, so
    // the check is recorded as done (it stays in Settings, as on any board).
    {
        Preferences p;
        p.begin("settings", false);
        if (!p.isKey("colorchk")) p.putBool("colorchk", true);
    }
    setup();
    clockFromBoard();
    Clock::onSet(rtcWrite);
    DIAG("squachwatch: ready\n");

    AppState shown = state;
    for (;;) {
        service();
        loop();
        if (state != shown) {                 // for the log (and the tests): which screen is up
            shown = state;
            DIAG("squachwatch: screen %s\n", fw2StateName(state));
        }
        if (!st7796_flush_busy() || s_first) present();
        fw2_nvs_flush(false);
    }
}
