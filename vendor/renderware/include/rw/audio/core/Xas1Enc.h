#pragma once

// =====================================================================================
// rw::audio::core::Xas1Enc -- the 'Xas1' encoder, the inverse of Xas1Dec. XAS1 codes each
// channel in 128-frame blocks of 76 bytes: four 32-frame sub-blocks, each two 16-bit
// header words (the first two samples rounded to 12 bits, carrying the predictor index and
// the shift in their low nibbles) plus thirty 4-bit residuals. The channels of a block are
// stored one after another; inside a channel's 76 bytes CustomInterleaveXAS puts the four
// sub-blocks' headers first and then interleaves their residual bytes.
//
// Encode buffers a partial block between calls (up to 128 frames of up to 64 channels);
// Flush pads that partial block by repeating its last frame and codes it.
//
// Reconstructed from the console image; its PowerPC asm is authoritative. See
// Encoder.h for the shared layout and v-table. Appended members (console offsets):
//   +0x20  miBufferedSamples   frames waiting in mafBuffer
//   +0x24  mafBuffer           128 interleaved frames of up to 64 channels (console sizeof 0x8024)
// =====================================================================================

#include "rw/audio/core/Encoder.h"

namespace rw
{
namespace audio
{
namespace core
{

class Xas1Enc : public Encoder
{
public:
    enum
    {
        KI_BLOCK_SAMPLES = 128,        // frames per block
        KI_BLOCK_BYTES = 76,           // bytes per channel per block
        KI_SUBBLOCKS = 4,              // 32-frame sub-blocks per block
        KI_SUBBLOCK_BYTES = 19,        // bytes per sub-block before interleaving
        KI_MAX_BUFFERED_CHANNELS = 64  // the partial-block buffer's channel capacity
    };

    // Allocate and seed an instance from pSystem; the data rate is the product of the first
    // two arguments times 76/128 bytes per sample. Returns null when the allocation fails.
    static Xas1Enc *CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem);

    s32 Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples, s32 *piBytesWritten,
               s32 iArg5, s32 *piAuxOut) override;
    s32 Flush(void *pOutput, s32 *piBytesWritten, s32 iArg3, s32 *piAuxOut) override;

    // Code one 128-frame block of iNumChannels interleaved channels from pafFrames into
    // 76 * iNumChannels bytes at pOutput.
    void EncodeBlock(const f32 *pafFrames, u8 *pOutput, s32 iNumChannels);

    // Reorder one channel's four 19-byte sub-blocks (pSubBlocks) into the XAS1 layout at
    // pOutput: the four 4-byte headers, then residual byte i of sub-blocks 0..3 for
    // i = 0..14.
    void CustomInterleaveXAS(const u8 *pSubBlocks, u8 *pOutput);

    s32 miBufferedSamples;                                          // +0x20
    f32 mafBuffer[KI_BLOCK_SAMPLES * KI_MAX_BUFFERED_CHANNELS];     // +0x24
};

} // namespace core
} // namespace audio
} // namespace rw
