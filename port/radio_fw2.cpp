// SquachWatch on the FREE-WILi 2: the radios.
//
// The FW2's Wi-Fi and Bluetooth are on its ESP32-C5, running FREE-WILi's
// stock firmware behind the MAIN processor. What an app can ask it for is
// an access-point scan and a Bluetooth scan; the results come back over
// WiliBSP's OneWili link as text events:
//
//   [*wifiscan <ts> <seq> <bssid> <rssi> <channel> <band> <authmode> <ssid> <ok>]
//   [*btscan   <ts> <seq> <name> <mac> <rssi> <ok>]
//
// This file runs those scans in turn for as long as SquachWatch has its
// sniffer and its Bluetooth scan switched on, and hands every result to the
// callbacks SquachWatch installed on the ESP32 APIs (esp_wifi.h,
// NimBLEDevice.h in this folder):
//  - a Wi-Fi network becomes a beacon frame (its BSSID, SSID, channel,
//    and the privacy bit from its security) passed to the promiscuous
//    callback, so SquachWatch's own frame parser, OUI and SSID tables,
//    evil-twin check and log handle it unchanged;
//  - a Bluetooth device becomes an advert with that name and address and no
//    payload, passed to the scan callbacks.
// The NEARBY sweeps (WiFi.scanNetworks, the raw Bluetooth list) use the
// same results.
#include <Arduino.h>
#include "esp_wifi.h"
#include "WiFi.h"
#include "NimBLEDevice.h"
#include "fw2_platform.h"
#include <vector>
#include <string>
#include <ctype.h>

extern "C" {
#include "onewili.h"
#include "pico/unique_id.h"
}

