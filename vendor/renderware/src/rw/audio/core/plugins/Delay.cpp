// =====================================================================================
// rw::audio::core::Delay bodies -- the "Delay" feedback delay-line audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/Delay.h for the byte-exact
// layout and the per-function X360 addresses.
//
// The plug-in runs a single feedback DelayLine (mDelayLine) coloured by a DelayFilter
// (mFilter), reconfigured by a profiling TimerHandle (mTimer). It reuses the committed
// audio-core homes DelayLine (W22), DelayFilter (W21), TimerHandle / TimerManager / System
// (W25) -- it does NOT fork them.
//
// All nine functions are now reconstructed. The `vector deleting destructor' @0x82B9DDE8 was
// previously left blocked on an "un-homed STUB" teardown of the embedded TimerHandle: that
// STUB resolves to 0x82AD5078, which is the image's shared empty thunk (a lone `blr` that the
// X360 identical-code-folder collapses every trivial function onto). i.e. ~TimerHandle is a
// TRIVIAL destructor (all TimerHandle members are POD -- pointers, ints, bytes -- so it owns
// nothing to release), and the compiler folded it to that no-op. The destructor is therefore
// faithfully reconstructed: the empty ~TimerHandle call, the real ~DelayLine, the base-vtable
// reinstall, and the flag-gated free -- store-for-store with the asm.
// =====================================================================================

#include "rw/audio/core/plugins/Delay.h"
#include "rw/audio/core/Voice.h"   // the owning Voice (mfFadeStart decay accumulator)
#include "rw/audio/core/PlugIn.h"       // rw::audio::core::System (mTimerManager @+0x60)
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager::AddTimer
#include "rw/audio/core/IFilter.h"      // rw::audio::core::IFilter (filter apply/reset install)

#include <cmath> // log10, std::fabs
#include <new>   // placement new (DelayLine sub-object construction)

