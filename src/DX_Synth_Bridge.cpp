#include "DX_Synth_Bridge.h"
#include "pico/stdlib.h"
#include "ram_hot.h"

void DX_Synth_Bridge::init() {
    synth_.init();
    fxHost_.init((float)SAMPLE_RATE);
    synth_.applyPatch(synth_.DigiChordPatch());
}

void RAM_HOT(DX_Synth_Bridge::fill_buffer)(float* buffer, int length) {
    uint32_t t0 = time_us_32();
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

        // Post-mix effects (2 slots: Thru/Distortion/Touch Wah/Chorus/Flanger/Phaser/Delay/Reverb)
        fxHost_.process(scratchL_, scratchR_, chunkLen);

        // Interleave planar data into the output buffer
        int offset = framesRendered * 2;
        for (int i = 0; i < chunkLen; ++i) {
            buffer[offset + (2 * i)]     = scratchL_[i];
            buffer[offset + (2 * i) + 1] = scratchR_[i];
        }

        framesRendered += chunkLen;
    }

    uint32_t elapsedUs = time_us_32() - t0;
    float budgetUs = (float)length * 1000000.0f / (float)SAMPLE_RATE;
    cpuLoadPercent_ = (elapsedUs / budgetUs) * 100.0f;
    if (cpuLoadPercent_ > cpuLoadPeakPercent_) cpuLoadPeakPercent_ = cpuLoadPercent_;
}
