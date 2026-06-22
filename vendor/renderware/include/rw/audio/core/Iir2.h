#pragma once

// =====================================================================================
// rw::audio::core::Iir2 -- a 2nd-order (biquad) IIR filter kernel home.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// of Iir2::Filter @0x82B69DD0 is authoritative. No Feb-2007 leak source and no DecFIGS
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

// Persistent filter state, direct-form-I history (4 floats, read at +0/+4/+8/+0xC):
//   mfX1, mfX2 = input  history x[-1], x[-2]
//   mfY1, mfY2 = output history y[-1], y[-2]
struct Iir2State
{
    f32 mfX1; // +0x00
    f32 mfX2; // +0x04
    f32 mfY1; // +0x08
    f32 mfY2; // +0x0C
};

// Coefficient block (5 floats, read from r6 at +0/+4/+8/+0xC/+0x10). Feedback
// coefficients first (a1,a2), then feedforward (b0,b1,b2) -- matching the asm's
// fnmsubs (feedback) / fmadds (feedforward) split.
struct Iir2Coeffs
{
    f32 mfA1; // +0x00 -- y[n-1] feedback
    f32 mfA2; // +0x04 -- y[n-2] feedback
    f32 mfB0; // +0x08 -- x[n]   feedforward
    f32 mfB1; // +0x0C -- x[n-1] feedforward
    f32 mfB2; // +0x10 -- x[n-2] feedforward
};

class Iir2
{
public:
    // Fixed processing-block length: the kernel runs until src reaches src + 0x400
    // bytes, i.e. 256 single-precision samples (asm: r11 = r5 + 0x400; r5 += 0x20/iter).
    enum { KI_BLOCK_SAMPLES = 256 };

    // Filter one 256-sample block from `src` into `dst`, threading and updating the
    // direct-form-I history in `state`. (Rendered nullary by IDA because the
    // _savefpr/_restfpr prologue hides the register args; the asm register usage --
    // r3=state, r4=dst, r5=src, r6=coeffs -- is authoritative.)
    static void Filter(Iir2State *state, f32 *dst, const f32 *src, const Iir2Coeffs *coeffs);
};

} // namespace core
} // namespace audio
} // namespace rw
