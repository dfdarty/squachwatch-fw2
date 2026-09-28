// SquachWatch on the FREE-WILi 2: the Arduino core, as far as SquachWatch
// uses it, on the Pico SDK.
//
// Found before the simulator's own Arduino.h (this folder comes first on
// the include path) and modelled on it: the same surface, but the clock is
// the RP2350's, delay() really waits (while the radio feed and HOME-to-exit
// keep running), Serial goes to the FW2's diagnostics log, and the heap
// numbers are real.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>

// Platform hooks, in platform_fw2.cpp.
uint32_t fw2_millis();
uint64_t fw2_micros();
void     fw2_delay(uint32_t ms);     // services the radio feed and the exit gesture while it waits
void     fw2_log(const char* s);     // one chunk of Serial output
uint32_t fw2_heap_free();
uint32_t fw2_heap_size();
uint32_t fw2_heap_largest();

inline uint32_t millis() { return fw2_millis(); }
inline uint32_t micros() { return (uint32_t)fw2_micros(); }
inline void delay(uint32_t ms) { fw2_delay(ms); }
inline void delayMicroseconds(uint32_t us) { fw2_delay((us + 999) / 1000); }
inline void yield() { fw2_delay(0); }

inline long random(long howbig) { return howbig <= 0 ? 0 : (long)(::rand() % howbig); }
inline long random(long howsmall, long howbig) {
    return howbig <= howsmall ? howsmall : howsmall + (long)(::rand() % (howbig - howsmall));
}
inline void randomSeed(unsigned long s) { ::srand((unsigned)s); }

#define IRAM_ATTR
#define PROGMEM
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886
#endif
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#define pgm_read_word(addr) (*(const unsigned short*)(addr))

// The CYD's pins mean nothing here: the FW2 has its own display, touch and
// backlight, driven by platform_fw2.cpp.
#define INPUT        0x01
#define OUTPUT       0x03
#define INPUT_PULLUP 0x05
#define HIGH         0x1
#define LOW          0x0
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int  digitalRead(uint8_t) { return 0; }
inline int  analogRead(uint8_t) { return 0; }
inline void ledcSetup(uint8_t, double, uint8_t) {}
inline void ledcAttachPin(uint8_t, uint8_t) {}
inline void ledcWrite(uint8_t, uint32_t) {}

// Single loop, no second task touching the firmware's queues: the radio
// callbacks run from the loop (radio_fw2.cpp), not from a Bluetooth host
// task on the other core, so the ESP32's critical sections have nothing to
// keep apart.
inline void interrupts() {}
inline void noInterrupts() {}
typedef struct { int unused; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
#define portENTER_CRITICAL_ISR(m) ((void)(m))
#define portEXIT_CRITICAL_ISR(m) ((void)(m))

// The ESP32's on-die temperature, for the RADIO console report. The FW2's
// display processor has no calibrated equivalent worth quoting: NAN, which
// the report prints as nan.
inline float temperatureRead() { return NAN; }

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#ifndef constrain
#define constrain(a, lo, hi) ((a) < (lo) ? (lo) : ((a) > (hi) ? (hi) : (a)))
#endif

// What the diagnostics screen reads.
struct EspClass {
    uint32_t getFreeHeap()    { return fw2_heap_free(); }
    uint32_t getHeapSize()    { return fw2_heap_size(); }
    uint32_t getPsramSize()   { return 0; }
    uint32_t getFreePsram()   { return 0; }
    const char* getChipModel(){ return "RP2350B (FREE-WILi 2)"; }
    uint8_t  getChipRevision(){ return 2; }
    uint32_t getCpuFreqMHz()  { return 250; }
    void     restart()        {}
};
inline EspClass ESP;
inline bool psramFound() { return false; }

// Serial -> the FW2's diagnostics log (RTT on the board, the log panel in
// the emulator), a line at a time.
struct SerialShim {
    void begin(unsigned long) {}
    void print(const char* s) { fw2_log(s); }
    void print(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); fw2_log(b); }
    void println(const char* s) { fw2_log(s); fw2_log("\n"); }
    void println() { fw2_log("\n"); }
    void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        char b[256];
        va_list ap; va_start(ap, fmt);
        vsnprintf(b, sizeof b, fmt, ap);
        va_end(ap);
        fw2_log(b);
    }
    void flush() {}
    int  available() { return 0; }
    int  read() { return -1; }
    size_t write(const uint8_t*, size_t n) { return n; }
    int  availableForWrite() { return 256; }
    explicit operator bool() const { return true; }
};
inline SerialShim Serial;

// The POWER SAVER menu's clock scaling: the FW2's display processor keeps
// its clock (the SPI and PSRAM timings are set from it), so this only
// records the request.
inline uint32_t g_fw2CpuMhz = 250;
inline bool setCpuFrequencyMhz(uint32_t mhz) { g_fw2CpuMhz = mhz; return true; }
inline uint32_t getCpuFrequencyMhz() { return g_fw2CpuMhz; }

// Arduino's String, as much of it as the radio code reads back
// (WiFi.SSID(i).c_str()).
#include <string>
class String {
public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    const char* c_str() const { return _s.c_str(); }
    size_t length() const { return _s.size(); }
    bool operator==(const char* o) const { return _s == (o ? o : ""); }
private:
    std::string _s;
};
