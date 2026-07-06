// include/settings.h
//
// PicoFaceDX — persisted settings snapshot (virtual EEPROM).
//
// Persisted state:
//   - UI octave transpose (-2..+2)
//   - RefaceMidi SYSTEM common block (RX channel, master tune/transpose,
//     local control, MIDI control, etc.), 32 bytes.
//   - Full current DX patch (RDX_Patch: common + 4 operators)
//
// Autosave policy:
//   Core 1 polls every 250 ms, builds a full snapshot, and if it differs
//   from the last saved image and remains stable for 2000 ms, writes one
//   veeprom record ("virtual potentiometer memory").
//
#pragma once

#include <stdint.h>
#include "dx_engine/RDX_Types.h"

#define SETTINGS_VERSION         2
#define SETTINGS_SYS_BLOCK_SIZE  32   // RefaceMidi SYSTEM common block (SYS_BLOCK_SIZE)

class DX_Synth_Bridge;
class RefaceMidi;

// Persisted settings snapshot, version 2 (version 1 was the reface-CP layout, removed).
struct __attribute__((packed)) SettingsV2 {
    int8_t    octave;                             // -2..+2 UI octave transpose
    uint8_t   sysBlock[SETTINGS_SYS_BLOCK_SIZE];   // RefaceMidi SYSTEM common image
    RDX_Patch patch;                               // full current DX patch (common + 4 operators)
};

static_assert(sizeof(SettingsV2) <= 240, "must fit VEEPROM_MAX_PAYLOAD");

// Core 0, main(): veeprom_init() + load; if a valid V2 record exists, apply
// octave and patch directly to dx. MUST run after dxBridge.init() and BEFORE
// multicore_launch_core1() (single-core phase, plain XIP reads).
void settings_boot_restore_core0(DX_Synth_Bridge* dx);

// Core 1, core1_main(): apply the Core-1-owned part of the loaded record: UI
// octave (ui_set_octave) and the RefaceMidi SYSTEM block. Call after
// refaceMidi.init().
void settings_boot_restore_core1(RefaceMidi* rm);

// Core 1, called continuously from ui_poll_usb(): every 250 ms gathers a
// snapshot of all persisted values (dx->patch() read-only + ui_get_octave +
// SYSTEM block); when the snapshot differs from the last saved image and has
// been stable for 2000 ms, writes one veeprom record ("virtual potentiometer
// memory" autosave).
void settings_task(DX_Synth_Bridge* dx, RefaceMidi* rm);
