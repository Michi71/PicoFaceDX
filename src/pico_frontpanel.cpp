// pico_frontpanel.cpp
//
// Three-encoder paged "virtual front panel" for the Reface DX
// (RP2350 + SH1106 128x64 OLED).
//
//   - SELECTOR encoder pages through the DX pages (OP1-4/LFO/ALGO).
//   - PARAM A and PARAM B encoders edit the current page's two values.
//   - Long press (>= 500 ms) of the selector opens the main menu.

#include "pico_frontpanel.h"
#include "pico_userinterface.h"
#include "pico/stdlib.h"
#include "midi_reface.h"
#include "DX_GUI.h"
#include "DX_Synth_Bridge.h"
#include "presets.h"
#include <cstdio>
#include <cstring>

extern RefaceMidi refaceMidi;
extern DX_Controller dxController;
extern DX_Synth_Bridge dxBridge;

#define LONG_PRESS_MS 500

/* ------------------------------------------------------------------ */
/* Menu helpers (use selector encoder / button)                       */
/* ------------------------------------------------------------------ */
static void showAbout(u8g2_t* u, Encoder* enc, PushButton* bt)
{
    (void)enc;
    for(;;){
        u8g2_FirstPage(u);
        do {
            u8g2_SetFont(u, u8g2_font_8x13B_tf);
            u8g2_SetFontPosBaseline(u);
            u8g2_DrawStr(u, 4, 14, "ABOUT");
            u8g2_DrawHLine(u, 0, 18, 128);
#ifdef PICO_PROGRAM_NAME
            u8g2_DrawStr(u, 4, 36, PICO_PROGRAM_NAME);
#else
            u8g2_DrawStr(u, 4, 36, "Reface DX");
#endif
#ifdef PICO_PROGRAM_VERSION_STRING
            u8g2_DrawStr(u, 4, 52, PICO_PROGRAM_VERSION_STRING);
#else
            u8g2_DrawStr(u, 4, 52, "v1.0");
#endif
            u8g2_SetFont(u, u8g2_font_6x10_tf);
            u8g2_DrawStr(u, 4, 62, "Press any button");
        } while (u8g2_NextPage(u));

        ui_poll_usb();
        if (bt->ReadButton() == PushButton::PRESSED) {
            ui_wait_button_release(bt);
            break;
        }
    }
}

static void showCpuLoad(u8g2_t* u, Encoder* enc, PushButton* bt)
{
    (void)enc;
    char buf[32];
    for(;;){
        u8g2_FirstPage(u);
        do {
            u8g2_SetFont(u, u8g2_font_8x13B_tf);
            u8g2_SetFontPosBaseline(u);
            u8g2_DrawStr(u, 4, 14, "CPU LOAD");
            u8g2_DrawHLine(u, 0, 18, 128);
            u8g2_SetFont(u, u8g2_font_6x10_tf);
            snprintf(buf, sizeof(buf), "Now:  %d %%", (int)dxBridge.cpuLoadPercent());
            u8g2_DrawStr(u, 4, 36, buf);
            snprintf(buf, sizeof(buf), "Peak: %d %%", (int)dxBridge.cpuLoadPeakPercent());
            u8g2_DrawStr(u, 4, 52, buf);
            u8g2_DrawStr(u, 4, 62, "Press any button");
        } while (u8g2_NextPage(u));

        ui_poll_usb();
        if (bt->ReadButton() == PushButton::PRESSED) {
            ui_wait_button_release(bt);
            break;
        }
    }
}

static void openSystem(u8g2_t* u, Encoder* enc, PushButton* bt)
{
    uint8_t sel = pico_UserInterfaceSelectionList(u, enc, bt, "SYSTEM", 1, "About\nCPU Load\n<< BACK");
    if (sel == 1) showAbout(u, enc, bt);
    else if (sel == 2) showCpuLoad(u, enc, bt);
}

static void openPresets(u8g2_t* u, Encoder* enc, PushButton* bt)
{
    // 32 preset names (<=10 chars each) + "<< BACK", newline-joined.
    char buf[512];
    buf[0] = 0;
    for (int i = 0; i < DX_NPRESETS; i++) {
        strcat(buf, dxPresets[i].name);
        strcat(buf, "\n");
    }
    strcat(buf, "<< BACK");

    uint8_t start = (uint8_t)(preset_get_current() + 1);
    uint8_t sel = pico_UserInterfaceSelectionList(u, enc, bt, "PRESETS", start, buf);
    if (sel >= 1 && sel <= (uint8_t)DX_NPRESETS) {
        uint8_t idx = (uint8_t)(sel - 1);
        preset_set_current(idx);
        preset_stage(idx);
        refaceMidi.txProgram(idx);
    }
}

static void openMainMenu(u8g2_t* u, Encoder* enc, PushButton* bt)
{
    uint8_t sel = pico_UserInterfaceSelectionList(u, enc, bt, "MENU", 1, "Presets\nSystem\n<< BACK");
    if (sel == 1)      openPresets(u, enc, bt);
    else if (sel == 2) openSystem(u, enc, bt);
}

/* ------------------------------------------------------------------ */
/* Main front-panel interface -- DX home screen                       */
/* ------------------------------------------------------------------ */
void pico_UserInterfaceFrontPanel(u8g2_t* u8g2, Encoder* encSel, PushButton* btSel,
                                  Encoder* encA,  PushButton* btA,
                                  Encoder* encB,  PushButton* btB)
{
    (void)btA;
    (void)btB;
    bool selHeld = false;
    uint32_t selPressT = 0;

    for (;;) {
        u8g2_FirstPage(u8g2);
        do {
            dxDrawScreen(u8g2, dxController);
        } while (u8g2_NextPage(u8g2));

        ui_poll_usb();
        uint32_t now = to_ms_since_boot(get_absolute_time());

        bool selState = btSel->ReadButton();
        if (selState == PushButton::PRESSED) {
            if (!selHeld) { selHeld = true; selPressT = now; }
        } else {
            if (selHeld) {
                uint32_t dur = now - selPressT;
                selHeld = false;
                if (dur >= LONG_PRESS_MS) {
                    openMainMenu(u8g2, encSel, btSel);
                }
                continue;
            }
        }

        int32_t dSel = encSel->delta();
        int32_t dA   = encA->delta();
        int32_t dB   = encB->delta();
        if (dSel != 0) dxController.onEncoder1(dSel);
        if (dA   != 0) dxController.onEncoder2(dA);
        if (dB   != 0) dxController.onEncoder3(dB);
    }
}
