// =====================================================================================
// rw::audio::core::Iir2::Filter -- 2nd-order (biquad) IIR filter kernel.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B69DD0; the
// PowerPC asm is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist.
//
// Form (direct-form-I, per the fmadds/fnmsubs trace -- fnmsubs a,b,c == c - a*b):
//   y[n] = c2*x[n] + c3*x[n-1] + c4*x[n-2] - c0*y[n-1] - c1*y[n-2] + 1e-18
// where the coefficient block is { c0, c1, c2, c3, c4 } and the persistent state is
// { x[-1], x[-2], y[-1], y[-2] }. The +1e-18 term is the engine's denormal-flush bias
// (flt_8214B108). The X360 kernel processes a fixed 256-sample block (r5 < r5+0x400),
// unrolled 8 samples per iteration; the rolling-state form below is store-for-store
// equivalent.
//
// IDA renders this as a nullary method because the _savefpr_24/_restfpr_24 prologue
// hides the register arguments; the call-site/asm register usage is authoritative:
//   r3 = filter state (4 floats)   r4 = dst   r5 = src   r6 = coeffs (5 floats)
// =====================================================================================

#include "rw/audio/core/Iir2.h"

namespace rw
{
namespace audio
{
namespace core
{

void Iir2::Filter(Iir2State *state, f32 *dst, const f32 *src, const Iir2Coeffs *coeffs)
{
    const f32 KF_DENORMAL_FLUSH = 1.0e-18f; // flt_8214B108

    const f32 c0 = coeffs->mfA1; // feedback  y[n-1]
    const f32 c1 = coeffs->mfA2; // feedback  y[n-2]
    const f32 c2 = coeffs->mfB0; // feedforward x[n]
    const f32 c3 = coeffs->mfB1; // feedforward x[n-1]
    const f32 c4 = coeffs->mfB2; // feedforward x[n-2]

    f32 x1 = state->mfX1; // x[-1]
    f32 x2 = state->mfX2; // x[-2]
    f32 y1 = state->mfY1; // y[-1]
    f32 y2 = state->mfY2; // y[-2]

    for (int i = 0; i < KI_BLOCK_SAMPLES; ++i)
    {
        f32 x0 = src[i];
        f32 ff = (c2 * x0) + (c3 * x1) + (c4 * x2) + KF_DENORMAL_FLUSH;
        f32 y0 = ff - (c0 * y1) - (c1 * y2);
        dst[i] = y0;

        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
    }

    state->mfX1 = x1;
    state->mfX2 = x2;
    state->mfY1 = y1;
    state->mfY2 = y2;
}

// -------------------------------------------------------------------------------------
// rw::audio::core::Iir2::ClearBuffer
// Zero one channel's biquad history. No standalone X360 export exists (the call name
// rw::audio::core::Iir2::ClearBuffer is folded into its call sites); it clears the
// 4-float Iir2State the Filter() difference equation threads.
// -------------------------------------------------------------------------------------
void Iir2::ClearBuffer(Iir2State *state)
{
    state->mfX1 = 0.0f;
    state->mfX2 = 0.0f;
    state->mfY1 = 0.0f;
    state->mfY2 = 0.0f;
}

} // namespace core
} // namespace audio
} // namespace rw
