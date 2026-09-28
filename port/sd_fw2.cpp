// SquachWatch on the FREE-WILi 2: the SD card (SD.h) and NVS
// (Preferences.h), on the card in the MAIN processor's slot, through
// WiliBSP's OneWili link. Everything lives under /appdata/squachwatch:
//   squachwatch-<day>.log   the detection log sd_log.cpp writes
//   nvs/<namespace>.nvs     one file per Preferences namespace
#include <Arduino.h>
#include "SD.h"
#include "Preferences.h"
#include "fw2_platform.h"
#include <map>
#include <string>

extern "C" {
#include "onewili.h"
#include "onewili_sd.h"
#include "input/app_recovery.h"
}

SDClass SD;

static std::string fullPath(const char* p) {
    std::string s = FW2_APP_DIR;
    if (!p || !*p || !strcmp(p, "/")) return s;
    if (p[0] != '/') s += '/';
    s += p;
    return s;
}

// Whole files are written in 512-byte pieces: the OneWili client WiliBSP
// ships sends each ow_sd_write() as one burst, and a long burst can overrun
// the MAIN processor's receive buffer while the card is busy.
static bool writeFile(const std::string& path, const std::string& data, bool append) {
    ow_device* dev = fw2_link();
    if (!dev || !fw2_card_ok()) return false;
    ow_sd_file f;
    if (ow_sd_open(dev, &f, path.c_str(), append ? OW_SD_APPEND : OW_SD_WRITE) != OW_OK) return false;
    bool ok = true;
    for (size_t off = 0; ok && off < data.size(); off += 512) {
        const size_t n = data.size() - off < 512 ? data.size() - off : 512;
        ok = ow_sd_write(&f, data.data() + off, n) == OW_OK;
        fw2_app_recovery_task();
    }
    return ow_sd_close(&f) == OW_OK && ok;
}

// ---- SD ------------------------------------------------------------------------
bool SDClass::mount() { return fw2_link() && fw2_card_ok(); }
uint64_t SDClass::cardSize() { return 0; }   // not something OneWili reports

File SDClass::open(const char* path, const char* mode) {
    File f;
    ow_device* dev = fw2_link();
    if (!dev || !fw2_card_ok()) return f;
    f._path = fullPath(path);
    const char* base = strrchr(f._path.c_str(), '/');
    f._name = base ? base + 1 : f._path;
    if (mode && (mode[0] == 'w' || mode[0] == 'a')) {
        f._append = mode[0] == 'a';
        f._open = true;
        if (!f._append) writeFile(f._path, std::string(), false);   // truncate now, as FILE_WRITE does
        f._append = true;
        return f;
    }
    bool isDir = false;
    uint32_t size = 0;
    if (ow_sd_stat(dev, f._path.c_str(), &isDir, &size) != OW_OK) return f;
    f._open = true;
    f._dir = isDir;
    f._size = size;
    if (isDir) {
        ow_sd_list(dev, f._path.c_str(), [](const char* name, bool d, uint32_t sz, void* user) {
            File* ff = (File*)user;
            ff->_entries.push_back(name);
            ff->_isDir.push_back(d);
            ff->_sizes.push_back(sz);
        }, &f);
    }
    return f;
}

bool SDClass::exists(const char* path) {
    bool d; uint32_t s;
    return fw2_link() && ow_sd_stat(fw2_link(), fullPath(path).c_str(), &d, &s) == OW_OK;
}
bool SDClass::remove(const char* path) {
    return fw2_link() && ow_sd_remove(fw2_link(), fullPath(path).c_str()) == OW_OK;
}
bool SDClass::mkdir(const char* path) {
    return fw2_link() && ow_sd_mkdir(fw2_link(), fullPath(path).c_str()) == OW_OK;
}

