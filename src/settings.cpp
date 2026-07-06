/*
 * settings.cpp - Persistent settings management for PicoFaceDX firmware.
 *
 * This module handles saving and restoring the current DX patch and MIDI
 * system settings to virtual EEPROM. The restore process is split across
 * cores:
 * - Core 0 restores the DX engine patch before multicore launch.
 * - Core 1 restores the UI octave and Reface MIDI system block after RefaceMidi initialization.
 *
 * Autosave is polled on core 1. It uses a debounce policy: changes must remain
 * stable for 2 seconds before being written to flash, preventing excessive writes
 * during continuous parameter sweeps.
 */

#include "settings.h"
#include "veeprom.h"
#include "DX_Synth_Bridge.h"
#include "midi_reface.h"
#include <string.h>
#include "pico/time.h"

extern "C" void ui_set_octave(int oct);
extern "C" int  ui_get_octave(void);

static SettingsV2 g_loaded;
static bool g_loadedValid = false;
static SettingsV2 g_lastSaved;
static bool g_baselineInit = false;
static SettingsV2 g_pending;
static bool g_pendingActive = false;
static uint32_t g_pendingSinceMs = 0;
static uint32_t g_lastPollMs = 0;

static void settings_gather(SettingsV2* s, DX_Synth_Bridge* dx, RefaceMidi* rm) {
    memset(s, 0, sizeof(*s));
    s->octave = (int8_t)ui_get_octave();
    rm->getSystemBlock(s->sysBlock);
    s->patch = dx->patch();
}

void settings_boot_restore_core0(DX_Synth_Bridge* dx) {
    veeprom_init();
    SettingsV2 s;
    uint16_t len = 0, ver = 0;
    if (!veeprom_load(&s, sizeof(s), &len, &ver)) return;
    if (ver != SETTINGS_VERSION || len != sizeof(SettingsV2)) return;

    if (s.octave < -2) s.octave = -2;
    if (s.octave > 2) s.octave = 2;

    dx->patch() = s.patch;

    g_loaded = s;
    g_loadedValid = true;
}

void settings_boot_restore_core1(RefaceMidi* rm) {
    if (!g_loadedValid) return;
    ui_set_octave(g_loaded.octave);
    rm->loadSystemBlock(g_loaded.sysBlock);
}

void settings_task(DX_Synth_Bridge* dx, RefaceMidi* rm) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - g_lastPollMs < 250) return;
    g_lastPollMs = now;

    SettingsV2 cur;
    settings_gather(&cur, dx, rm);

    if (!g_baselineInit) {
        g_lastSaved = cur;
        g_baselineInit = true;
        return;
    }

    if (memcmp(&cur, &g_lastSaved, sizeof(cur)) == 0) {
        g_pendingActive = false;
        return;
    }

    if (!g_pendingActive || memcmp(&cur, &g_pending, sizeof(cur)) != 0) {
        g_pending = cur;
        g_pendingSinceMs = now;
        g_pendingActive = true;
        return;
    }

    if (now - g_pendingSinceMs >= 2000) {
        if (veeprom_save(&g_pending, sizeof(g_pending), SETTINGS_VERSION)) {
            g_lastSaved = g_pending;
        }
        g_pendingActive = false;
    }
}
