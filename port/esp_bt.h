// SquachWatch on the FREE-WILi 2: Bluetooth Classic isn't reachable (the
// stock ESP32-C5 firmware scans BLE only), so the controller reports idle.
#pragma once
typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE = 0, ESP_BT_CONTROLLER_STATUS_INITED, ESP_BT_CONTROLLER_STATUS_ENABLED
} esp_bt_controller_status_t;
inline esp_bt_controller_status_t esp_bt_controller_get_status() { return ESP_BT_CONTROLLER_STATUS_IDLE; }
