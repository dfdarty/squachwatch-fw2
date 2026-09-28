# SquachWatch for the FREE-WILi 2: SquachWatch-CYD's own sources (the
# submodule, unmodified) with the FREE-WILi 2 underneath them in port/.
# Builds for the FREE-WILi 2 emulator and, with the Pico SDK and Arm GCC,
# into a UF2 for the board.
set(SQW ${CMAKE_CURRENT_LIST_DIR}/SquachWatch-CYD)
if(NOT EXISTS ${SQW}/src/main.cpp)
    message(FATAL_ERROR "SquachWatch-CYD is missing: git submodule update --init")
endif()

# SquachWatch's version, as its PlatformIO build stamps it, plus this port.
execute_process(COMMAND git -C ${SQW} describe --tags --always --dirty
                OUTPUT_VARIABLE SQW_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT SQW_VERSION)
    set(SQW_VERSION "unknown")
endif()

# Everything SquachWatch's simulator builds of the firmware (sim/Makefile),
# plus the parts it leaves out because they need a radio or a card:
# detection.cpp and sd_log.cpp. Not the mesh cipher: see
# port/meshcrypto_fw2.cpp. Not built: the
# CrowPanel and T-Watch hardware, the benches, and the over-the-air update
# (port/ota_fw2.cpp stands in).
set(SQW_SOURCES
    theme squachy squachmesh mesh meshmsg emote_script crowd_bench meshwords meshtalk
    meshtutor ui_phone qwerty ui_meshmenu ui_meshwarn ui_beaconwarn security
    ui_security ui_squad ui_meshphrase ui_meshcompose settings signatures remote_id clock
    blackbox log_index bingo ui_bingo dex regulars notices ui_dex detection_info device_info
    idle_events ui_clear ui_power ui_light status_light pet ui_log ui_alert ui_settings
    ui_boot ui_colorcheck ui_diagnostics ui_desk ui_zone ui_diary ui_hunt ui_outfit
    ui_outfit_unlock ignore_list ui_ignorelist ui_detfilter ui_rawscan ui_watchalert
    ui_update ui_nudge ui_squadupdate ui_invite ui_wifipass ui_sysprops ui_wifinets
    frame_push frame_prof fast_sprite
    main cap_touch touch_cal detection sd_log)
list(TRANSFORM SQW_SOURCES PREPEND ${SQW}/src/)
list(TRANSFORM SQW_SOURCES APPEND .cpp)

add_executable(squachwatch
    ${SQW_SOURCES}
    port/platform_fw2.cpp
    port/radio_fw2.cpp
    port/sd_fw2.cpp
    port/ota_fw2.cpp
    port/meshcrypto_fw2.cpp)

# port/ first: its Arduino.h, Preferences.h, SD.h, WiFi.h, esp_*.h and
# NimBLE headers are the FW2's. Then SquachWatch's headers, then its
# simulator's, for the in-memory TFT_eSPI panel, the XPT2046 touch reader,
# the fonts and the inert SPI/Wire.
target_include_directories(squachwatch PRIVATE ${CMAKE_CURRENT_LIST_DIR}/port ${SQW}/include ${SQW}/sim)
target_compile_features(squachwatch PRIVATE cxx_std_17)
target_compile_definitions(squachwatch PRIVATE
    _GNU_SOURCE     # as ESP-IDF builds: newlib declares strcasestr() only with it
    SQUACH_MESH=1   # as every SquachWatch board: the code doesn't build without it (see port/meshcrypto_fw2.cpp)
    FIRMWARE_VERSION="${SQW_VERSION}-fw2"
    FW2_PORT_VERSION="${SQW_VERSION}-fw2")
# As the Arduino build does, and the simulator's live build: Arduino.h in
# every file, and the CYD's user setup for the TFT_/touch macros main.cpp
# expects (the plain XPT2046 path; the FW2's panel size is set at start-up).
target_compile_options(squachwatch PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include Arduino.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${SQW}/include/cyd_user_setup.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>")
# The names of SquachWatch's screens (enum class AppState in state.h), so the
# port can log which one is showing: generated from the header, so a screen
# SquachWatch adds is named without touching the port.
file(READ ${SQW}/include/state.h _state_h)
string(REGEX MATCH "enum class AppState[^{]*{([^}]*)}" _m "${_state_h}")
string(REGEX REPLACE "//[^\n]*" "" _body "${CMAKE_MATCH_1}")
string(REGEX MATCHALL "[A-Z][A-Z0-9_]+" _names "${_body}")
set(_cases "")
foreach(_n IN LISTS _names)
    string(APPEND _cases "        case AppState::${_n}: return \"${_n}\";\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fw2_state_names.h CONTENT
"// Generated from SquachWatch-CYD/include/state.h by CMakeLists.txt.
#pragma once
#include \"state.h\"
inline const char* fw2StateName(AppState s) {
    switch (s) {
${_cases}        default: return \"?\";
    }
}
")
target_include_directories(squachwatch PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(squachwatch freewili2_bsp onewili_fwgui pico_rand pico_unique_id)

fw2_psram_app(squachwatch
    POWER_ZONES DISPLAY SDCARD
    NAME [=[SquachWatch]=]
    VERSION 001
    DESCRIPTION [=[Surveillance-device detector (SquachWatch port): Wi-Fi and Bluetooth scans]=])
