#pragma once

// =====================================================================================
// rw::audio::core::XasEnc -- the 'Xas0' encoder, the inverse of XasDec. XAS0 codes each
// channel in 32-frame blocks of 19 bytes: two 16-bit header words (the first two samples,
// rounded to 12 bits, carrying the predictor index and the shift in their low nibbles)
// followed by thirty 4-bit residuals. A block of every channel is laid out channel-
// interleaved: all channels' first header words, then all channels' second header words,
// then the residual bytes with a stride of one byte per channel.
//
// Encode buffers a partial block between calls (up to 32 frames of up to 6 channels);
// Flush pads that partial block by repeating its last frame and codes it.
//
// Reconstructed from the console image; its PowerPC asm is authoritative. See
// Encoder.h for the shared layout and v-table. Appended members (console offsets):
//   +0x20  miBufferedSamples   frames waiting in mafBuffer
//   +0x24  mafBuffer           32 interleaved frames of up to 6 channels (console sizeof 0x324)
// =====================================================================================

#include "rw/audio/core/Encoder.h"

namespace rw
{
namespace audio
{
namespace core
{

class XasEnc : public Encoder
{
public:
    enum
    {
        KI_BLOCK_SAMPLES = 32,         // frames per block
        KI_BLOCK_BYTES = 19,           // bytes per channel per block
        KI_MAX_BUFFERED_CHANNELS = 6   // the partial-block buffer's channel capacity
    };

    // Allocate and seed an instance from pSystem; the data rate is the product of the first
    // two arguments times 19/32 bytes per sample. Returns null when the allocation fails.
    static XasEnc *CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem);

    s32 Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples, s32 *piBytesWritten,
               s32 iArg5, s32 *piAuxOut) override;
    s32 Flush(void *pOutput, s32 *piBytesWritten, s32 iArg3, s32 *piAuxOut) override;

    // Code one 32-frame block of iNumChannels interleaved channels from pafFrames into
    // 19 * iNumChannels bytes at pOutput.
    void EncodeBlock(const f32 *pafFrames, u8 *pOutput, s32 iNumChannels);

    s32 miBufferedSamples;                                          // +0x20
    f32 mafBuffer[KI_BLOCK_SAMPLES * KI_MAX_BUFFERED_CHANNELS];     // +0x24
};

} // namespace core
} // namespace audio
} // namespace rw
