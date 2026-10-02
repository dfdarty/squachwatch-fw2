// SquachWatch on the FREE-WILi 2: the NimBLE-Arduino classes detection.cpp
// uses, backed by the stock Bluetooth scan the ESP32-C5 does for the MAIN
// processor (radio_fw2.cpp).
//
// That scan reports a device's name, address and signal and nothing else,
// so each result reaches SquachWatch's scan callbacks as an advert with
// that name and no services, company IDs or payload. SquachWatch's own
// handler then does what it does with any advert: here, that means the
// name signatures (Flock, Axon, Flipper, skimmer names) can match and the
// payload ones (AirTag, Tile, SmartTag, drones, Meta glasses) can't.
//
// Advertising (the SquachMesh beacon) is accepted and goes nowhere: the
// stock firmware can advertise a name, not manufacturer data.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#define BLE_HCI_ADV_TYPE_ADV_IND            0
#define BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD  1
#define BLE_HCI_ADV_TYPE_ADV_SCAN_IND       2
#define BLE_HCI_ADV_TYPE_ADV_NONCONN_IND    3
#define BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD  4
#define BLE_GAP_CONN_MODE_NON 0
#define BLE_GAP_CONN_MODE_DIR 1
#define BLE_GAP_CONN_MODE_UND 2
#define BLE_GAP_DISC_MODE_NON 0
#define BLE_GAP_DISC_MODE_LTD 1
#define BLE_GAP_DISC_MODE_GEN 2

struct ble_addr_t { uint8_t type; uint8_t val[6]; };

class NimBLEAddress {
public:
    NimBLEAddress() { memset(&_a, 0, sizeof _a); }
    // `mac` in printed order (AA:BB:...:FF, as the FW2's scan reports it).
    // NimBLE keeps an address least-significant byte first, and prints it
    // the other way round; SquachWatch relies on both (it flips the bytes
    // into printed order itself, from v1.24.0 on).
    explicit NimBLEAddress(const uint8_t mac[6]) {
        _a.type = 0;
        for (int i = 0; i < 6; i++) _a.val[i] = mac[5 - i];
    }
    const ble_addr_t* getBase() const { return &_a; }
    std::string toString() const {
        char b[18];
        snprintf(b, sizeof b, "%02x:%02x:%02x:%02x:%02x:%02x", _a.val[5], _a.val[4], _a.val[3], _a.val[2], _a.val[1], _a.val[0]);
        return b;
    }
private:
    ble_addr_t _a;
};

class NimBLEUUID {
public:
    NimBLEUUID() {}
    explicit NimBLEUUID(uint16_t v) : _bits(16) { _v[0] = (uint8_t)v; _v[1] = (uint8_t)(v >> 8); }
    explicit NimBLEUUID(const char* s);              // "e8ccbb38-9532-46a8-9fe5-1814df172e6f"
    uint8_t bitSize() const { return _bits; }
    const uint8_t* getValue() const { return _bits ? _v : nullptr; }
    bool equals(const NimBLEUUID& o) const { return _bits == o._bits && !memcmp(_v, o._v, _bits / 8); }
private:
    uint8_t _bits = 0;
    uint8_t _v[16] = {0};
};

// One scan result as the stock firmware reports it.
class NimBLEAdvertisedDevice {
public:
    NimBLEAdvertisedDevice(const uint8_t mac[6], int rssi, const char* name)
        : _addr(mac), _rssi(rssi), _name(name ? name : "") {}
    const NimBLEAddress& getAddress() const { return _addr; }
    uint8_t     getAdvType() const { return BLE_HCI_ADV_TYPE_ADV_IND; }
    bool        isLegacyAdvertisement() const { return true; }
    bool        isScannable() const { return false; }
    int         getRSSI() const { return _rssi; }
    std::string getName() const { return _name; }   // a copy, as NimBLE-Arduino 2.x returns
    bool        haveName() const { return !_name.empty(); }
    bool        haveManufacturerData() const { return false; }
    uint8_t     getManufacturerDataCount() const { return 0; }
    std::string getManufacturerData(uint8_t = 0) const { return std::string(); }
    const std::vector<uint8_t>& getPayload() const { return _payload; }
    bool        haveServiceUUID() const { return false; }
    int         getServiceUUIDCount() const { return 0; }
    NimBLEUUID  getServiceUUID(int = 0) const { return NimBLEUUID(); }
    bool        haveServiceData() const { return false; }
private:
    NimBLEAddress _addr;
    int _rssi;
    std::string _name;
    std::vector<uint8_t> _payload;       // the advert's bytes: not reported by the stock scan
};

class NimBLEScanCallbacks {
public:
    virtual ~NimBLEScanCallbacks() {}
    virtual void onDiscovered(const NimBLEAdvertisedDevice*) {}
    virtual void onResult(const NimBLEAdvertisedDevice*) {}
    virtual void onScanEnd(const std::vector<NimBLEAdvertisedDevice*>&, int) {}
};

class NimBLEScan {
public:
    void setActiveScan(bool) {}
    void setInterval(uint16_t) {}
    void setWindow(uint16_t) {}
    void setDuplicateFilter(bool) {}
    void setScanResponseTimeout(uint32_t) {}
    void setMaxResults(uint8_t) {}
    void setScanCallbacks(NimBLEScanCallbacks* cb, bool = false) { _cb = cb; }
    bool start(uint32_t duration = 0, bool = false, bool = true) { (void)duration; _scanning = true; return true; }
    bool stop() { _scanning = false; return true; }
    bool isScanning() const { return _scanning; }
    NimBLEScanCallbacks* callbacks() const { return _cb; }   // radio_fw2.cpp hands results to these
private:
    NimBLEScanCallbacks* _cb = nullptr;
    bool _scanning = false;
};

class NimBLEAdvertisementData {
public:
    void setManufacturerData(const std::string&) {}
    void setName(const std::string&) {}
};

class NimBLEAdvertising {
public:
    void setAdvertisementData(const NimBLEAdvertisementData&) {}
    void setScanResponseData(const NimBLEAdvertisementData&) {}
    void enableScanResponse(bool) {}
    void setDiscoverableMode(uint8_t) {}
    void setConnectableMode(uint8_t) {}
    void setMinInterval(uint16_t) {}
    void setMaxInterval(uint16_t) {}
    bool start() { return true; }
    bool stop() { return true; }
};

class NimBLEDevice {
public:
    static bool init(const std::string&);
    static bool isInitialized();
    static NimBLEScan* getScan();
    static NimBLEAdvertising* getAdvertising();
    static NimBLEAddress getAddress();
};
