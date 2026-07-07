#ifndef SDKS_EATECH_SND_EALAYER3REORDER_H
#define SDKS_EATECH_SND_EALAYER3REORDER_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/EALayer3Reorder.h
//
// EATech "Snd" EALayer3 codec spectral-buffer helpers. These are the pure DSP
// coefficient shuffles the EALayer3 decoder runs between the Huffman/dequant
// stage and the polyphase synthesis filter. Each operates in place / element by
// element on the granule's float sample buffer -- no codec state, no external
// codebook tables -- so the parameters are modelled as plain f32* signal
// buffers (raw indexed access is faithful here; these are signal arrays, not
// C++ objects).
//
//   - Snd::FrequencyInversionX4    @ 0x82B79468
//   - Snd::ReorderForFPolySynth    @ 0x82B8A9B0
//   - Snd::ReorderForVectoring     @ 0x82B89A28
//
// Callers: Snd::CEALayer3::DecodeMono / DecodeStereo (and the shared
// rw::audio::core::Layer3Dec / EALayer3Core decode paths). Cited by X360 address
// only -- no leaked-source provenance and no DWARF for this source path.
// ============================================================================

namespace Snd
{
    // 0x82B79468. Frequency inversion for the polyphase synthesis: over 8 blocks
    // of 72 floats (0x120 bytes), negate two of every group of eight coefficients
    // (indices 5 and 7, stepping by 8) so alternate sub-bands are phase-inverted.
    // Returns apData (the X360 leaves the buffer pointer in r3).
    f32* FrequencyInversionX4(f32* apData);

    // 0x82B8A9B0. Transpose the EALayer3 granule coefficients into the layout the
    // floating-point polyphase synthesizer expects: for each of 18 rows and 9
    // sub-band groups, copy a run of 4 coefficients from the strided source into
    // the contiguous destination. apSrc is read, apDst is written; returns apSrc.
    f32* ReorderForFPolySynth(f32* apSrc, f32* apDst);

    // 0x82B89A28. Companion reorder that gathers the source coefficients into the
    // "vectoring" layout: for each of 18 rows and 9 groups, scatter a run of 4
    // coefficients (source stride 18 floats) into the contiguous destination.
    // apSrc is read, apDst is written; returns apSrc.
    f32* ReorderForVectoring(f32* apSrc, f32* apDst);
} // namespace Snd

#endif // SDKS_EATECH_SND_EALAYER3REORDER_H
