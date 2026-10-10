#ifndef SDKS_EATECH_SND_CEAXABLKDECF_H
#define SDKS_EATECH_SND_CEAXABLKDECF_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CEAXABLKDecf.h
//
// EATech "Snd" EA-XA block decoder support. This header homes the per-channel XA
// block-decode cursor state plus the "raw block" sub-path.
//
//   - Snd::process_raw_block   (defined in CEAXABLKDecf.cpp)
//   - Snd::decodexac           (defined in CEAXABLKDecf.cpp)
//
// The state shape is attested by process_raw_block and by the top-level
// Snd::decodexac:
//   +0x00  miCount   samples still to decode (decodexac's outer loop guard)
//   +0x04  mfHist0   ADPCM history: the most recent output sample
//   +0x08  mfHist1   ADPCM history: the second most recent output sample
//   +0x0C  mpSource  input byte cursor
//   +0x10  mpDest    output float cursor
// On the PC target the two pointers widen to 8 bytes; access is by named member
// so the layout stays parity-correct (semantic parity, not byte offsets).
//
// decodexac's predictor-coefficient and nibble-dequantiser tables are dumped
// from the image into CEAXABLKDecf.cpp.
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

    // Decode one uncompressed ("raw") XA block: verify the 0xEE block
    // marker (return unchanged if absent), seed the two history slots from the
    // next two big-endian s16 samples, then decode 28 further big-endian s16
    // samples verbatim into the output buffer. Returns apState (r3 pass-through).
    XaBlockDecoder* process_raw_block(XaBlockDecoder* apState);

    // Decode EA-XA blocks until miCount is exhausted. Each block is either a raw
    // block (0xEE marker, see process_raw_block) or a 15-byte ADPCM block: one
    // header byte (high nibble = predictor pair, low nibble = shift row) and 14
    // bytes of two 4-bit residuals each, giving 28 samples predicted from the two
    // previous outputs. The history is seeded into mpDest[-2], mpDest[-1] on
    // entry and written back from the last two outputs of each ADPCM block.
    // Returns apState.
    XaBlockDecoder* decodexac(XaBlockDecoder* apState);
} // namespace Snd

#endif // SDKS_EATECH_SND_CEAXABLKDECF_H
