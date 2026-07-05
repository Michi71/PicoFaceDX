#include "DX_Synth_Bridge.h"
#include "cp_hot.h"

void DX_Synth_Bridge::init() {
    synth_.init();
    synth_.applyPatch(synth_.DigiChordPatch());
}

void CP_HOT(DX_Synth_Bridge::fill_buffer)(float* buffer, int length) {
    // Refresh cached parameters once per block
    synth_.updateCache();

    int framesRendered = 0;
    while (framesRendered < length) {
        // Determine chunk size, capped at DMA_BUFFER_LEN (64)
        int chunkLen = length - framesRendered;
        if (chunkLen > DMA_BUFFER_LEN) {
            chunkLen = DMA_BUFFER_LEN;
        }

        // Render planar audio into scratch buffers
        synth_.renderAudioBlock(scratchL_, scratchR_, chunkLen);

        // Interleave planar data into the output buffer
        int offset = framesRendered * 2;
        for (int i = 0; i < chunkLen; ++i) {
            buffer[offset + (2 * i)]     = scratchL_[i];
            buffer[offset + (2 * i) + 1] = scratchR_[i];
        }

        framesRendered += chunkLen;
    }
}
