#pragma once

#include "SDKs/XAudio/XAudioSrcCommon.h"

// ===========================================================================
// XAUDIO::SRC::Double -- 2x (sample-rate-doubling) sample-rate converter.
//
// `Double` is a stateless helper class exposing a single static function
// template `Process<T, CHANNELS>`. Each instantiation upsamples one block of
// interleaved input samples (format `T`) by 2x with a per-block linear
// volume/pitch glide and writes the result into a de-interleaved (planar)
// float output buffer. The X360 build emits 18 explicit instantiations
// (format x channel-count); all are reconstructed from BURNOUT_X360_ARTIST.XEX
// PowerPC (the Hex-Rays pseudocode is authoritative -- no reference source or
// DWARF exists for this TU). The kernel is SCALAR (no VMX); the DoubleVector
// sibling TU wraps these scalar kernels for its remainders.
//
//   T  = format tag: `float` (native), `short` (big-endian s16), `SHORTLE`
//        (little-endian s16), `unsigned char` (u8, 128-biased).
//   CHANNELS = interleave count; 0 means "read the runtime channel count from
//        XAUDIOSRCHDR::muChannels", N>0 means a fixed N-channel block.
// ===========================================================================

// The shared SRC types -- SHORTLE (empty little-endian s16 tag),
// SrcHistorySample (4-byte punned per-channel history slot), and XAUDIOSRCHDR
// (the source/destination descriptor passed by DoubleVector / the mixer) --
// live in their single owning home, SDKs/XAudio/XAudioSrcCommon.h.

namespace XAUDIO
{
namespace SRC
{

class Double
{
public:
    // Upsample one block by 2x. `T` selects the input sample format; `CHANNELS`
    // selects a fixed interleave count (0 = runtime muChannels). Returns nothing
    // (the X360 kernel leaves the descriptor pointer in r3; it is a void SRC).
    template <typename T, int CHANNELS>
    static void Process(XAUDIOSRCHDR* pHdr);
};

// The XAUDIOSRCHDR / SrcHistorySample layout self-check (_AssertLayout) lives in
// the shared home, SDKs/XAudio/XAudioSrcCommon.h.

} // namespace SRC
} // namespace XAUDIO
