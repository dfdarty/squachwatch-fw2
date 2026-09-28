// SquachWatch on the FREE-WILi 2: Preferences (the ESP32's NVS) on the SD
// card, one file per namespace in /appdata/squachwatch/nvs/.
//
// Derived from the simulator's Preferences shim (sim/Preferences.h, GPL-3.0,
// same text format: "<type> <key> <value>" per line). Two differences, both
// for NVS's sake:
//  - Every Preferences object on one namespace shares one store, as NVS
//    handles do. SquachWatch opens the same namespace from several places
//    (a settings object and a local one in touch_cal.cpp, say); with a map
//    per object, whichever saved last would erase the other's keys.
//  - A write marks the namespace dirty and platform_fw2.cpp saves it a
//    moment later, so a burst of puts is one card write, not twenty.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace Fw2Nvs {
struct Store {
    std::map<std::string, bool>        b;
    std::map<std::string, uint8_t>     u;
    std::map<std::string, uint32_t>    ui;
    std::map<std::string, uint64_t>    ul;
    std::map<std::string, int16_t>     sh;
    std::map<std::string, std::string> s;
    bool dirty = false;
};
Store& open(const char* ns);          // loads it from the card the first time (platform_fw2.cpp)
void   touched(const char* ns);       // schedules a save
}

class Preferences {
public:
    bool begin(const char* ns, bool = false) {
        _ns = ns ? ns : "";
        _st = &Fw2Nvs::open(_ns.c_str());
        return true;
    }
    void end() {}

    bool isKey(const char* k) const {
        return _st && (_st->b.count(k) || _st->u.count(k) || _st->ui.count(k) || _st->ul.count(k) ||
                       _st->s.count(k) || _st->sh.count(k));
    }
    bool remove(const char* k) {
        if (!_st) return false;
        _st->b.erase(k); _st->u.erase(k); _st->ui.erase(k); _st->ul.erase(k); _st->s.erase(k); _st->sh.erase(k);
        save();
        return true;
    }
    bool clear() {
        if (!_st) return false;
        _st->b.clear(); _st->u.clear(); _st->ui.clear(); _st->ul.clear(); _st->s.clear(); _st->sh.clear();
        save();
        return true;
    }

    int16_t putShort(const char* k, int16_t v) { if (_st) { _st->sh[k] = v; save(); } return v; }
    int16_t getShort(const char* k, int16_t d = 0) const { return get(_st ? &_st->sh : nullptr, k, d); }
    bool putBool(const char* k, bool v) { if (_st) { _st->b[k] = v; save(); } return true; }
    bool getBool(const char* k, bool d = false) const { return get(_st ? &_st->b : nullptr, k, d); }
    uint8_t putUChar(const char* k, uint8_t v) { if (_st) { _st->u[k] = v; save(); } return 1; }
    uint8_t getUChar(const char* k, uint8_t d = 0) const { return get(_st ? &_st->u : nullptr, k, d); }
    uint32_t putUInt(const char* k, uint32_t v) { if (_st) { _st->ui[k] = v; save(); } return 4; }
    uint32_t getUInt(const char* k, uint32_t d = 0) const { return get(_st ? &_st->ui : nullptr, k, d); }
    size_t putULong(const char* k, uint32_t v) { return putUInt(k, v); }
    uint32_t getULong(const char* k, uint32_t d = 0) const { return getUInt(k, d); }
    size_t putULong64(const char* k, uint64_t v) { if (_st) { _st->ul[k] = v; save(); } return 8; }
    uint64_t getULong64(const char* k, uint64_t d = 0) const { return get(_st ? &_st->ul : nullptr, k, d); }

    // Blobs, hex-encoded into the string map (as the simulator does). A
    // zero-length put is refused and leaves the key alone, as on the ESP32.
    size_t putBytes(const char* k, const void* v, size_t len) {
        if (!_st || !k || !v || !len) return 0;
        static const char* HEX = "0123456789abcdef";
        const uint8_t* p = (const uint8_t*)v;
        std::string out;
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) { out += HEX[(p[i] >> 4) & 0xF]; out += HEX[p[i] & 0xF]; }
        _st->s[k] = out;
        save();
        return len;
    }
    size_t getBytesLength(const char* k) const {
        if (!_st) return 0;
        auto it = _st->s.find(k);
        return it == _st->s.end() ? 0 : it->second.size() / 2;
    }
    size_t getBytes(const char* k, void* out, size_t maxLen) const {
        if (!_st) return 0;
        auto it = _st->s.find(k);
        if (it == _st->s.end()) return 0;
        const std::string& h = it->second;
        size_t n = h.size() / 2;
        if (n > maxLen) n = maxLen;
        uint8_t* o = (uint8_t*)out;
        for (size_t i = 0; i < n; i++) o[i] = (uint8_t)((nyb(h[i * 2]) << 4) | nyb(h[i * 2 + 1]));
        return n;
    }
    size_t putString(const char* k, const char* v) {
        if (!_st) return 0;
        _st->s[k] = v ? v : "";
        save();
        return v ? strlen(v) : 0;
    }
    size_t getString(const char* k, char* buf, size_t maxLen) const {
        if (!buf || !maxLen) return 0;
        std::string v;
        if (_st) { auto it = _st->s.find(k); if (it != _st->s.end()) v = it->second; }
        size_t n = v.size() < maxLen - 1 ? v.size() : maxLen - 1;
        memcpy(buf, v.data(), n);
        buf[n] = 0;
        return n;
    }

private:
    template <class M, class T>
    static T get(const M* m, const char* k, T d) {
        if (!m) return d;
        auto it = m->find(k);
        return it == m->end() ? d : it->second;
    }
    static uint8_t nyb(char c) {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
        return 0;
    }
    void save() { if (_st) { _st->dirty = true; Fw2Nvs::touched(_ns.c_str()); } }

    std::string     _ns;
    Fw2Nvs::Store*  _st = nullptr;
};
