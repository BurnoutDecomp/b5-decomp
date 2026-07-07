#ifndef SDKS_EATECH_SND_CEAXABLKDECF_H
#define SDKS_EATECH_SND_CEAXABLKDECF_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CEAXABLKDecf.h
//
// EATech "Snd" EA-XA block decoder support. This header homes the per-channel XA
// block-decode cursor state plus the "raw block" sub-path.
//
//   - Snd::process_raw_block   @ 0x82B80758   (defined in CEAXABLKDecf.cpp)
//
// The state shape is attested by process_raw_block and by the top-level
// Snd::decodexac (0x82B80828):
//   +0x00  miCount   samples/blocks remaining (decodexac's outer loop guard)
//   +0x04  mfHist0   ADPCM history / seed sample (written from the last output)
//   +0x08  mfHist1   ADPCM history / seed sample (written from the 2nd-last output)
//   +0x0C  mpSource  input byte cursor
//   +0x10  mpDest    output float cursor
// On the PC target the two pointers widen to 8 bytes; access is by named member
// so the layout stays parity-correct (semantic parity, not byte offsets).
//
// FLAG (BLOCKED, un-recovered rodata): the top-level Snd::decodexac (0x82B80828)
// is NOT reconstructed here -- its ADPCM path reads coefficient / dequantiser
// tables that are not present in the dossier (unk_82F88400 dequant table,
// unk_82F883E0 filter-coefficient table). It is left blocked rather than
// fabricated. process_raw_block below has no such dependency (verbatim PCM).
// ============================================================================

namespace Snd
{
    // EA-XA per-channel block-decode cursor state.
    struct XaBlockDecoder
    {
        s32       miCount;   // +0x00  samples/blocks still to decode
        f32       mfHist0;   // +0x04  ADPCM history / seed sample
        f32       mfHist1;   // +0x08  ADPCM history / seed sample
        const u8* mpSource;  // +0x0C  input byte cursor
        f32*      mpDest;    // +0x10  output float cursor
    };

    // 0x82B80758. Decode one uncompressed ("raw") XA block: verify the 0xEE block
    // marker (return unchanged if absent), seed the two history slots from the
    // next two big-endian s16 samples, then decode 28 further big-endian s16
    // samples verbatim into the output buffer. Returns apState (r3 pass-through).
    XaBlockDecoder* process_raw_block(XaBlockDecoder* apState);
} // namespace Snd

#endif // SDKS_EATECH_SND_CEAXABLKDECF_H
