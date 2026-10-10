#pragma once

// =====================================================================================
// rw::audio::core::Pcm16BigEnc -- the 'P6B0' encoder: interleaved f32 frames to 16-bit PCM
// (the inverse of Pcm16BigDec). No buffering: every Encode call converts all it is given,
// so Flush has nothing to emit. The instance is just the Encoder base (console sizeof 0x20).
//
// Reconstructed from the console image; its PowerPC asm is authoritative. See
// Encoder.h for the shared layout and v-table.
// =====================================================================================

#include "rw/audio/core/Encoder.h"

namespace rw
{
namespace audio
{
namespace core
{

class Pcm16BigEnc : public Encoder
{
public:
    // Allocate and seed an instance from pSystem. The data rate is the product of the
    // first two arguments (channel count and sample rate; only the product is observed)
    // times 2 bytes per sample. Returns null when the allocation fails.
    static Pcm16BigEnc *CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem);

    s32 Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples, s32 *piBytesWritten,
               s32 iArg5, s32 *piAuxOut) override;
    s32 Flush(void *pOutput, s32 *piBytesWritten, s32 iArg3, s32 *piAuxOut) override;
};

} // namespace core
} // namespace audio
} // namespace rw
