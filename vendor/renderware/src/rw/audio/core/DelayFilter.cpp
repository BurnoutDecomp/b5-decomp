// =====================================================================================
// rw::audio::core::DelayFilter bodies -- the feedback delay-line filter of the
// rw::audio::core::Delay plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No Feb-2007 leak source and no DecFIGS DWARF exist.
//   DelayFilter::DelayFilter          @0x82B67D10 -- store-for-store
//   DelayFilter::DelayFilterApplyFunc @0x82B67DD0
//   DelayFilter::DelayFilterResetFunc @0x82B67E88
//   DelayFilter::SetFeedback          @0x82B67EA0
// See DelayFilter.h for the byte-exact layout.
// =====================================================================================

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
static const f32 KF_ZERO = 0.0f;
// flt_8214AF4C == 0.99000001f -- the feedback magnitude clamp (a runaway feedback loop
// >= 1.0 would be unstable); flt_820CC338 == -0.99000001f is its negated form.
static const f32 KF_FEEDBACK_CLAMP = 0.99000001f;

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilter @0x82B67D10 -- clear the record, mark the read-length slot.
// -------------------------------------------------------------------------------------
DelayFilter *DelayFilter::DelayFilter_ctor(DelayFilter *self)
{
    self->mfFeedback = KF_ZERO;     // stfs flt @ +0x10
    self->miField00 = 0;            // stw r11 (0) @ +0x00
    self->mfPrevFeedback = KF_ZERO; // stfs flt @ +0x14
    self->miField04 = 0;            // stw r11 (0) @ +0x04
    self->miField0C = 0;            // stw r11 (0) @ +0x0C
    self->miMode = 1;               // stw r10 (1) @ +0x08
    return self;
}

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilterApplyFunc @0x82B67DD0
//
// When a feedback change is in flight (ctx->miInactive != 0) it hands off to the
// crossfade recurrence. Otherwise it runs the delay loop over `count` samples: the
// delayed input (mpBase) mixed with the fed-back "work" stream (mpFeedback) scaled by the
// current feedback gain lands in the accumulate buffer, and the raw work sample is copied
// forward into the delay-write buffer.
//   ctx->mpAccum[n]       = ctx->mpFeedback[n] * self->mfFeedback + ctx->mpBase[n];
//   ctx->mpFeedforward[n] = ctx->mpFeedback[n];
// (r3=self, r4=count, r7=ctx; src/extra are dead register params the body never reads.)
// -------------------------------------------------------------------------------------
void *DelayFilter::DelayFilterApplyFunc(DelayFilter *self, s32 count, s32 src,
                                        f32 *extra, Context *ctx)
{
    (void)src;
    (void)extra;

    if (ctx->miInactive)
        return DelayFilterCrossFadeFunc(count, self->mfFeedback, self->mfPrevFeedback,
                                        ctx->mpBase, ctx->mpFeedback, ctx->miInactive,
                                        ctx->miField0C, ctx->mpAccum, ctx->mpFeedforward);

    const f32 feedback = self->mfFeedback;
    for (s32 n = 0; n < count; ++n)
    {
        f32 work = ctx->mpFeedback[n];
        ctx->mpAccum[n] = (work * feedback) + ctx->mpBase[n]; // fmadds
        ctx->mpFeedforward[n] = work;                         // re-read of the same sample
    }
    return self;
}

// -------------------------------------------------------------------------------------
// DelayFilter::DelayFilterResetFunc @0x82B67E88 -- clear both feedback gain snapshots.
// -------------------------------------------------------------------------------------
DelayFilter *DelayFilter::DelayFilterResetFunc(DelayFilter *self)
{
    self->mfFeedback = KF_ZERO;     // stfs flt @ +0x10
    self->mfPrevFeedback = KF_ZERO; // stfs flt @ +0x14
    return self;
}

// -------------------------------------------------------------------------------------
// DelayFilter::SetFeedback @0x82B67EA0 -- clamp the requested feedback to +/-0.99 (a
// magnitude >= 1.0 makes the feedback loop unstable), latch the outgoing gain into the
// "previous" slot so the apply func can cross-fade, then install the new gain.
// -------------------------------------------------------------------------------------
DelayFilter *DelayFilter::SetFeedback(DelayFilter *self, f32 feedback)
{
    if (std::fabs(feedback) > KF_FEEDBACK_CLAMP)
        feedback = (feedback <= KF_ZERO) ? -KF_FEEDBACK_CLAMP : KF_FEEDBACK_CLAMP;

    self->mfPrevFeedback = self->mfFeedback; // stfs (old +0x10) -> +0x14
    self->mfFeedback = feedback;             // stfs (clamped)  -> +0x10
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
