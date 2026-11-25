// =====================================================================================
// rw::audio::core reverb building-block filter bodies (AllPassFilter, CombFilter, and
// the shared AllPassFilterFunc recurrence kernel).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/coefficient. No LegacySrc reference source and no DecFIGS
// DWARF exist.
//   AllPassFilter::AllPassFilter      @0x82B6C3D8 -- store-for-store
//   AllPassFilter::AllPassFilterApply @0x82B64640 -- store-for-store
//   AllPassFilter::AllPassFilterReset -- ICF-folded empty thunk 0x82AD5078 (see below)
//   AllPassFilter::SetGains           @0x82B64688 -- store-for-store
//   AllPassFilterFunc                 @0x82B64560 -- recurrence, store-for-store
//   CombFilterFunc                    @0x82B64C20 -- recurrence, store-for-store
//   CombFilter::CombFilter            @0x82B6C6B0 -- store-for-store
//   CombFilter::CombFilterApplyFunc   @0x82B64D00 -- store-for-store (disasm recovered
//                                       from the XEX image via idat; the address had no
//                                       export JSON -- dump in the workflow repo,
//                                       scratchpad/waveE/ReverbModel1.rodata.txt)
//   CombFilter::CombFilterResetFunc   @0x82B64D88 -- store-for-store
//   CombFilter::SetGains              @0x82B64D98 -- store-for-store
// See ReverbFilters.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/ReverbFilters.h"

#include <cmath>
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
    self->mpApplyFunc = 0;      // stw r10 (0) @ +0x00
    self->mfGain2 = KF_ZERO;  // stfs flt @ +0x14
    self->mpResetFunc = 0;      // stw r10 (0) @ +0x04
    self->miChannels = 1;      // stw r11 (1) @ +0x0C
    self->miReadLength = 1;         // stw r11 (1) @ +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// AllPassFilterFunc @0x82B64560 -- the all-pass recurrence the apply func tail-calls.
//
// The kernel walks `work` (r11, == ctx->mpTap in the apply path) forward `count`
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
                       f32 *feedforwardA, f32 *feedforwardB, IFilter::FilterOutputMode mode)
{
    for (s32 n = 0; n < count; ++n)
    {
        // ARTIST 82B64560: preserve both bias round trips and each fused site.
        const f32 biased = work[n] + KF_DENORMAL_FLUSH;
        work[n] = biased - KF_DENORMAL_FLUSH;
        const f32 y = base[n] - g1 * work[n];
        feedforwardA[n] = y;
        const f32 yBiased = y + KF_DENORMAL_FLUSH;
        feedforwardA[n] = yBiased - KF_DENORMAL_FLUSH;
        const f32 tap = feedforwardA[n] * g1 + work[n];
        feedforwardB[n] = mode ? tap * g2 + feedforwardB[n] : tap * g2;
    }
}

// -------------------------------------------------------------------------------------
// AllPassFilter::AllPassFilterApplyFunc @0x82B64640
// A second tap selects the crossfade fallback, which clears the output.
// -------------------------------------------------------------------------------------
void AllPassFilter::AllPassFilterApplyFunc(IFilter *filter, s32 count, FilterOutputMode mode,
                                         s32, LocationParams *ctx)
{
    if (!ctx->pDelayCross)
    {
        AllPassFilter *self = static_cast<AllPassFilter *>(filter);
        AllPassFilterFunc(count, self->mfGain1, self->mfGain2,
                          ctx->pInput, ctx->pDelayFull, ctx->pDelayZero, ctx->pOutput, mode);
    }
    else
    {
        std::memset(ctx->pOutput, 0, static_cast<usize>(count) * sizeof(f32));
    }
}