namespace rw
{
namespace audio
{
namespace core
{

// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System TU; ReleaseEvent removes the reconfigure timer through it.
extern "C" System *off_83271928;

// off_8217F58C -- the Delay v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the destructor reinstalls before any free. These are opaque data symbols in
// the XEX (no exported contents); modelled as honest placeholder storage so the ctor / dtor
// link without fabricating their contents.
static void *const KDL_DelayVTable      = nullptr; // off_8217F58C
static void *const KDL_BasePlugInVTable = nullptr; // off_820AA810

// The delay targets the fixed 48 kHz internal rate (flt_820AA808); the latency form uses the
// shared 5.0 scale (flt_8200426C, grounded == 5.0 across the image); flt_82001CC0 == 0.0.
static const f32 KF_ZERO          = 0.0f;     // flt_82001CC0
static const f32 KF_INTERNAL_RATE = 48000.0f; // flt_820AA808
static const f32 KF_LATENCY_SCALE = 5.0f;     // flt_8200426C

// The per-frame scratch local buffer Process reserves for the delay line: 704 f32 samples
// (0xB00 bytes) carved off the top of the System object table's scratch cursor (word [3]).
static const s32 KI_SCRATCH_SAMPLES = 704; // li r5, 0x2C0

// -------------------------------------------------------------------------------------
// GetSize @0x82B96A38 -- the plug-in instance footprint (X360 constant).
// -------------------------------------------------------------------------------------
int Delay::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit, phase-D follow-up): the console
    // immediate under-allocates the widened host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof (the RawPuller2/Send/
    // SinePlayer precedent).
    return static_cast<int>(sizeof(Delay));   // X360: li r3, 0xB8 (184)
}

// -------------------------------------------------------------------------------------
// GetPpuTicksEvent @0x82B9DDE0 -- the CPU cycles the last timer callback took (the timer's
// recorded tick count). lwz r3, 0xA8(r3) == mTimer.mCpuTicks (mTimer @+0x98, +0x10).
// -------------------------------------------------------------------------------------
u32 Delay::GetPpuTicksEvent(Delay *self)
{
    return self->mTimer.mCpuTicks;
}

// -------------------------------------------------------------------------------------
// Delay::Delay @0x82B9DD90 -- install the v-table, then default-construct the embedded
// filter / delay-line / timer in the asm's order. (Invoked through PlugIn::Initialize<Delay>;
// the base-field wiring is done by that helper, reproduced in CreateInstance below.)
// -------------------------------------------------------------------------------------
Delay *Delay::Delay_ctor(Delay *self)
{
    self->mBase.mpVTable = KDL_DelayVTable;   // stw off_8217F58C @ +0x00

    DelayFilter::DelayFilter_ctor(&self->mFilter); // addi r3,r31,0x44 -> &mFilter
    new (&self->mDelayLine) DelayLine();           // addi r3,r31,0x5C -> &mDelayLine (real ctor)
    TimerHandle::TimerHandle_ctor(&self->mTimer);  // addi r3,r31,0x98 -> &mTimer

    return self;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82B9DDE8 -- destruct then conditionally free.
//
//   ~TimerHandle(&mTimer); ~DelayLine(&mDelayLine);
//   mBase.mpVTable = off_820AA810;                 // reinstall the base PlugIn v-table
//   if (flags & 1) operator delete(self);
//   return self;
//
// The leading teardown call (r3 = self+0x98 = &mTimer -> "STUB") targets 0x82AD5078, the
// image's shared empty thunk (a lone `blr`): ~TimerHandle is trivial (POD members), folded to
// the no-op, so the call is reproduced but does nothing. ~DelayFilter (mFilter @+0x44) is
// likewise trivial and the compiler elided its call entirely -- so, per the asm, none is made
// here. Only mTimer and mDelayLine get explicit destructor calls, in asm order.
// -------------------------------------------------------------------------------------
void *Delay::VectorDeletingDestructor(Delay *self, char flags)
{
    self->mTimer.~TimerHandle();  // bl STUB (0x82AD5078) -- trivial/no-op ~TimerHandle
    self->mDelayLine.~DelayLine(); // bl ~DelayLine (0x82B6E498)

    self->mBase.mpVTable = KDL_BasePlugInVTable; // stw off_820AA810 @ +0x00

    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2918 -- placement-init a Delay over `self`.
//
// Initialize<Delay>(self, 0x28) constructs the plug-in and bases its attribute table at
// self+0x28 (mfDelayTime). The templated PlugIn::Initialize helper lives in the PlugIn TU;
// its locally-observable effect (construct + attribute base) is reproduced inline here, as
// the Gain / Iir2* / ReverbModel1 shapes do. FLAGGED: the real Initialize<T> body (and the
// base mpSystem/mpInput wiring it performs from the voice/factory) lives in the PlugIn TU.
//
// Then the delay sets its own defaults (optionally overriding the max delay time from the
// `pMaxDelayTime` param), sizes the ring, and registers the per-frame reconfigure timer.
// Returns 0 if the ring alloc or the timer registration fails, else 1.
// -------------------------------------------------------------------------------------
int Delay::CreateInstance(Delay *self, f32 *pMaxDelayTime)
{
    Delay_ctor(self);
    self->mBase.mpAttributes = &self->mfDelayTime; // attribute table base @ self+0x28

    self->mfDelayTime    = KF_ZERO;          // +0x28
    self->mbActive       = 0;                // +0xB0 (byte)
    self->mfFeedback     = KF_ZERO;          // +0x30
    self->mState         = 0;                // +0x38 (word)
    self->mfMaxDelayTime = KF_ZERO;          // +0x40
    self->mfSampleRate   = KF_INTERNAL_RATE; // +0x3C = 48000
    if (pMaxDelayTime)
        self->mfMaxDelayTime = *pMaxDelayTime;

    // Size the ring: input-channel count, max delay in samples, and the filter's read length.
    const s32 channels    = self->mBase.mbFlag20; // lbz r4, 0x20(r31)
    const s32 maxSamples  = static_cast<s32>(self->mfMaxDelayTime * KF_INTERNAL_RATE);
    const s32 readLength  = self->mFilter.miMode; // lwz r6, 0x4C(r31)

    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    if (!self->mDelayLine.Init(channels, maxSamples, readLength)
        || TimerManager::AddTimer(
               &pSystem->mTimerManager, &self->mTimer,
               reinterpret_cast<TimerManager::TimerCallback>(&Delay::RwacTimerClient),
               self, "Delay", 1, 1))
    {
        return 0;
    }

    self->mbActive = 1;
    return 1;
}

// -------------------------------------------------------------------------------------
// Process @0x82BA2A08 -- process one frame.
//
// Install the filter's apply/reset dispatch funcs and hand the filter to the delay line,
// reserve a scratch local buffer off the System object table, then run the crossfade
// machine: on the frame the delay first goes live (state 0) reset the filter and prime the
// line; while running (state 1) re-tune the feedback / re-delay when the requested delay
// still fits the valid samples, else fall to state 2 to wait; state 2 completes the re-delay
// once enough samples are valid. When the delay is live, filter one frame through the line
// and ping-pong the src/dst context buffers. Finally refresh the reported latency and
// restore the scratch cursor. Always returns 1.
// -------------------------------------------------------------------------------------
int Delay::Process(Delay *self, AudioProcessContext *ctx)
{
    DelayFilter *pFilter    = &self->mFilter;
    DelayLine   *pDelayLine = &self->mDelayLine;

    // Install the DelayFilter's IFilter dispatch funcs (its leading slots ARE the IFilter
    // apply/reset function pointers; DelayLine::ApplyFilter dispatches mpApplyFunc through
    // the mpFilter pointer). Set them through the IFilter view DelayLine uses.
    IFilter *pFilterInterface = reinterpret_cast<IFilter *>(pFilter);
    pFilterInterface->mpApplyFunc =
        reinterpret_cast<IFilterApplyFunc>(&DelayFilter::DelayFilterApplyFunc); // stw @ +0x00
    pFilterInterface->mpResetFunc =
        reinterpret_cast<IFilterResetFunc>(&DelayFilter::DelayFilterResetFunc); // stw @ +0x04
    pDelayLine->SetFilter(pFilterInterface);

    // Reserve a 704-sample scratch region off the System object table's scratch cursor
    // (word [3]); the value is restored at the tail. (The object table is the shared runtime
    // word-table System::mpObjectTable points at; slot [3] is its scratch stack top.)
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    f32 **ppScratchTable = reinterpret_cast<f32 **>(pSystem->mpObjectTable);
    f32 *pScratchTop = ppScratchTable[3];              // r26 (saved to restore)
    f32 *pLocalBuffer = pScratchTop - KI_SCRATCH_SAMPLES; // r26 - 0xB00 bytes
    ppScratchTable[3] = pLocalBuffer;
    pDelayLine->SetLocalBuffer(pLocalBuffer, KI_SCRATCH_SAMPLES);

    // The requested delay this frame, in samples (output sample rate * live delay time).
    const s32 requestedDelay =
        static_cast<s32>(ctx->mpFormat->mfSampleRate * self->mfDelayTime); // v13
    const f32 feedback = self->mfFeedback;                                 // v9 (f31)

    switch (self->mState)
    {
    case 0: // idle / priming -- go live once a delay is requested
        if (requestedDelay > 0)
        {
            DelayFilter::DelayFilterResetFunc(pFilter); // (*(v3+4))(v3)
            DelayFilter::SetFeedback(pFilter, feedback);
            pDelayLine->Reset(requestedDelay);
            self->mState = 1;
        }
        break;

    case 1: // running -- re-tune, or drop to state 2 if the new delay is not yet valid
        if (requestedDelay > 0)
        {
            if (pDelayLine->GetValidDelaySamples() >= requestedDelay)
            {
                DelayFilter::SetFeedback(pFilter, feedback);
                pDelayLine->SetDelay(requestedDelay);
                // (no state change; the re-delay took effect immediately)
            }
            else
            {
                self->mState = 2;
            }
        }
        else
        {
            self->mState = 0;
        }
        break;

    case 2: // re-delaying -- complete once enough samples are valid
        if (requestedDelay > 0)
        {
            if (requestedDelay <= pDelayLine->GetValidDelaySamples())
            {
                DelayFilter::SetFeedback(pFilter, feedback);
                pDelayLine->SetDelay(requestedDelay);
                self->mState = 1;
            }
            // else: not enough valid samples yet -- leave state 2, filter below
        }
        else
        {
            self->mState = 0;
        }
        break;

    default: // >= 3 -- inert
        break;
    }

    // When the delay is live, filter one frame through the ring and ping-pong the buffers.
    if (self->mState)
    {
        pDelayLine->ApplyFilter(256, ctx->mpSrcBuffer, ctx->mpDstBuffer, 0);
        AudioChannelBuffer *pSwap = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = pSwap;
    }

    UpdateLatencyAndDecay(self);

    ppScratchTable[3] = pScratchTop; // restore the scratch cursor
    return 1;
}

// -------------------------------------------------------------------------------------
// UpdateLatencyAndDecay @0x82B9DE50 -- recompute the delay's latency (mLatencyInSamples,
// +0x14: the raw delay length the mixer must pre-roll) and its DECAY-TAIL length
// (mDecaySamples, +0x18: the Schroeder RT60 form  d - d*5/log10(|g|) -- how long the
// feedback tail rings), folding the decay CHANGE into the owning voice's decay
// accumulator (voice+0x28 == Voice::mfFadeStart, the expel-after-decay deadline).
// When the delay is not running (state != 1) both collapse to zero.
// (2026-08-25 wave 4: the "+0x28 latency word" naming here was WRONG -- the
// accumulator is the fade/decay deadline System::UpdateExpellingVoices consumes.)
// -------------------------------------------------------------------------------------
void Delay::UpdateLatencyAndDecay(Delay *self)
{
    f32 newDecay;   // v5
    f32 decayDelta; // v7

    if (self->mState == 1)
    {
        const f32 delaySamples = static_cast<f32>(self->mDelayLine.miReadPosition); // v4
        const f32 feedback     = self->mFilter.mfFeedback;                          // v3
        newDecay = delaySamples;                                                    // v5 = v4
        if (feedback != KF_ZERO)
            newDecay = delaySamples - static_cast<f32>(
                (delaySamples * KF_LATENCY_SCALE)
                / static_cast<f32>(log10(std::fabs(feedback))));
        decayDelta = newDecay - self->mBase.mDecaySamples; // v5 - v6 (old +0x18)
        self->mBase.mLatencyInSamples = delaySamples;      // *(this+0x14) = v4
    }
    else
    {
        decayDelta = -self->mBase.mDecaySamples;       // v7 = -*(this+0x18)
        newDecay = KF_ZERO;                            // v5 = 0
        self->mBase.mLatencyInSamples = KF_ZERO;       // *(this+0x14) = 0
    }

    // Fold the decay change into the owning voice's decay accumulator
    // (voice+0x28 == Voice::mfFadeStart, by name), then latch it.
    self->mBase.mpVoice->mfFadeStart += decayDelta;
    self->mBase.mDecaySamples = newDecay; // *(this+0x18) = v5
}

// -------------------------------------------------------------------------------------
// RwacTimerClient @0x82B972C8 -- the per-frame reconfigure timer callback. Clamp the max
// delay time up to at least the live delay time, force the internal rate back to 48 kHz
// (dropping the crossfade state if it had drifted), and grow the ring if the max delay no
// longer fits. (Registered via a function-pointer cast; its return value is discarded by the
// scheduler.)
// -------------------------------------------------------------------------------------
int Delay::RwacTimerClient(Delay *self)
{
    if (self->mfMaxDelayTime < self->mfDelayTime)
        self->mfMaxDelayTime = self->mfDelayTime;

    if (self->mfSampleRate != KF_INTERNAL_RATE)
    {
        self->mfSampleRate = KF_INTERNAL_RATE;
        self->mState = 0;
    }

    const s32 required = static_cast<s32>(self->mfMaxDelayTime * self->mfSampleRate);
    if (self->mDelayLine.miMaxDelaySamples < required)
        return self->mDelayLine.Resize(required);
    return 1;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B97278 -- tear the delay down: release the ring back to the allocator,
// and (if it was registered) remove the reconfigure timer through the shared System
// singleton. (The X360 return is a cleanup register value callers discard.)
// -------------------------------------------------------------------------------------
int Delay::ReleaseEvent(Delay *self)
{
    self->mDelayLine.Release();

    if (self->mbActive)
        System::RemoveTimer(off_83271928, &self->mTimer);

    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
