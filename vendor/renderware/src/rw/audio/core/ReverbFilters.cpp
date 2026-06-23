// =====================================================================================
// rw::audio::core reverb building-block filter bodies (AllPassFilter, CombFilter, and
// the shared AllPassFilterFunc recurrence kernel).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/coefficient. No Feb-2007 leak source and no DecFIGS
// DWARF exist.
//   AllPassFilter::AllPassFilter      @0x82B6C3D8 -- store-for-store
//   AllPassFilter::AllPassFilterApply @0x82B64640 -- store-for-store
//   AllPassFilter::SetGains           @0x82B64688 -- store-for-store
//   AllPassFilterFunc                 @0x82B64560 -- recurrence, store-for-store
//   CombFilter::CombFilter            @0x82B6C6B0 -- store-for-store
//   CombFilter::CombFilterResetFunc   @0x82B64D88 -- store-for-store
//   CombFilter::SetGains              @0x82B64D98 -- store-for-store
// See ReverbFilters.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/ReverbFilters.h"

#include <cstring> // std::memset (the X360 memset the apply func tail-calls)

namespace rw
{
namespace audio
{
namespace core
{

// flt_82001CC0 == 0.0f (the float-zero immediate every ctor stores).
static const f32 KF_ZERO = 0.0f;
// flt_8214B108 == 1e-18f, the engine's denormal-flush bias (round +1e-18 then -1e-18),
// identical to the Iir2 kernel.
static const f32 KF_DENORMAL_FLUSH = 1.0e-18f;

// -------------------------------------------------------------------------------------
// AllPassFilter::AllPassFilter @0x82B6C3D8
// -------------------------------------------------------------------------------------
AllPassFilter *AllPassFilter::AllPassFilter_ctor(AllPassFilter *self)
{
    self->mfGain1 = KF_ZERO;  // stfs flt @ +0x10
    self->miField00 = 0;      // stw r10 (0) @ +0x00
    self->mfGain2 = KF_ZERO;  // stfs flt @ +0x14
    self->miField04 = 0;      // stw r10 (0) @ +0x04
    self->miField0C = 1;      // stw r11 (1) @ +0x0C
    self->miMode = 1;         // stw r11 (1) @ +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// AllPassFilterFunc @0x82B64560 -- the all-pass recurrence the apply func tail-calls.
//
// The kernel walks `work` (r11, == ctx->mpFeedback in the apply path) forward `count`
// samples. Three other streams are addressed by a constant byte delta from `work`:
//   feedback     = base[i + (a6 - a7)]   (the delayed-input read)
//   feedforwardA = work[i + (a8 - a7)]
//   feedforwardB = work[i + (a9 - a7)]
// Per the asm (fnmsubs d,a,b == b - a*d; fmadds d,a,b == a*d + b), with denormal flush:
//
//   x      = flush(work[i])
//   y      = feedback[i] - g1*x          ; fnmsubs f1,f13,f12  (store @ feedforwardA)
//   yf     = flush(y)
//   accum  = (g1*yf + work[i]) * g2       (lowPass==0)          ; store @ feedforwardB
//   accum  = (g1*yf + work[i]) * g2 + feedforwardB[i] (lowPass!=0; the += comb form)
//
// The two branches differ only by whether the final feedforwardB store accumulates
// (fmadds onto the prior value) -- exactly the asm's loc_82B645D4 vs fall-through split.
// -------------------------------------------------------------------------------------
void AllPassFilterFunc(s32 count, f32 g1, f32 g2, f32 *base, f32 *work,
                       f32 *feedforwardA, f32 *feedforwardB, s32 lowPass)
{
    // Byte deltas reduce to element deltas on the f32 streams (the asm keeps them as
    // raw byte subtractions `subf` then `lfsx`/`stfsx`; element form is equivalent).
    const ptrdiff_t dFeedback = base - work;         // a6 - a7
    const ptrdiff_t dForwardA = feedforwardA - work; // a8 - a7
    const ptrdiff_t dForwardB = feedforwardB - work; // a9 - a7

    if (count <= 0)
        return;

    for (s32 n = 0; n < count; ++n)
    {
        f32 x = (work[n] + KF_DENORMAL_FLUSH) - KF_DENORMAL_FLUSH;
        work[n] = x;

        f32 y = work[n + dFeedback] - (g1 * x); // fnmsubs: feedback - g1*x
        work[n + dForwardA] = y;
        f32 yf = (y + KF_DENORMAL_FLUSH) - KF_DENORMAL_FLUSH;
        work[n + dForwardA] = yf;

        f32 fwd = ((yf * g1) + work[n]) * g2; // fmadds g1*yf + work, then fmuls * g2
        if (lowPass)
            fwd += work[n + dForwardB]; // fmadds onto prior value (comb accumulate)
        work[n + dForwardB] = fwd;
    }
}

// -------------------------------------------------------------------------------------
// AllPassFilter::AllPassFilterApplyFunc @0x82B64640
//   if (ctx->miInactive) memset(ctx->mpFeedforward, 0, count*sizeof(f32));
//   else AllPassFilterFunc(count, self->mfGain1, self->mfGain2, ctx->mpBase,
//                          ctx->mpFeedback, ctx->mpAccum, ctx->mpFeedforward,
//                          lowPass = (a3 != 0));
// (r3=self, r4=count, r5=a3->lowPass, r7=ctx; r6=ctx+0, r7'=ctx+4, r8=ctx+0x10,
//  r9=ctx+0x14.)
// -------------------------------------------------------------------------------------
void *AllPassFilter::AllPassFilterApplyFunc(AllPassFilter *self, s32 count, s32 src,
                                            f32 *extra, Context *ctx)
{
    (void)extra;
    if (ctx->miInactive)
        return std::memset(ctx->mpFeedforward, 0, static_cast<usize>(count) * 4);

    AllPassFilterFunc(count, self->mfGain1, self->mfGain2,
                      /*base a6  */ ctx->mpBase,
                      /*work a7  */ ctx->mpFeedback,
                      /*forwA a8 */ ctx->mpAccum,
                      /*forwB a9 */ ctx->mpFeedforward,
                      /*lowPass  */ src != 0 ? 1 : 0);
    return nullptr;
}

// -------------------------------------------------------------------------------------
// AllPassFilter::SetGains @0x82B64688
// -------------------------------------------------------------------------------------
AllPassFilter *AllPassFilter::SetGains(AllPassFilter *self, f32 g1, f32 g2)
{
    self->mfGain1 = g1; // stfs f1 @ +0x10
    self->mfGain2 = g2; // stfs f2 @ +0x14
    return self;
}

// -------------------------------------------------------------------------------------
// CombFilter::CombFilter @0x82B6C6B0
// -------------------------------------------------------------------------------------
CombFilter *CombFilter::CombFilter_ctor(CombFilter *self)
{
    self->mfGain1 = KF_ZERO;  // +0x10
    self->miField00 = 0;      // +0x00
    self->mfGain2 = KF_ZERO;  // +0x14
    self->miField04 = 0;      // +0x04
    self->mfGain3 = KF_ZERO;  // +0x18
    self->miMode = 2;         // +0x08 (li r9,2)
    self->mfGain4 = KF_ZERO;  // +0x1C
    self->mfState = KF_ZERO;  // +0x20
    self->miField0C = 1;      // +0x0C (li r11,1)
    return self;
}

// -------------------------------------------------------------------------------------
// CombFilter::CombFilterResetFunc @0x82B64D88 -- clear the loop damping state.
// -------------------------------------------------------------------------------------
CombFilter *CombFilter::CombFilterResetFunc(CombFilter *self)
{
    self->mfState = KF_ZERO; // stfs flt @ +0x20
    return self;
}

// -------------------------------------------------------------------------------------
// CombFilter::SetGains @0x82B64D98
// -------------------------------------------------------------------------------------
CombFilter *CombFilter::SetGains(CombFilter *self, f32 g1, f32 g2, f32 g3, f32 g4)
{
    self->mfGain1 = g1; // +0x10
    self->mfGain2 = g2; // +0x14
    self->mfGain3 = g3; // +0x18
    self->mfGain4 = g4; // +0x1C
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