// -------------------------------------------------------------------------------------
// AllPassFilter::AllPassFilterResetFunc -- the all-pass reset the IFilter reset slot
// carries. The X360 image installs the shared empty thunk 0x82AD5078 there
// (ReverbModel1::Process @0x82B9F884..94): the original empty reset (the all-pass
// section keeps no loop state to clear -- unlike the comb's mfState) was ICF-folded
// onto that lone-`blr` thunk, exactly as the trivial ~TimerHandle teardown was. The
// reset callback has no work to do.
// -------------------------------------------------------------------------------------
void AllPassFilter::AllPassFilterResetFunc(IFilter *)
{
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
CombFilter::CombFilter()
    : mfGain1(KF_ZERO), mfGain2(KF_ZERO), mfGain3(KF_ZERO), mfGain4(KF_ZERO), mfState(KF_ZERO)
{
    miChannels = 1;
    miReadLength = 2;
}

// -------------------------------------------------------------------------------------
// CombFilterFunc @0x82B64C20 -- the feedback-comb recurrence the comb apply func calls.
//
// The kernel walks the four f32 streams index-parallel over `count` samples. In the asm
// each stream is a constant byte delta off one walking pointer (subf then lfsx/stfsx);
// element-index form is equivalent. `work` is read at [n] and [n+1] (the asm advances the
// work pointer one slot before the loop and reads 0(r10) / -4(r10)). Per sample (fnmsubs
// d,a,c,b == b - a*c; fmadds d,a,c,b == a*c + b):
//
//   out            = base[n] - state*g1 - work[n+1]*g2   ; the comb loop write
//   accum[n]       = out
//   feedforward[n] = (work[n]*g3 + work[n+1]) * g4        ; lowPass==0 (overwrite form)
//   feedforward[n] += (work[n]*g3 + work[n+1]) * g4       ; lowPass!=0 (the comb bank's
//                                                           sum-into-mix accumulate form)
//   state          = accum[n]                              ; the asm reloads the stored word
//
// The two branches differ only by whether the feedforward store accumulates -- exactly
// the asm's loc_82B64C94 vs fall-through split. Returns the final state (fmr f1, f0);
// the caller latches it into CombFilter::mfState.
// -------------------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
// The original rwAudio common object uses the smaller instruction form here.
#pragma optimize("s", on)
#endif

f32 CombFilterFunc(s32 count, f32 g1, f32 g2, f32 g3, f32 g4, f32 previousState,
                   f32 *base, f32 *work, f32 *accum, f32 *feedforward, IFilter::FilterOutputMode mode)
{
    f32 state = previousState;
    if (mode == 0)
    {
        for (s32 n = 0; n < count; ++n)
        {
            const f32 tap = work[n + 1];
            const f32 out = (base[n] - state * g1) - tap * g2; // fnmsubs pair
            accum[n] = out;
            feedforward[n] = (work[n] * g3 + work[n + 1]) * g4; // fmuls (overwrite)
            state = accum[n];                                             // lfs reload
        }
    }
    else
    {
        for (s32 n = 0; n < count; ++n)
        {
            const f32 out = (base[n] - state * g1) - work[n + 1] * g2; // fnmsubs pair
            accum[n] = out;
            feedforward[n] = (work[n] * g3 + work[n + 1]) * g4 + feedforward[n]; // fmadds onto prior
            state = accum[n];                                             // lfsx reload
        }
    }
    return state;
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

// -------------------------------------------------------------------------------------
// CombFilter::CombFilterApplyFunc @0x82B64D00
// Save the comb's damping state after applying a block. The crossfade
// fallback clears the output without advancing that state.
// -------------------------------------------------------------------------------------
void CombFilter::CombFilterApplyFunc(IFilter *filter, s32 count, FilterOutputMode mode,
                                    s32, LocationParams *ctx)
{
    if (ctx->pDelayCross)
    {
        std::memset(ctx->pOutput, 0, static_cast<usize>(count) * sizeof(f32));
        return;
    }

    CombFilter *self = static_cast<CombFilter *>(filter);
    self->mfState = CombFilterFunc(count, self->mfGain1, self->mfGain2,
                                  self->mfGain3, self->mfGain4, self->mfState,
                                  ctx->pInput, ctx->pDelayFull, ctx->pDelayZero, ctx->pOutput, mode);
}

// -------------------------------------------------------------------------------------
// CombFilter::CombFilterResetFunc @0x82B64D88 -- clear the loop damping state.
// -------------------------------------------------------------------------------------
void CombFilter::CombFilterResetFunc(IFilter *filter)
{
    CombFilter *self = static_cast<CombFilter *>(filter);
    self->mfState = KF_ZERO; // stfs flt @ +0x20
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
