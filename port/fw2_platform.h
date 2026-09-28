// SquachWatch on the FREE-WILi 2: what the port's files share.
#pragma once
#include <cstdint>

struct ow_device;

ow_device* fw2_link();            // WiliBSP's OneWili link to the MAIN processor, or nullptr
bool       fw2_card_ok();         // an SD card answered
void       fw2_radio_pump();      // radio_fw2.cpp: drain scan results, start the next scan

struct Fw2RadioStats {
    uint32_t wifiScans, bleScans;   // scans completed / started
    uint32_t networks, devices;     // results received
    uint32_t beacons;               // beacons handed to SquachWatch's sniffer callback
    uint32_t refusals;              // scans the MAIN processor refused
};
Fw2RadioStats fw2_radio_stats();

// sd_fw2.cpp: SquachWatch's card paths, under the app's own folder.
#define FW2_APP_DIR "/appdata/squachwatch"