namespace {

// ---- what the ESP32 APIs have been told --------------------------------
wifi_promiscuous_cb_t s_rxCb = nullptr;
bool        s_promisc = false;
bool        s_wifiStarted = false;
wifi_mode_t s_mode = WIFI_MODE_NULL;
uint8_t     s_channel = 1;
bool        s_bleInit = false;
NimBLEScan  s_scan;
NimBLEAdvertising s_adv;

// ---- the last Wi-Fi scan -------------------------------------------------
struct Net { uint8_t bssid[6]; char ssid[33]; int8_t rssi; uint8_t ch; uint8_t auth; };
std::vector<Net> s_nets;        // being filled by the scan in progress
std::vector<Net> s_last;        // the last complete one (WiFi.SSID(i) etc.)
bool s_asyncWanted = false;     // WiFi.scanNetworks(true) is waiting
bool s_asyncDone = false;

// ---- the scan cycle ------------------------------------------------------
enum class Phase { IDLE, BLE, WIFI };
Phase    s_phase = Phase::IDLE;
uint32_t s_phaseAt = 0, s_phaseEnd = 0, s_lastRecordAt = 0;
uint32_t s_retryAt = 0;
bool     s_warned = false;
const uint32_t BLE_SCAN_MS   = 3000;
const uint32_t WIFI_QUIET_MS = 700;     // no new network for this long: the scan is over
const uint32_t WIFI_MAX_MS   = 6000;
const uint32_t RETRY_MS      = 30000;   // MAIN said no (no ESP32, or an old emulator): try again later

Fw2RadioStats s_stats;
uint32_t s_bleDevicesAt = 0;      // s_stats.devices when the Bluetooth scan started

bool parseMac(const char* s, uint8_t out[6]) {
    unsigned v[6];
    char tail;
    if (sscanf(s, "%x:%x:%x:%x:%x:%x%c", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &tail) != 6) return false;
    for (int i = 0; i < 6; i++) { if (v[i] > 255) return false; out[i] = (uint8_t)v[i]; }
    return true;
}

// An event's fields: after "<ts> <seq> ", before " <ok>". The name or SSID
// can have spaces, or be empty.
char* eventFields(char* args) {
    char* p = strchr(args, ' ');
    p = p ? strchr(p + 1, ' ') : nullptr;
    if (!p) return nullptr;
    p++;
    size_t n = strlen(p);
    while (n && (p[n - 1] == '\r' || p[n - 1] == '\n')) p[--n] = 0;
    if (n >= 2 && p[n - 2] == ' ' && (p[n - 1] == '0' || p[n - 1] == '1')) p[n - 2] = 0;
    return p;
}

// A beacon frame for one network: header, fixed parameters, SSID and DS
// parameter elements, and a (zero) frame check sequence -- as much of a
// beacon as SquachWatch's parser reads (detection.cpp, the promiscuous
// callback: frame control, addr2/addr3, the capability's privacy bit, the
// SSID element at offset 36).
void feedBeacon(const Net& n) {
    if (!s_rxCb || !s_promisc || !s_wifiStarted) return;
    struct { wifi_pkt_rx_ctrl_t rx; uint8_t f[96]; } pkt;
    memset(&pkt, 0, sizeof pkt);
    uint8_t* f = pkt.f;
    size_t len = 0;
    f[0] = 0x80;                                   // type 0 (management), subtype 8 (beacon)
    memset(f + 4, 0xFF, 6);                        // addr1: broadcast
    memcpy(f + 10, n.bssid, 6);                    // addr2: transmitter
    memcpy(f + 16, n.bssid, 6);                    // addr3: BSSID
    f[32] = 0x64;                                  // beacon interval 100 TU
    f[34] = 0x01 | (n.auth != WIFI_AUTH_OPEN ? 0x10 : 0x00);   // ESS, and privacy when secured
    len = 36;
    const size_t sl = strlen(n.ssid);
    f[len++] = 0x00; f[len++] = (uint8_t)sl;       // SSID element
    memcpy(f + len, n.ssid, sl); len += sl;
    f[len++] = 0x03; f[len++] = 1; f[len++] = n.ch;   // DS parameter set: the channel
    len += 4;                                      // FCS
    pkt.rx.rssi = n.rssi;
    pkt.rx.channel = n.ch;
    pkt.rx.sig_len = (uint16_t)len;
    s_stats.beacons++;
    s_rxCb(&pkt, WIFI_PKT_MGMT);
}

void onWifiScan(char* f) {
    char bssid[20];
    int rssi, ch, band, auth, used = 0;
    if (sscanf(f, "%19s %d %d %d %d %n", bssid, &rssi, &ch, &band, &auth, &used) < 5) return;
    Net n;
    memset(&n, 0, sizeof n);
    if (!parseMac(bssid, n.bssid)) return;
    snprintf(n.ssid, sizeof n.ssid, "%s", used ? f + used : "");
    n.rssi = (int8_t)(rssi < -127 ? -127 : rssi > 0 ? 0 : rssi);
    n.ch = (uint8_t)(ch < 0 ? 0 : ch > 255 ? 255 : ch);
    n.auth = (uint8_t)auth;
    s_stats.networks++;
    s_lastRecordAt = millis();
    bool seen = false;
    for (auto& e : s_nets) if (!memcmp(e.bssid, n.bssid, 6)) { e = n; seen = true; }
    if (!seen && s_nets.size() < 64) s_nets.push_back(n);
    feedBeacon(n);
}

void onBtScan(char* f) {
    // From the back: the RSSI, then the MAC, and the name is what is left.
    char* sp = strrchr(f, ' ');
    if (!sp) return;
    const int rssi = atoi(sp + 1);
    *sp = 0;
    char* mp = strrchr(f, ' ');
    const char* macs = mp ? mp + 1 : f;
    if (mp) *mp = 0; else f[0] = 0;
    uint8_t mac[6];
    if (!parseMac(macs, mac)) return;
    const char* name = mp ? f : "";
    s_stats.devices++;
    if (!s_scan.isScanning() || !s_scan.callbacks()) return;
    NimBLEAdvertisedDevice dev(mac, rssi, name);
    s_scan.callbacks()->onDiscovered(&dev);
    s_scan.callbacks()->onResult(&dev);
}

void drainEvents(ow_device* dev) {
    char id[24], args[256];
    while (ow_poll_text_line(dev, id, sizeof id, args, sizeof args) == 1) {
        char* f = eventFields(args);
        if (!f) continue;
        if (!strcmp(id, "wifiscan")) onWifiScan(f);
        else if (!strcmp(id, "btscan")) onBtScan(f);
    }
}

bool wantWifi() { return (s_wifiStarted && s_promisc && s_rxCb) || s_asyncWanted; }
bool wantBle()  { return s_bleInit && s_scan.isScanning(); }

void refused(const char* what) {
    s_stats.refusals++;
    if (!s_warned) {
        Serial.printf("[fw2] the %s scan was refused by the MAIN processor; retrying every %lu s\n", what,
                      (unsigned long)(RETRY_MS / 1000));
        s_warned = true;
    }
    s_retryAt = millis() + RETRY_MS;
    s_phase = Phase::IDLE;
}

void finishWifi() {
    s_last = s_nets;
    s_stats.wifiScans++;
    Serial.printf("[fw2] Wi-Fi scan %lu: %u network%s\n", (unsigned long)s_stats.wifiScans, (unsigned)s_nets.size(),
                  s_nets.size() == 1 ? "" : "s");
    if (s_asyncWanted) { s_asyncWanted = false; s_asyncDone = true; }
}

}  // namespace

