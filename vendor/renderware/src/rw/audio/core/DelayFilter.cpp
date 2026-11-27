// =====================================================================================
// rw::audio::core::DelayFilter bodies -- the feedback delay-line filter of the
// rw::audio::core::Delay plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No LegacySrc reference source and no DecFIGS DWARF exist.
//   DelayFilter::DelayFilter          @0x82B67D10 -- store-for-store
//   DelayFilter::DelayFilterApplyFunc @0x82B67DD0
//   DelayFilter::DelayFilterResetFunc @0x82B67E88
//   DelayFilter::SetFeedback          @0x82B67EA0
// See DelayFilter.h for the byte-exact layout.
// =====================================================================================

#if defined(_XBOX) && defined(_MSC_VER)
// Size optimization reproduces the ARTIST apply and feedback bodies.
#pragma optimize("s", on)
#endif

#include "rw/audio/core/DelayFilter.h"

#include <cmath> // std::fabs (the fabs the feedback clamp uses)

namespace rw
{
namespace audio
{
namespace core
{

// flt_82001CC0 == 0.0f (the float-zero immediate the ctor / reset store, and the
// clamp's sign test comparand).

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilter @0x82B67D10 -- clear the record, mark the read-length slot.
// -------------------------------------------------------------------------------------
DelayFilter *DelayFilter::DelayFilter_ctor(DelayFilter *self)
{
    self->mfFeedback = 0.0f;     // stfs flt @ +0x10
    self->mpApplyFunc = 0;          // stw r11 (0) @ +0x00
    self->mfPrevFeedback = 0.0f; // stfs flt @ +0x14
    self->mpResetFunc = 0;          // stw r11 (0) @ +0x04
    self->miChannels = 0;           // stw r11 (0) @ +0x0C
    self->miReadLength = 1;         // stw r10 (1) @ +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilterApplyFunc @0x82B67DD0
//
// When a feedback change is in flight (ctx->pDelayCross != 0) it hands off to the
// crossfade recurrence. Otherwise it runs the delay loop over `count` samples: the
// delayed input (mpBase) mixed with the fed-back "work" stream (mpFeedback) scaled by the
// current feedback gain lands in the accumulate buffer, and the raw work sample is copied
// forward into the delay-write buffer.
// ARTIST uses r3 for the filter, r4 for the count and r7 for the locations.
// -------------------------------------------------------------------------------------
void DelayFilter::DelayFilterApplyFunc(IFilter *filter, s32 count, FilterOutputMode,
                                       s32, LocationParams *ctx)
{
    DelayFilter *self = static_cast<DelayFilter *>(filter);
    if (!ctx->pDelayCross)
    {
        f32 *output = ctx->pOutput;
        const f32 feedback = self->mfFeedback;
        f32 *delay = ctx->pDelayZero;
        const f32 *full = ctx->pDelayFull;
        const f32 *input = ctx->pInput;
        for (s32 n = 0; n < count; ++n)
        {
            delay[n] = full[n] * feedback + input[n];
            output[n] = full[n];
        }
    }
    else
    {
        DelayFilterCrossFadeFunc(count, self->mfFeedback, self->mfPrevFeedback,
                                 ctx->pInput, ctx->pDelayFull, ctx->pDelayCross,
                                 ctx->pCrossFadeCoeff, ctx->pDelayZero, ctx->pOutput);
    }
}

// ARTIST 82B67D40: each output is computed separately, with source streams
// re-read after the accumulation store (the buffers may alias).
void DelayFilterCrossFadeFunc(s32 count, f32 feedback, f32 prevFeedback,
                             f32 *base, f32 *primary, f32 *secondary,
                             f32 *weight, f32 *accum, f32 *feedforward)
{
    if (count <= 0)
        return;

    do
    {
        const f32 blend = *weight;
        const f32 inverse = 1.0f - blend;
        const f32 oldTerm = *secondary * blend;
        const f32 newTerm = inverse * *primary;
        const f32 weightedOld = oldTerm * prevFeedback;
        const f32 combined = newTerm * feedback + weightedOld;
        *accum = combined + *base;

        const f32 blendAgain = *weight;
        const f32 inverseAgain = 1.0f - blendAgain;
        const f32 oldAgain = *secondary * blendAgain;
        *feedforward = inverseAgain * *primary + oldAgain;
        ++weight;
        ++secondary;
        ++primary;
        ++base;
        ++accum;
        ++feedforward;
    } while (--count);
}

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilterResetFunc @0x82B67E88 -- clear both feedback gain snapshots.
// -------------------------------------------------------------------------------------
void DelayFilter::DelayFilterResetFunc(IFilter *filter)
{
    DelayFilter *self = static_cast<DelayFilter *>(filter);
    self->mfFeedback = 0.0f;     // stfs flt @ +0x10
    self->mfPrevFeedback = 0.0f; // stfs flt @ +0x14
}

// -------------------------------------------------------------------------------------
// DelayFilter::SetFeedback @0x82B67EA0 -- clamp the requested feedback to +/-0.99 (a
// magnitude >= 1.0 makes the feedback loop unstable), latch the outgoing gain into the
// "previous" slot so the apply func can cross-fade, then install the new gain.
// -------------------------------------------------------------------------------------
void DelayFilter::SetFeedback(f32 feedback)
{
    // ARTIST loads the upper bound from 0x8214AF4C and the lower from 0x820CC338.
    if (std::fabs(feedback) > 0.99f)
    {
        if (feedback > 0.0f)
            feedback = 0.99f;
        else
            feedback = -0.99f;
    }

    mfPrevFeedback = mfFeedback;
    mfFeedback = feedback;
}

} // namespace core
} // namespace audio
} // namespace rw

namespace rw { namespace audio { namespace core {
} } }
