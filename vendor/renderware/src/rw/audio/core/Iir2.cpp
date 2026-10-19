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
// unrolled 8 samples per iteration. Preserve its feedforward order and single-rounding
// fused sites: algebraically equivalent sums diverge once fed back into the history.
//
// IDA renders this as a nullary method because the _savefpr_24/_restfpr_24 prologue
// hides the register arguments; the call-site/asm register usage is authoritative:
//   r3 = filter state (4 floats)   r4 = dst   r5 = src   r6 = coeffs (5 floats)
// =====================================================================================

#include "rw/audio/core/Iir2.h"

#include <cmath>

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

    for (int i = 0; i < KI_BLOCK_SAMPLES; i += 8)
    {
        // The console reads the whole group before any output store, including
        // when src and dst are the same buffer (82B69E18..82B69E44).
        f32 laInput[8];
        for (int j = 0; j < 8; ++j)
            laInput[j] = src[i + j];

        f32 laFeedForward[8];
        const f32 lfFirst = laInput[0] * c2;
        laFeedForward[0] = std::fmaf(x1, c3, std::fmaf(x2, c4, lfFirst));
        const f32 lfSecond = laInput[1] * c2;
        laFeedForward[1] = std::fmaf(x1, c4, std::fmaf(laInput[0], c3, lfSecond));
        // Slots 2..7 begin with b1*x[n-1], then fuse b2*x[n-2],
        // then b0*x[n]. Slots 0 and 1 above have a different ordering.
        for (int j = 2; j < 8; ++j)
        {
            const f32 lfMiddle = laInput[j - 1] * c3;
            laFeedForward[j] = std::fmaf(laInput[j], c2,
                std::fmaf(laInput[j - 2], c4, lfMiddle));
        }

        for (int j = 0; j < 8; ++j)
        {
            const f32 lfBiased = laFeedForward[j] + KF_DENORMAL_FLUSH;
            // fnmsubs is the negated fused multiply-subtract, with a single
            // rounding before negation (82B69E98/EA4 and each following pair).
            const f32 lfFirstFeedback = -std::fmaf(y1, c0, -lfBiased);
            const f32 y0 = -std::fmaf(y2, c1, -lfFirstFeedback);
            dst[i + j] = y0;

            y2 = y1;
            y1 = y0;
        }

        x2 = laInput[6];
        x1 = laInput[7];
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
