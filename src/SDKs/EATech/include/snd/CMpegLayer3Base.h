#ifndef SDKS_EATECH_SND_CMPEGLAYER3BASE_H
#define SDKS_EATECH_SND_CMPEGLAYER3BASE_H

#include "types.hpp"
#include "SDKs/EATech/include/snd/CMpegBase.h"

// ============================================================================
// SDKs/EATech/include/snd/CMpegLayer3Base.h
//
// EATech "Snd" MPEG Layer III decode stages. CMpegLayer3Base sits between the
// frame/bitstream base Snd::CMpegBase and the concrete Snd::CEALayer3 decoder:
// it owns the per-granule side information and scale factors that CEALayer3's
// bitstream readers fill in, and the spectral stages CEALayer3::DecodeMono /
// DecodeStereo run on each granule after the Huffman decode:
//
//   Dequantize -> Stereo (stereo streams) -> Reorder -> AntiAlias -> Hybrid
//
// Bodies and the dumped rodata live in CMpegLayer3Base.cpp.
//
// VTABLE. The class's own vtable has the same six slots as CMpegBase (dumped from
// the image): only the scalar deleting destructor is replaced; ProcessHeader,
// Open, Close, Seek and OpenLayer are inherited. CEALayer3 appends its Decode slot.
//
// LAYOUT NOTE (console byte offsets, from the asm of the six methods). The PC
// target widens the base's pointers and mpOverlap, so the absolute offsets differ;
// every access is by named member.
//   +0x00 .. +0x53  Snd::CMpegBase
//   +0x54           four bytes no Layer III stage touches (opaque)
//   +0x58           maGranule[2][2]       side information, [channel][granule], 0x18 each
//   +0xB8           maScaleFactors[2]     scale factors, [channel], 0x7C each
//   +0x1B0          mpOverlap             IMDCT overlap buffer (576 f32 per channel)
// ============================================================================

namespace Snd
{

class CMpegLayer3Base : public CMpegBase
{
public:
    // Layer III granule side information (0x18 bytes). Only the fields the six
    // stages read are named; the rest is opaque to them.
    struct GranuleInfo
    {
        u8  mPad00[4];              // +0x00  (opaque)
        u16 muScalefacCompress;     // +0x04  bit 0 = LSF intensity-stereo table select
        u8  mPad06;                 // +0x06  (opaque)
        u8  mucWindowSwitching;     // +0x07  window_switching_flag
        u8  mucBlockType;           // +0x08  block_type (2 = short blocks)
        u8  mucMixedBlock;          // +0x09  mixed_block_flag
        u8  mPad0A[6];              // +0x0A  (opaque)
        u8  maucSubblockGain[3];    // +0x10  subblock_gain per short window
        u8  mucPreflag;             // +0x13  preflag (adds the pre-emphasis table)
        u32 muScalefacScale;        // +0x14  scalefac_scale (0 or 1), used as a shift
    };

    // Scale factors of one channel (0x7C bytes): 23 long-block factors followed by
    // three windows of 13 short-block factors.
    struct ScaleFactors
    {
        s16 masLong[23];            // +0x00
        s16 masShort[3][13];        // +0x2E
    };

    // Free mpOverlap through the Snd free hook when it was allocated; the base
    // destructor then closes an open stream.
    virtual ~CMpegLayer3Base();

    // Apply the scale-factor gains (2^(-0.5 * (1 + scalefac_scale) * sf), with the
    // pre-emphasis and sub-block gains) band by band to the dequantised spectrum.
    void Dequantize(s32 aiChannel, s32 aiGranule, f32* apSamples);

    // Joint-stereo processing of both channels of granule aiGranule in place
    // (apSamples = left[576] followed by right[576]): intensity stereo (MPEG-1
    // tangent ratios or the LSF k-values) and/or mid-side.
    void Stereo(s32 aiGranule, f32* apSamples);

    // Reorder short-block lines from window-major to the frequency-interleaved
    // order the IMDCT reads (only for short and mixed blocks).
    void Reorder(s32 aiChannel, s32 aiGranule, const f32* apIn, f32* apOut);

    // Alias-reduction butterflies across the sub-band boundaries.
    void AntiAlias(s32 aiChannel, s32 aiGranule, f32* apSamples);

    // IMDCT + windowing + overlap-add of the granule, four sub-bands at a time
    // (the spectrum is interleaved in groups of four sub-bands).
    void Hybrid(s32 aiChannel, s32 aiGranule, f32* apSamples);

    // LSF intensity-stereo scale pair for line aiIndex from intensity position
    // aiIsPos and table aiIoType, written to apK[0][aiIndex] / apK[1][aiIndex].
    void i_stereo_k_values(s32 aiIsPos, s32 aiIoType, s32 aiIndex, f32 (*apK)[576]);

    u8           mPad54[4];             // +0x54  (opaque)
    GranuleInfo  maGranule[2][2];       // +0x58  [channel][granule]
    ScaleFactors maScaleFactors[2];     // +0xB8  [channel]
    f32*         mpOverlap;             // +0x1B0 IMDCT overlap, 576 f32 per channel
};

} // namespace Snd

#endif // SDKS_EATECH_SND_CMPEGLAYER3BASE_H
