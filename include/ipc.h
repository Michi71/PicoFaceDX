// include/ipc.h
#ifndef __IPC_H__
#define __IPC_H__

#include <stdint.h>
#include "pico/multicore.h"

enum IpcCommand : uint8_t {
    IPC_CMD_FLASH_LOCK   = 0x0A,  // Core 1 asks Core 0 to park in RAM during flash write
    IPC_CMD_DX_NOTE_ON   = 0x0B,
    IPC_CMD_DX_NOTE_OFF  = 0x0C,
    IPC_CMD_DX_PARAM     = 0x0D,
    IPC_CMD_DX_PITCH_BEND = 0x0E,
    IPC_CMD_DX_CC        = 0x0F,
    IPC_CMD_DX_RAW_WRITE = 0x10,  // blockSel: 0=system(unused, system block stays Core-1-local), 1=common, 2..5=operator 0..3
    IPC_CMD_DX_PATCH_APPLY = 0x11,
    IPC_CMD_DX_MASTER_TUNE = 0x12
};

enum DxParamId : uint8_t {
    DX_PARAM_OP1_FREQ      = 0,
    DX_PARAM_OP1_LEVEL     = 1,
    DX_PARAM_OP2_FREQ      = 2,
    DX_PARAM_OP2_LEVEL     = 3,
    DX_PARAM_OP3_FREQ      = 4,
    DX_PARAM_OP3_LEVEL     = 5,
    DX_PARAM_OP4_FREQ      = 6,
    DX_PARAM_OP4_LEVEL     = 7,
    DX_PARAM_LFO_SPEED     = 8,
    DX_PARAM_LFO_PMD       = 9,
    DX_PARAM_ALGO          = 10,
    DX_PARAM_OP1_FEEDBACK  = 11
};

static inline uint32_t ipc_pack(uint8_t type, uint8_t d1, uint16_t d2) {
    return ((uint32_t)type << 24) | ((uint32_t)d1 << 16) | (uint32_t)d2;
}

static inline uint8_t ipc_type(uint32_t p) {
    return (uint8_t)(p >> 24);
}

static inline uint8_t ipc_d1(uint32_t p) {
    return (uint8_t)(p >> 16);
}

static inline uint16_t ipc_d2(uint32_t p) {
    return (uint16_t)(p & 0xFFFF);
}

static inline uint16_t ipc_f_to_u16(float f) {
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return (uint16_t)(f * 65535.0f + 0.5f);
}

static inline float ipc_u16_to_f(uint16_t u) {
    return (float)u / 65535.0f;
}

static inline void ipc_send_flash_lock(void) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_FLASH_LOCK, 0, 0));
}

static inline void ipc_send_dx_note_on(uint8_t note, uint8_t vel) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_NOTE_ON, note, vel));
}

static inline void ipc_send_dx_note_off(uint8_t note) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_NOTE_OFF, note, 0));
}

static inline void ipc_send_dx_param(uint8_t paramId, uint8_t value) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_PARAM, paramId, value));
}

static inline void ipc_send_dx_pitch_bend(uint16_t bend14) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_PITCH_BEND, 0, bend14));
}

static inline void ipc_send_dx_cc(uint8_t cc, uint8_t value) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_CC, cc, value));
}

// blockSel: 0=system(unused, system block stays Core-1-local), 1=common, 2..5=operator 0..3
static inline void ipc_send_dx_raw_write(uint8_t blockSel, uint8_t byteOffset, uint8_t value) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_RAW_WRITE, byteOffset, ((uint16_t)blockSel << 8) | value));
}

static inline uint8_t ipc_raw_write_block_sel(uint32_t p) {
    return (uint8_t)(ipc_d2(p) >> 8);
}

static inline uint8_t ipc_raw_write_value(uint32_t p) {
    return (uint8_t)(ipc_d2(p) & 0xFF);
}

static inline void ipc_send_dx_patch_apply(void) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_PATCH_APPLY, 0, 0));
}

// rawTune: 16-bit combined value from the 4 SYSTEM Master Tune nibbles
// (SYS_TUNE_0..3), i.e. (tune0<<12)|(tune1<<8)|(tune2<<4)|tune3. Core 0
// decodes this into cents/semitones (see ipc_apply() in main.cpp).
static inline void ipc_send_dx_master_tune(uint16_t rawTune) {
    multicore_fifo_push_blocking(ipc_pack(IPC_CMD_DX_MASTER_TUNE, 0, rawTune));
}

#endif // __IPC_H__
