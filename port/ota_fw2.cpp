// SquachWatch on the FREE-WILi 2: no over-the-air updates.
//
// SquachWatch updates itself by writing a signed image into the ESP32's
// second flash slot, over Wi-Fi or Bluetooth. On the FW2 it is a UF2 app on
// the display processor, installed from the SD card like any other, and the
// radios belong to the ESP32-C5's stock firmware. So OtaCore::available()
// is false, which takes the UPDATE rows out of the settings menu, and
// everything else here is inert. (Shaped after the simulator's
// sim/ota_sim.cpp, GPL-3.0.)
#include "ota_core.h"
#include "ota_ble.h"
#include "ota_wifi.h"
#include <Arduino.h>
#include <string.h>

#ifndef FW2_PORT_VERSION
#define FW2_PORT_VERSION "fw2"
#endif

namespace OtaCore {
const char* failWords(Fail) { return "Updates come from the FW2's SD card: install the new app there."; }
bool        available()                        { return false; }
void        boot()                             {}
void        tick(uint32_t)                     {}
const char* takeBootNote(const char**, bool*)  { return nullptr; }
const char* runningSlot()                      { return "fw2"; }
const char* runningVersion()                   { return FW2_PORT_VERSION; }
void        noteAvailable(const char*, const char*) {}
const char* availableVersion()                 { return ""; }
const char* availableFrom()                    { return ""; }
void        noteRelease(const char*, const char* const*, uint8_t) {}
const char* releaseName()                      { return ""; }
uint8_t     newsCount()                        { return 0; }
const char* newsAt(uint8_t)                    { return ""; }
bool        takeAvailableNotice()              { return false; }
const char* buildName()                        { return "freewili2"; }
uint32_t    maxImageSize()                     { return 0; }
void        refreshOther()                     {}
const char* otherVersion()                     { return ""; }
Fail        switchToOther()                    { return Fail::NOT_FIRMWARE; }
void        restartSoon(uint32_t)              {}
bool        restartPending()                   { return false; }
Fail        begin(uint32_t, const uint8_t*, uint8_t) { return Fail::NOT_FIRMWARE; }
bool        write(const uint8_t*, size_t)      { return false; }
uint32_t    written()                          { return 0; }
Fail        finish()                           { return Fail::NOT_FIRMWARE; }
void        abort()                            {}
}  // namespace OtaCore

namespace OtaBle {
bool        available()     { return false; }
bool        begin()         { return false; }
void        end()           {}
void        tick(uint32_t)  {}
State       state()         { return State::OFF; }
bool        codeAccepted()  { return false; }
uint32_t    bytesExpected() { return 0; }
uint32_t    bytesReceived() { return 0; }
uint8_t     percent()       { return 0; }
const char* failureText()   { return OtaCore::failWords(OtaCore::Fail::NOT_FIRMWARE); }
const char* deviceName()    { return ""; }
uint32_t    pairingCode()   { return 0; }
}  // namespace OtaBle

namespace OtaWifi {
bool        begin()                              { return false; }
bool        end()                                { return false; }
void        tick(uint32_t)                       {}
void        rescan()                             {}
State       state()                              { return State::OFF; }
uint8_t     netCount()                           { return 0; }
const Net*  net(uint8_t)                         { return nullptr; }
bool        hasSaved()                           { return false; }
bool        bootCheck(uint32_t)                  { return false; }
bool        savedPassAt(uint8_t, char*, size_t)  { return false; }
void        forget()                             {}
uint8_t     savedCount()                         { return 0; }
const char* savedSsidAt(uint8_t)                 { return ""; }
uint8_t     savedUse()                           { return 0; }
int8_t      savedIndexOf(const char*)            { return -1; }
SavedResult savedResult(uint8_t)                 { return SavedResult::UNTRIED; }
bool        saveNetwork(const char*, const char*) { return false; }
void        removeSaved(uint8_t)                 {}
void        useSaved(uint8_t)                    {}
void        printSaved()                         {}
void        connectSavedAt(uint8_t)              {}
void        connect(const char*, const char*, bool) {}
void        connectSaved()                       {}
const char* network()                            { return ""; }
const char* latestVersion()                      { return ""; }
bool        upToDate()                           { return true; }
void        install()                            {}
bool        canTryAgain()                        { return false; }
void        tryAgain()                           {}
uint32_t    bytesExpected()                      { return 0; }
uint32_t    bytesReceived()                      { return 0; }
uint8_t     percent()                            { return 0; }
const char* failureText()                        { return OtaCore::failWords(OtaCore::Fail::NOT_FIRMWARE); }
}  // namespace OtaWifi
