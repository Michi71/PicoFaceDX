#ifndef DX_SYNTH_BRIDGE_H
#define DX_SYNTH_BRIDGE_H

#include "dx_engine/RDX_Synth.h"

// DX_Synth_Bridge: Wraps the RDX_Synth FM engine for the Pico audio task.
// NOTE: noteOn()/noteOff()/fill_buffer() are audio-rate/mutating and must only be
// called from Core 0 (the audio DMA IRQ context), mirroring the existing mdaEPiano
// convention. Core 1 code may safely call patch() for READ-ONLY access (matching
// how pico_frontpanel.cpp reads engine getters directly); writing patch fields
// from Core 1 must go through IPC_CMD_DX_PARAM (ipc_send_dx_param), see
// DX_Controller and ipc_apply() in main.cpp.

class DX_Synth_Bridge {
public:
    void init();

    // Main audio entry point. buffer is interleaved stereo (L,R,L,R...), length is number of frames.
    void fill_buffer(float* buffer, int length);

    // Trivial forwards kept inline
    inline void noteOn(uint8_t note, uint8_t velocity) {
        synth_.noteOn(note, velocity);
    }

    inline void noteOff(uint8_t note) {
        synth_.noteOff(note);
    }

    inline RDX_Patch& patch() {
        return synth_.currentPatch();
    }

private:
    RDX_Synth synth_;
    // Fixed scratch buffers to avoid heap allocation in the audio path
    float scratchL_[DMA_BUFFER_LEN];
    float scratchR_[DMA_BUFFER_LEN];
};

#endif // DX_SYNTH_BRIDGE_H
