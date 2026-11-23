#pragma once

// =====================================================================================
// rw::audio::core::Iir2 -- a 2nd-order (biquad) IIR filter kernel home.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// of Iir2::Filter @0x82B69DD0 is authoritative. No LegacySrc reference source and no DecFIGS
// DWARF exist for this type. Layout (coefficient block, persistent state, block size)
// is grounded directly in the disassembly -- see rw/audio/core/Iir2.cpp.
//
// Filter() is shared by the concrete filter shapes BandPass/HighPass/HighShelf/
// LowPass/LowShelf/PeakingIir2 (each calls it from its Process()).
// =====================================================================================

#include "types.hpp" // f32

namespace rw
{
namespace audio
{
namespace core
{

// The Xbox 360 audio PDB identifies two histories in each 16-byte Iir2.
// ARTIST clears them at 0x82B69F58, and the caller at 0x82B9E36C advances by 16 bytes.
class Iir2
{
public:
    struct FilterCoefficients
    {
        f32 a1;
        f32 a2;
        f32 b0;
        f32 b1;
        f32 b2;
    };

    // Fixed processing-block length: the kernel runs until src reaches src + 0x400
    // bytes, i.e. 256 single-precision samples (asm: r11 = r5 + 0x400; r5 += 0x20/iter).
    enum { KI_BLOCK_SAMPLES = 256 };

    void Filter(f32 *dst, const f32 *src, const FilterCoefficients *coeffs);
    void ClearBuffer();

private:
    struct History
    {
        f32 delay1;
        f32 delay2;
    };

    History mInHistory;
    History mOutHistory;
};

} // namespace core
} // namespace audio
} // namespace rw
