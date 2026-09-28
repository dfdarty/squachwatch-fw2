// SquachWatch on the FREE-WILi 2: the Arduino SD library, as sd_log.cpp
// uses it, on the SD card in the MAIN processor's slot (WiliBSP's OneWili
// link). SquachWatch's card root is the app's own folder,
// /appdata/squachwatch, so its daily logs land there rather than at the top
// of the FW2's card.
//
// A file opened for writing collects what is printed and writes it on
// close(), in one append: sd_log.cpp opens, prints one line and closes, per
// detection.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <SPI.h>          // as Arduino's SD.h: sd_log.cpp starts the CYD's SD bus with SPI.begin()

#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

class File {
public:
    File() {}
    explicit operator bool() const { return _open; }
    size_t print(const char* s);
    size_t println(const char* s = "");
    size_t write(const uint8_t* b, size_t n);
    size_t write(uint8_t c) { return write(&c, 1); }
    void   flush();
    void   close();
    const char* name() const { return _name.c_str(); }
    bool   isDirectory() const { return _dir; }
    size_t size() const { return _size; }
    File   openNextFile();
private:
    friend class SDClass;
    bool _open = false, _dir = false, _append = false;
    std::string _path, _name, _pending;
    size_t _size = 0;
    std::vector<std::string> _entries;     // a directory's names, read once
    std::vector<uint32_t>    _sizes;
    std::vector<bool>        _isDir;
    size_t _next = 0;
};

class SDClass {
public:
    template <class... A> bool begin(A&&...) { return mount(); }
    void     end() {}
    uint64_t cardSize();
    File     open(const char* path, const char* mode = FILE_READ);
    bool     exists(const char* path);
    bool     remove(const char* path);
    bool     mkdir(const char* path);
    bool     rmdir(const char* path) { return remove(path); }
private:
    bool mount();
};
extern SDClass SD;
