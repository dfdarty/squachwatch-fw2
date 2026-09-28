# SquachWatch for the FREE-WILi 2

[SquachWatch](https://github.com/skizzophrenic/SquachWatch-CYD), the
surveillance-device detector for the ESP32 "Cheap Yellow Display", running
on the [FREE-WILi 2](https://freewili.com). It uses SquachWatch's own
firmware sources, unmodified (the `SquachWatch-CYD` submodule), and supplies
the FREE-WILi 2 underneath them in `port/`: screen, touch, buttons, clock,
SD card and radios.

![the main screen](docs/main.png)
![a Flock camera found by its Wi-Fi scan](docs/alert.png)

All of SquachWatch's UI is here at the 480×320 of its 3.5" build, including
Squachy, the alert cards, HUNT, LOG, DESK, settings, the dex and diagnostics.
Detections come from the FREE-WILi 2's stock Wi-Fi and Bluetooth scans.

## What the stock radio can see

The FREE-WILi 2's radios are on its ESP32-C5, and FREE-WILi's stock
firmware there offers two things to an app: an access-point scan (BSSID,
signal, channel, security, SSID) and a Bluetooth LE scan (name, address,
signal). The port runs them in turn (3 s of Bluetooth, then a Wi-Fi scan).
It turns each network into the beacon frame SquachWatch's parser expects
and each Bluetooth device into an advert. So SquachWatch's own tables and
rules make every decision.

**Detected:** anything SquachWatch knows by a Wi-Fi hardware prefix or
network name, and Bluetooth devices by name:

- `FLOCK`
- `AXON`
- `ALPR`
- `CAMERA`
- `RING`
- `HACKER`: Flipper's prefix and name, `Pineapple_`, `pwned`
- `EVILTWIN`: one SSID, two BSSIDs that disagree about encryption
- the Bluetooth name rules, such as the Flock and skimmer names

**Not detected yet:** anything that needs more than a stock scan gives:

- the advert payload: service UUIDs and company IDs, which means AirTags,
  SmartTags, Google tags, Tile, iBeacons, Remote ID drones, camera glasses,
  Raven, and Flipper's UUIDs;
- raw 802.11 frames: deauth floods, probe requests, the Pwnagotchi beacon;
- Bluetooth Classic, for HC-05-style skimmers that don't advertise over BLE.

The plan for those is a custom ESP32-C5 firmware that passes raw frames
and adverts through. `port/radio_fw2.cpp` is the only file that changes,
because SquachWatch's callbacks already take raw frames.

The SquachWatch-to-SquachWatch mesh is off: its cipher needs mbedtls, and
the port refuses rather than fake it. Diagnostics will say
`crypto self-test FAIL`. Over-the-air updates are hidden; the FREE-WILi 2
installs apps its own way.

## Using it

- **Touch** works as on the CYD.
- **Grey, yellow, green** press SCAN, LOG and DESK on the button bar.
- **HOME held 5 s** leaves; **PAGE held 5 s** shows About.

On first start SquachWatch shows its walkthrough, then asks for your zone.
The clock comes from the FREE-WILi 2's real-time clock, and SquachWatch
writes it back when it learns the time.

On the SD card, under `/appdata/squachwatch/`:

- `squachwatch-<day>.log` holds SquachWatch's detection log, as on the CYD;
- `nvs/<namespace>.nvs` holds its settings (the ESP32's NVS, as text).

## Build it

```sh
git clone --recurse-submodules https://github.com/dfdarty/squachwatch-fw2
git clone --recurse-submodules https://github.com/dfdarty/freewili2-emu ~/freewili2-emu
cd squachwatch-fw2
~/freewili2-emu/tools/fw2emu run .                  # in a window; the SD card is ./sdcard
~/freewili2-emu/tools/fw2emu test .                 # run test.txt: PASS or FAIL
~/freewili2-emu/tools/fw2emu hwcheck --fetch-toolchain .   # fits the chip? also builds the UF2
```

This needs the emulator from 1.2.0 on, for the radio model and C++ apps.
In the emulator, `--radio @town` fills the air with a small town's worth
of cameras, a Pineapple and a Flipper. `radio ap …` and `radio ble …` in a
script add more (see the emulator's
[scripting guide](https://dfdarty.github.io/freewili2-emu/scripting/)).

`test.txt` starts with two ordinary networks and two Bluetooth devices,
then brings in a Flock camera and a Flipper. It checks both alerts, the
log lines on the SD card and the saved settings. It runs under
AddressSanitizer too (`fw2emu test --sanitize .`).

On the real chip (`fw2emu hwcheck`): 926 KB image, 151 KB of SRAM, 22.6 KB
of stack, and a 5 MB heap in PSRAM.

## How it's built

- `CMakeLists.txt` compiles SquachWatch's `src/`, the same files its PC
  simulator builds, plus `detection.cpp` and `sd_log.cpp`. It uses
  SquachWatch's simulator headers for the in-memory TFT_eSPI panel and the
  XPT2046 touch code.
- `port/platform_fw2.cpp` handles the board:
  - runs `setup()` and `loop()`;
  - after each loop, sends the rows that changed to the LCD by DMA;
  - turns capacitive touch into the raw readings SquachWatch's calibration
    expects;
  - puts the heap in PSRAM.
- `port/radio_fw2.cpp` handles the scans, as above.
- `port/sd_fw2.cpp` provides SD and NVS over the MAIN processor's card.
- `port/*.h` stand in for Arduino, ESP-IDF and NimBLE, as much as
  SquachWatch uses.

## License

GPL-3.0, as SquachWatch. See [LICENSE](LICENSE). SquachWatch, Squachy and
SquachWare are the work of TalkingSasquach and SquachWatch's contributors.
This is an unofficial port, not affiliated with SquachWatch or FREE-WILi
LLC.