// ---- the loop's side: run the cycle ----------------------------------------
void fw2_radio_pump() {
    ow_device* dev = fw2_link();
    if (!dev) return;
    drainEvents(dev);
    const uint32_t now = millis();
    switch (s_phase) {
    case Phase::BLE:
        if ((int32_t)(now - s_phaseEnd) < 0) return;
        s_phase = Phase::IDLE;
        Serial.printf("[fw2] Bluetooth scan %lu: %lu device%s\n", (unsigned long)s_stats.bleScans,
                      (unsigned long)(s_stats.devices - s_bleDevicesAt), s_stats.devices - s_bleDevicesAt == 1 ? "" : "s");
        break;
    case Phase::WIFI: {
        const bool quiet = !s_nets.empty() && now - s_lastRecordAt >= WIFI_QUIET_MS;
        if (!quiet && now - s_phaseAt < WIFI_MAX_MS) return;
        finishWifi();
        s_phase = Phase::IDLE;
        // A Wi-Fi scan just ran: Bluetooth next, if it is wanted.
        if (wantBle()) goto start_ble;
        break;
    }
    case Phase::IDLE:
        break;
    }
    if (s_retryAt && (int32_t)(now - s_retryAt) < 0) return;
    s_retryAt = 0;
    // Alternate: after Bluetooth, Wi-Fi; a raw Wi-Fi sweep jumps the queue.
    if (wantWifi() && (s_asyncWanted || !wantBle() || s_stats.bleScans > s_stats.wifiScans)) {
        s_nets.clear();
        s_lastRecordAt = now;
        if (ow_wireless_wifi_on_scan_for_access_points(dev) != OW_OK) { refused("Wi-Fi"); return; }
        s_phase = Phase::WIFI;
        s_phaseAt = now;
        return;
    }
    if (!wantBle()) return;
start_ble:
    if (ow_wireless_bluetooth_le_on_scan_bt_devices(dev, (int32_t)BLE_SCAN_MS) != OW_OK) { refused("Bluetooth"); return; }
    s_stats.bleScans++;
    s_bleDevicesAt = s_stats.devices;
    s_phase = Phase::BLE;
    s_phaseAt = millis();
    s_phaseEnd = s_phaseAt + BLE_SCAN_MS + 300;
}

Fw2RadioStats fw2_radio_stats() { return s_stats; }

