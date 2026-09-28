// SquachWatch on the FREE-WILi 2: the ESP-IDF Wi-Fi driver calls
// detection.cpp makes, backed by radio_fw2.cpp.
//
// The FW2's Wi-Fi is on its ESP32-C5, behind the MAIN processor, and the
// stock firmware there offers an access-point scan, not a sniffer. So the
// promiscuous callback SquachWatch installs is fed with a beacon frame
// made up from each network the scan reports (radio_fw2.cpp), and
// SquachWatch's own frame parser takes it from there. Probe requests, data
// frames and deauths never arrive: the stock scan doesn't see them.
#pragma once
#include <cstdint>
#include <cstddef>

typedef int esp_err_t;
#define ESP_OK   0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101

typedef enum { WIFI_MODE_NULL = 0, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_APSTA } wifi_mode_t;
typedef enum { WIFI_SECOND_CHAN_NONE = 0, WIFI_SECOND_CHAN_ABOVE, WIFI_SECOND_CHAN_BELOW } wifi_second_chan_t;
typedef enum { WIFI_PKT_MGMT, WIFI_PKT_CTRL, WIFI_PKT_DATA, WIFI_PKT_MISC } wifi_promiscuous_pkt_type_t;
typedef enum { WIFI_SCAN_TYPE_ACTIVE = 0, WIFI_SCAN_TYPE_PASSIVE } wifi_scan_type_t;
typedef enum {
    WIFI_AUTH_OPEN = 0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
    WIFI_AUTH_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WAPI_PSK, WIFI_AUTH_OWE,
    WIFI_AUTH_MAX
} wifi_auth_mode_t;
#define WIFI_AUTH_WPA2_ENTERPRISE WIFI_AUTH_ENTERPRISE

#define WIFI_PROMIS_FILTER_MASK_ALL  0xFFFFFFFF
#define WIFI_PROMIS_FILTER_MASK_MGMT (1)
#define WIFI_PROMIS_FILTER_MASK_CTRL (1 << 1)
#define WIFI_PROMIS_FILTER_MASK_DATA (1 << 2)
typedef struct { uint32_t filter_mask; } wifi_promiscuous_filter_t;

// Only the three fields SquachWatch reads, and a whole byte for the
// channel: the ESP32's 4-bit field can't hold the C5's 5 GHz channels.
typedef struct {
    int8_t   rssi;
    uint8_t  channel;
    uint16_t sig_len;
} wifi_pkt_rx_ctrl_t;
typedef struct {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t payload[0];
} wifi_promiscuous_pkt_t;
typedef void (*wifi_promiscuous_cb_t)(void* buf, wifi_promiscuous_pkt_type_t type);

typedef struct {
    int static_rx_buf_num, dynamic_rx_buf_num, tx_buf_type, static_tx_buf_num, dynamic_tx_buf_num,
        cache_tx_buf_num, csi_enable, ampdu_rx_enable, ampdu_tx_enable, amsdu_tx_enable, nvs_enable,
        mgmt_sbuf_num;
} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

typedef struct { char cc[3]; uint8_t schan; uint8_t nchan; int8_t max_tx_power; int policy; } wifi_country_t;
typedef struct {
    const uint8_t* ssid; const uint8_t* bssid; uint8_t channel; bool show_hidden; wifi_scan_type_t scan_type;
    struct { struct { uint32_t min, max; } active; uint32_t passive; } scan_time;
} wifi_scan_config_t;
typedef struct {
    uint8_t bssid[6]; uint8_t ssid[33]; uint8_t primary; wifi_second_chan_t second; int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_t;

esp_err_t esp_wifi_init(const wifi_init_config_t*);
esp_err_t esp_wifi_deinit();
esp_err_t esp_wifi_start();
esp_err_t esp_wifi_stop();
esp_err_t esp_wifi_set_mode(wifi_mode_t);
esp_err_t esp_wifi_get_mode(wifi_mode_t*);
esp_err_t esp_wifi_set_promiscuous(bool);
esp_err_t esp_wifi_get_promiscuous(bool*);
esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t*);
esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t);
esp_err_t esp_wifi_set_channel(uint8_t, wifi_second_chan_t);
esp_err_t esp_wifi_get_channel(uint8_t*, wifi_second_chan_t*);
esp_err_t esp_wifi_get_max_tx_power(int8_t*);
esp_err_t esp_wifi_get_country(wifi_country_t*);
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*, bool block);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t*);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t*, wifi_ap_record_t*);
esp_err_t esp_wifi_clear_ap_list();
