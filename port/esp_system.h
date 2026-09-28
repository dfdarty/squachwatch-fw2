// SquachWatch on the FREE-WILi 2: the ESP-IDF system calls it uses.
// Replaces the simulator's version for one reason: esp_random() there is a
// fixed sequence (so the emulator renders the same frame every run), and on
// a device the security phrase and the mesh keys come from it. Here it is
// the RP2350's true random number generator.
#pragma once
#include <stdint.h>

typedef enum {
    ESP_RST_UNKNOWN = 0, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW,
    ESP_RST_PANIC, ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT,
    ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO,
} esp_reset_reason_t;
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif

uint32_t fw2_random32();   // platform_fw2.cpp: pico_rand
inline uint32_t esp_random() { return fw2_random32(); }
inline void esp_fill_random(void* buf, unsigned len) {
    uint8_t* p = (uint8_t*)buf;
    while (len) {
        uint32_t r = fw2_random32();
        for (int i = 0; i < 4 && len; i++, len--) { *p++ = (uint8_t)r; r >>= 8; }
    }
}
inline void esp_restart() {}
