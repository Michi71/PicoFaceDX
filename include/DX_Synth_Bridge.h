#ifndef DX_SYNTH_BRIDGE_H
#define DX_SYNTH_BRIDGE_H

#include "dx_engine/RDX_Synth.h"
#include "dx_engine/DX_FXHost.h"
#include <cstdint>
#include <cmath>

// DX_Synth_Bridge: Wraps the RDX_Synth FM engine for the Pico audio task.
// NOTE: noteOn()/noteOff()/fill_buffer() are audio-rate/mutating and must only be
// called from Core 0 (the audio DMA IRQ context). Core 1 code may safely call patch() for READ-ONLY access (matching
// how pico_frontpanel.cpp reads engine getters directly); writing patch fields
// from Core 1 must go through IPC_CMD_DX_PARAM (ipc_send_dx_param), see
// DX_Controller and ipc_apply() in main.cpp.

class DX_Synth_Bridge {
public:
    void init();

    // Main audio entry point. buffer is interleaved stereo (L,R,L,R...), length is number of frames.
    void fill_buffer_i32(int32_t* out, int length);

    // Trivial forwards kept inline
    inline void noteOn(uint8_t note, uint8_t velocity) {
        synth_.noteOn(note, velocity);
    }

    inline void noteOff(uint8_t note) {
        synth_.noteOff(note);
    }

    inline void processCC(uint8_t cc, uint8_t val) {
        synth_.processCC(0, cc, val);
    }

    inline void updatePB(int bend) {
        synth_.updatePB(0, bend);
    }

    inline void setMasterTune(float semitones) {
        synth_.setMasterTune(semitones);
    }

    inline RDX_Patch& patch() {
        return synth_.currentPatch();
    }

    // CPU load of the most recently rendered audio block, and the peak value
    // observed since boot, as a percentage of the real-time budget for that
    // block (100% = fill_buffer() takes exactly as long as the audio itself
    // lasts). Written by fill_buffer() on Core 0; Core 1 may read these
    // directly for on-screen display (same convention as patch()).
    inline float cpuLoadPercent() const { return cpuLoadPercent_; }
    inline float cpuLoadPeakPercent() const { return cpuLoadPeakPercent_; }

private:
    RDX_Synth synth_;
    DX_FXHost fxHost_;
    // Fixed scratch buffers to avoid heap allocation in the audio path
    // Soft-clip a normalized sample to (-1, 1): transparent below 0.9,
    // smoothly saturates toward +/-1.0 above (moved here from main.cpp;
    // fused into fill_buffer_i32 to avoid a second output pass).
    static inline float softClipSample(float x) {
        constexpr float kSoftClipThreshold = 0.9f;
        constexpr float kSoftClipRange = 1.0f - kSoftClipThreshold;
        float ax = fabsf(x);
        if (ax <= kSoftClipThreshold) return x;
        float excess = ax - kSoftClipThreshold;
        float y = kSoftClipThreshold + kSoftClipRange * (excess / (excess + kSoftClipRange));
        return (x < 0.0f) ? -y : y;
    }

    float scratchL_[DMA_BUFFER_LEN];
    float scratchR_[DMA_BUFFER_LEN];
    float cpuLoadPercent_ = 0.0f;
    float cpuLoadPeakPercent_ = 0.0f;
};

#endif // DX_SYNTH_BRIDGE_H
