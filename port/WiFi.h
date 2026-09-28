// SquachWatch on the FREE-WILi 2: Arduino's WiFi object, for the
// NEARBY/raw Wi-Fi sweep, backed by the stock access-point scan the
// ESP32-C5 does for the MAIN processor (radio_fw2.cpp).
#pragma once
#include <Arduino.h>
#include "esp_wifi.h"

#define WIFI_OFF WIFI_MODE_NULL
#define WIFI_STA WIFI_MODE_STA
#define WIFI_AP  WIFI_MODE_AP
#define WIFI_AP_STA WIFI_MODE_APSTA
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED  (-2)

class WiFiClass {
public:
    bool     mode(wifi_mode_t m);
    wifi_mode_t getMode();
    bool     disconnect(bool = false, bool = false) { return true; }
    int16_t  scanNetworks(bool async = false, bool show_hidden = false, bool passive = false,
                          uint32_t max_ms_per_chan = 300, uint8_t channel = 0);
    int16_t  scanComplete();
    void     scanDelete();
    String   SSID(uint8_t i);
    int32_t  RSSI(uint8_t i);
    int32_t  channel(uint8_t i);
    wifi_auth_mode_t encryptionType(uint8_t i);
    uint8_t* BSSID(uint8_t i);
};
extern WiFiClass WiFi;
