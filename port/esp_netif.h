// SquachWatch on the FREE-WILi 2: no network interface of its own (the
// ESP32-C5 behind MAIN owns Wi-Fi); detection.cpp only looks one up.
#pragma once
#include "esp_wifi.h"
typedef struct esp_netif_obj esp_netif_t;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) { return nullptr; }