size_t File::print(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
size_t File::println(const char* s) { size_t n = print(s); return n + print("\n"); }
size_t File::write(const uint8_t* b, size_t n) {
    if (!_open || _dir) return 0;
    _pending.append((const char*)b, n);
    return n;
}
void File::flush() {
    if (_open && !_pending.empty()) {
        writeFile(_path, _pending, true);
        _size += _pending.size();
        _pending.clear();
    }
}
void File::close() { flush(); _open = false; }
File File::openNextFile() {
    File f;
    if (!_dir || _next >= _entries.size()) return f;
    f._open = true;
    f._name = _entries[_next];
    f._path = _path + "/" + f._name;
    f._dir = _isDir[_next];
    f._size = _sizes[_next];
    _next++;
    return f;
}

// ---- NVS ------------------------------------------------------------------------
namespace {
std::map<std::string, Fw2Nvs::Store> s_ns;
std::map<std::string, uint32_t> s_dirtyAt;
const uint32_t SAVE_AFTER_MS = 500;

std::string nvsPath(const std::string& ns) { return std::string(FW2_APP_DIR "/nvs/") + ns + ".nvs"; }

std::string serialize(const Fw2Nvs::Store& st) {
    std::string out;
    char line[600];
    for (auto& kv : st.b)  { snprintf(line, sizeof line, "b %s %d\n", kv.first.c_str(), kv.second ? 1 : 0);   out += line; }
    for (auto& kv : st.u)  { snprintf(line, sizeof line, "u %s %u\n", kv.first.c_str(), (unsigned)kv.second); out += line; }
    for (auto& kv : st.ui) { snprintf(line, sizeof line, "i %s %lu\n", kv.first.c_str(), (unsigned long)kv.second); out += line; }
    for (auto& kv : st.ul) { snprintf(line, sizeof line, "l %s %llu\n", kv.first.c_str(), (unsigned long long)kv.second); out += line; }
    for (auto& kv : st.sh) { snprintf(line, sizeof line, "h %s %d\n", kv.first.c_str(), (int)kv.second);      out += line; }
    for (auto& kv : st.s)  { out += "s "; out += kv.first; out += ' '; out += kv.second; out += '\n'; }
    return out;
}

void deserialize(Fw2Nvs::Store& st, const char* blob, size_t len) {
    std::string all(blob, len);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t nl = all.find('\n', pos);
        if (nl == std::string::npos) nl = all.size();
        std::string line = all.substr(pos, nl - pos);
        pos = nl + 1;
        while (!line.empty() && (line.back() == '\r')) line.pop_back();
        if (line.size() < 4 || line[1] != ' ') continue;
        const size_t sp = line.find(' ', 2);
        if (sp == std::string::npos) continue;
        const std::string key = line.substr(2, sp - 2), val = line.substr(sp + 1);
        switch (line[0]) {
            case 'b': st.b[key]  = atoi(val.c_str()) != 0; break;
            case 'u': st.u[key]  = (uint8_t)strtoul(val.c_str(), nullptr, 10); break;
            case 'i': st.ui[key] = (uint32_t)strtoul(val.c_str(), nullptr, 10); break;
            case 'l': st.ul[key] = (uint64_t)strtoull(val.c_str(), nullptr, 10); break;
            case 'h': st.sh[key] = (int16_t)atoi(val.c_str()); break;
            case 's': st.s[key]  = val; break;
            default: break;
        }
    }
}
}  // namespace

Fw2Nvs::Store& Fw2Nvs::open(const char* ns) {
    auto it = s_ns.find(ns);
    if (it != s_ns.end()) return it->second;
    Store& st = s_ns[ns];
    ow_device* dev = fw2_link();
    if (dev && fw2_card_ok()) {
        static char buf[16384];
        size_t got = 0;
        // A .new left behind is a save the power interrupted after it was complete.
        if (ow_sd_get_mem(dev, nvsPath(ns).c_str(), buf, sizeof buf, &got) == OW_OK ||
            ow_sd_get_mem(dev, (nvsPath(ns) + ".new").c_str(), buf, sizeof buf, &got) == OW_OK)
            deserialize(st, buf, got);
    }
    return st;
}

void Fw2Nvs::touched(const char* ns) { s_dirtyAt[ns] = millis(); }

// From the main loop: namespaces that have been quiet for half a second are
// written out, each as a new file that then replaces the old, so a card
// pulled mid-write keeps the previous settings.
void fw2_nvs_flush(bool all) {
    if (s_dirtyAt.empty()) return;
    const uint32_t now = millis();
    for (auto it = s_dirtyAt.begin(); it != s_dirtyAt.end();) {
        if (!all && now - it->second < SAVE_AFTER_MS) { ++it; continue; }
        Fw2Nvs::Store& st = s_ns[it->first];
        if (st.dirty) {
            const std::string path = nvsPath(it->first), tmp = path + ".new";
            if (writeFile(tmp, serialize(st), false)) {
                ow_sd_remove(fw2_link(), path.c_str());
                ow_sd_rename(fw2_link(), tmp.c_str(), path.c_str());
            }
            st.dirty = false;
        }
        it = s_dirtyAt.erase(it);
    }
}