// ---- esp_wifi.h ---------------------------------------------------------------
esp_err_t esp_wifi_init(const wifi_init_config_t*) { return ESP_OK; }
esp_err_t esp_wifi_deinit() { s_wifiStarted = false; return ESP_OK; }
esp_err_t esp_wifi_start() { s_wifiStarted = true; return ESP_OK; }
esp_err_t esp_wifi_stop() { s_wifiStarted = false; return ESP_OK; }
esp_err_t esp_wifi_set_mode(wifi_mode_t m) { s_mode = m; return ESP_OK; }
esp_err_t esp_wifi_get_mode(wifi_mode_t* m) { if (m) *m = s_mode; return ESP_OK; }
esp_err_t esp_wifi_set_promiscuous(bool on) { s_promisc = on; return ESP_OK; }
esp_err_t esp_wifi_get_promiscuous(bool* on) { if (on) *on = s_promisc; return ESP_OK; }
esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t*) { return ESP_OK; }
esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb) { s_rxCb = cb; return ESP_OK; }
// SquachWatch hops the sniffer across channels 1-13; the ESP32-C5's access
// point scan covers every channel (both bands) by itself, so the hop is
// only remembered.
esp_err_t esp_wifi_set_channel(uint8_t ch, wifi_second_chan_t) { s_channel = ch; return ESP_OK; }
esp_err_t esp_wifi_get_channel(uint8_t* ch, wifi_second_chan_t* c2) {
    if (ch) *ch = s_channel;
    if (c2) *c2 = WIFI_SECOND_CHAN_NONE;
    return ESP_OK;
}
esp_err_t esp_wifi_get_max_tx_power(int8_t* p) { if (p) *p = 0; return ESP_OK; }
esp_err_t esp_wifi_get_country(wifi_country_t* c) {
    if (c) { memset(c, 0, sizeof *c); memcpy(c->cc, "01", 2); c->schan = 1; c->nchan = 13; }
    return ESP_OK;
}
// The console's RADIO SCAN: what the last scan found.
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*, bool) { return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t* n) { if (n) *n = (uint16_t)s_last.size(); return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_records(uint16_t* n, wifi_ap_record_t* recs) {
    if (!n || !recs) return ESP_FAIL;
    uint16_t k = 0;
    for (; k < *n && k < s_last.size(); k++) {
        memset(&recs[k], 0, sizeof recs[k]);
        memcpy(recs[k].bssid, s_last[k].bssid, 6);
        memcpy(recs[k].ssid, s_last[k].ssid, sizeof recs[k].ssid);
        recs[k].primary = s_last[k].ch;
        recs[k].rssi = s_last[k].rssi;
        recs[k].authmode = (wifi_auth_mode_t)s_last[k].auth;
    }
    *n = k;
    return ESP_OK;
}
esp_err_t esp_wifi_clear_ap_list() { return ESP_OK; }

// ---- WiFi.h -------------------------------------------------------------------
WiFiClass WiFi;
bool WiFiClass::mode(wifi_mode_t m) { s_mode = m; s_wifiStarted = m != WIFI_MODE_NULL; return true; }
wifi_mode_t WiFiClass::getMode() { return s_mode; }
int16_t WiFiClass::scanNetworks(bool async, bool, bool, uint32_t, uint8_t) {
    s_asyncWanted = true;
    s_asyncDone = false;
    if (!async) {                                   // blocking: wait for it here
        const uint32_t t0 = millis();
        while (!s_asyncDone && millis() - t0 < 15000) delay(10);
    }
    return async ? WIFI_SCAN_RUNNING : (int16_t)s_last.size();
}
int16_t WiFiClass::scanComplete() {
    if (s_asyncWanted) return WIFI_SCAN_RUNNING;
    return s_asyncDone ? (int16_t)s_last.size() : WIFI_SCAN_FAILED;
}
void WiFiClass::scanDelete() { s_asyncDone = false; s_asyncWanted = false; }
String WiFiClass::SSID(uint8_t i) { return i < s_last.size() ? String(s_last[i].ssid) : String(""); }
int32_t WiFiClass::RSSI(uint8_t i) { return i < s_last.size() ? s_last[i].rssi : 0; }
int32_t WiFiClass::channel(uint8_t i) { return i < s_last.size() ? s_last[i].ch : 0; }
wifi_auth_mode_t WiFiClass::encryptionType(uint8_t i) {
    return i < s_last.size() ? (wifi_auth_mode_t)s_last[i].auth : WIFI_AUTH_OPEN;
}
uint8_t* WiFiClass::BSSID(uint8_t i) { return i < s_last.size() ? s_last[i].bssid : nullptr; }

// ---- NimBLEDevice.h -----------------------------------------------------------
NimBLEUUID::NimBLEUUID(const char* s) {
    // 128-bit, written big-endian with dashes; stored little-endian, as NimBLE does.
    uint8_t b[16];
    int n = 0;
    for (const char* p = s; p && *p && n < 32; p++) {
        if (*p == '-') continue;
        if (!isxdigit((unsigned char)*p)) return;
        const int v = isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10);
        if (n % 2 == 0) b[n / 2] = (uint8_t)(v << 4); else b[n / 2] |= (uint8_t)v;
        n++;
    }
    if (n != 32) return;
    for (int i = 0; i < 16; i++) _v[i] = b[15 - i];
    _bits = 128;
}
bool NimBLEDevice::init(const std::string&) { s_bleInit = true; return true; }
bool NimBLEDevice::isInitialized() { return s_bleInit; }
NimBLEScan* NimBLEDevice::getScan() { return s_bleInit ? &s_scan : nullptr; }
NimBLEAdvertising* NimBLEDevice::getAdvertising() { return s_bleInit ? &s_adv : nullptr; }
// A stable, locally administered address from the display processor's
// unique id: SquachMesh puts it in its message nonces.
NimBLEAddress NimBLEDevice::getAddress() {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = id.id[i + 2];
    mac[0] = (uint8_t)((mac[0] | 0x02) & 0xFE);
    return NimBLEAddress(mac);
}
