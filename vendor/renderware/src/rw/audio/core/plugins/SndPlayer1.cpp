// =====================================================================================
// rw::audio::core::SndPlayer1 -- the engine's sample player. CONSTRUCTION HALF.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// store and branch. Full body decode, adversarially verified:
// progress/scratch_dossiers/sndplayer1_bodies_decode.md.
//   GetSize               @0x82BA0220 -- the computed layout
//   CreateInstance        @0x82BA6C80 -- 27 init stores, in the asm's order
//   PreProcess            @0x82B9C2D8
//   WaitForStartTime      @0x82B9C148
//   Declick               @0x82B9C1C0
//   AdvanceCurrentRequest @0x82B9C2E8
//   GetFeedSlot           @0x82BA0380
//   GetPpuTicksEvent      @0x82BDD2D0 (vt[2])
//
// ⚠️ DELIBERATELY NOT REGISTERED YET. The streaming half -- Process, the event surface and
// the whole decoder/stream chain -- needs ONE type this tree has not homed:
// rw::audio::core::StreamPool. Those bodies are honest FLAG'd deferrals below and the
// descriptor stays unregistered until every slot it would publish is real, which is the rule
// the RWAC registration site has enforced since the descriptor wave.
//
// ⭐ CORRECTION 2026-08-29: an earlier revision of this banner also named
// rw::core::filesys::Stream as un-homed. THAT WAS WRONG. It is homed, at
// b5-decomp/src/SDKs/EATech/rwcore/filesys/stream.h, and it carries QueueFile / GetChunk /
// ReleaseChunk / GetRequestState / GetState plus Chunk (muSize, mpData) and the
// ChunkParseCallback typedef -- the exact shapes every SndPlayer1 call site uses. The
// streaming half must reach for those BY NAME and invent nothing.
// See plugins/SndPlayer1.h for the layout and the per-field hazards.
// =====================================================================================

#include "rw/audio/core/plugins/SndPlayer1.h"
#include "rw/audio/core/Mixer.h"        // Mixer (the process context) + SampleBuffer
#include "rw/audio/core/TimerManager.h" // TimerManager::AddTimer

#include <cstring> // std::memset

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
    // Rodata, each recomputed at file_off = 0x3000 + vaddr - 0x82000000 and re-read
    // big-endian out of the decrypted XEX:
    const f32 KF_ZERO = 0.0f;             // flt_82001CC0
    const f64 KD_ZERO = 0.0;              // dbl_82001CA8
    const f32 KF_INTERNAL_RATE = 48000.0f;// flt_820AA808
    const f32 KF_MIX_FRAME = 256.0f;      // flt_820ADBFC

    // The allocation tag the console passes to System::Alloc (rodata @0x82174598).
    const char KSZ_ALLOC_NAME[] = "SndPlayer1 RequestHandle and RequestExternal array";
    // The timer's name (rodata @0x821745CC).
    const char KSZ_TIMER_NAME[] = "SndPlayer";

    inline u32 AlignUp(u32 auValue, u32 auAlign)
    {
        return (auValue + (auAlign - 1u)) & ~(auAlign - 1u);
    }
}

// -------------------------------------------------------------------------------------
// TruncateRequestCount -- the console's `fctidz` + `stfiwx` pair.
//
// fctidz converts to a 64-bit integer rounding TOWARD ZERO and SATURATES (NaN gives
// 0x8000000000000000, a huge positive gives 0x7FFFFFFFFFFFFFFF); stfiwx then stores only
// the LOW 32 BITS. So NaN yields 0 and a huge positive yields 0xFFFFFFFF -- neither of
// which an x64 `(int)f` produces (it gives INT_MIN for both).
//
// GetSize and CreateInstance both convert the SAME constructor parameter and must agree
// exactly, or CreateInstance's request ring overruns the buffer GetSize asked for; sharing
// this helper is what guarantees that.
// -------------------------------------------------------------------------------------
u32 SndPlayer1::TruncateRequestCount(f32 afValue)
{
    // Saturating 64-bit truncate-toward-zero, then keep the low word.
    s64 lSaturated;
    if (afValue != afValue)                                   // NaN
        lSaturated = static_cast<s64>(0x8000000000000000LL);
    else if (afValue >= 9223372036854775808.0f)               // >= 2^63
        lSaturated = 0x7FFFFFFFFFFFFFFFLL;
    else if (afValue <= -9223372036854775808.0f)
        lSaturated = static_cast<s64>(0x8000000000000000LL);
    else
        lSaturated = static_cast<s64>(afValue);
    return static_cast<u32>(static_cast<u64>(lSaturated) & 0xFFFFFFFFu);
}

// -------------------------------------------------------------------------------------
// ComputeLayout -- where the two variable-length tails live, computed from HOST sizes.
//
// Console: declick at 0x1D8, requests at align_up(0x1D8 + 4*channels, 8), total plus
// 0x30 per request. 0x1D8 is align_up(console sizeof, 8) and spans about 49 pointer slots;
// 0x30 is the console sizeof(RequestInternal) and holds a Decoder*. Neither survives.
// -------------------------------------------------------------------------------------
SndPlayer1Layout SndPlayer1::ComputeLayout(u8 au8Channels, u32 auMaxRequests)
{
    SndPlayer1Layout lLayout;

    // The console's `addi this,0x1DF ; clrrwi 3` is align_up(fixed-header extent, 8); the
    // host's own trailing padding is already inside sizeof, and the align_up is kept
    // because it documents the console's 8 and costs nothing.
    const u32 luDeclick = AlignUp(static_cast<u32>(sizeof(SndPlayer1)), 8u);

    // The console's 8 here IS alignof(RequestInternal) -- the record leads with an f64 --
    // so the host uses alignof(RequestInternal) for exactly the same reason.
    const u32 luRequest = AlignUp(luDeclick + static_cast<u32>(sizeof(f32)) * au8Channels,
                                  static_cast<u32>(alignof(RequestInternal)));

    lLayout.muDeclickBufferOffset   = static_cast<u16>(luDeclick);
    lLayout.muRequestInternalOffset = static_cast<u16>(luRequest);
    lLayout.muTotalSize =
        luRequest + static_cast<u32>(sizeof(RequestInternal)) * auMaxRequests;
    return lLayout;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82BA0220 -- the stage-carve stride.
//
// ⚠️ The channel count comes from the config's mFlagAndField8. The console reads it as
// `lbz r9, 8(r3)`, but +8 on x64 lands INSIDE the widened mpDesc -- that mis-read is
// exactly what caused the phase-D mixer-scribble crash, so this goes by name.
// -------------------------------------------------------------------------------------
int SndPlayer1::GetSize(const VoiceStageConfig *apConfig)
{
    const ConstructorParams *lpParams =
        static_cast<const ConstructorParams *>(apConfig->mpContext);
    const u32 luMaxRequests = lpParams ? TruncateRequestCount(lpParams->maxRequests) : 1u;
    const u8 lu8Channels = static_cast<u8>(apConfig->mFlagAndField8);
    return static_cast<int>(ComputeLayout(lu8Channels, luMaxRequests).muTotalSize);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA6C80 -- 27 init stores in the assembly's exact order.
//
// The generic PlugIn::CreateInstance has already run the base init and passes the config's
// mpContext as the constructor params; only the LOW BYTE of the return is tested, and on
// failure the generic caller runs vt[0] then vt[3] and returns null -- which is why nothing
// here unwinds its own allocation.
// -------------------------------------------------------------------------------------
int SndPlayer1::CreateInstance(SndPlayer1 *self, const ConstructorParams *apParams)
{
    // [phase 1] the request-ring depth, defaulting to one.
    const u32 luMaxRequests = apParams ? TruncateRequestCount(apParams->maxRequests) : 1u;

    // [phase 2] PlugIn::Initialize<SndPlayer1>(self, 0x28) -- installs the vtable (the host
    // compiler's construction), constructs the TimerHandle, and bases the attribute table.
    self->mpAttribute = self->mAttribute;

    // [phase 3] compute and publish the layout. mbTimerAdded is cleared FIRST, before any
    // fallible work, so the teardown path can trust it.
    const SndPlayer1Layout lLayout =
        ComputeLayout(self->mOutputChannels, luMaxRequests);
    self->mbTimerAdded = 0;                                       // [1]
    self->mDeclickBufferOffset   = lLayout.muDeclickBufferOffset; // [2]
    self->mRequestInternalOffset = lLayout.muRequestInternalOffset; // [3]

    // [phase 4] the ONE private allocation: a leading f32 handle counter followed by the
    // RequestExternal array.
    //
    // ⚠️ The console asks for `0x50*n + 4` and puts the array at base+4. Both halves break
    // on the host: 0x50 is a console sizeof holding seven pointers, and the +4 head would
    // leave RequestExternal's leading f64 at 4-mod-16. The host therefore reserves an
    // ALIGNED head for the counter and starts the array on its natural alignment. The
    // observable state is identical -- a counter plus n records -- and the pointer stored
    // at mpRequestHandle is still the block base, which the destructor frees.
    const size_t luHead = AlignUp(static_cast<u32>(sizeof(f32)),
                                  static_cast<u32>(alignof(RequestExternal)));
    const size_t luBytes = luHead + sizeof(RequestExternal) * luMaxRequests;
    void *lpBlock = System::Alloc(self->mpSystemUseGetSystemAccessor,
                                  static_cast<u32>(luBytes), KSZ_ALLOC_NAME, 16, 0);
    self->mpRequestHandle = static_cast<f32 *>(lpBlock);           // [4] the BASE
    if (!lpBlock)
        return 0;   // the generic caller's vt[0]/vt[3] path does the teardown

    self->mMaxRequests = static_cast<u8>(luMaxRequests);           // [5] ⚠️ LOW BYTE ONLY
    self->mpRequestExternal = reinterpret_cast<RequestExternal *>(
        static_cast<char *>(lpBlock) + luHead);                    // [6]

    // [phase 5] only the state byte of each request, and only for i < n. Nothing else in
    // RequestInternal is initialised -- not startTime, not pDecoder.
    for (u32 luRequest = 0; luRequest < luMaxRequests; ++luRequest)
        self->GetRequestInternal(luRequest)->state = REQUESTSTATE_FREE;    // [7]

    // [phase 6] the flat seeding, in the assembly's store order.
    self->mMaxChannels = self->mOutputChannels;                              // [8]
    self->mAttribute[ATTRIBUTE_GETCURRENTREQUEST].mfValue = KF_ZERO;         // [9]
    self->SetSampleLengthAttribute(KD_ZERO);                                 // [10] f64
    self->SetSamplePositionAttribute(KD_ZERO);                               // [11] f64
    *self->mpRequestHandle = KF_ZERO;                                        // [12]
    self->mCurrentRequest = 0;                                               // [13]
    self->mLastRequestHandleProcessed = KF_ZERO;                             // [14]
    self->mNextRequestToFree = 0;                                            // [15]
    self->mNextFreeRequest = 0;                                              // [16]
    self->mLastRequestHandleSuccessfullyProcessed = KF_ZERO;                 // [17]
    self->mCurrentRequestHandle = KF_ZERO;                                   // [18]
    self->mCurrentRequestSamplesPlayed = 0;                                  // [19]
    self->mCurrentRequestSampleRate = KF_INTERNAL_RATE;                      // [20]
    self->mCurrentRequestNumSamples = 0;                                     // [21]
    self->mPreviousSampleRate = KF_INTERNAL_RATE;                            // [22]
    self->mNumDeclickSamples = 0;                                            // [23]
    self->mDcOffsetsGathered = 0;                                            // [24]
    self->mNextFeedSlotToFill = 0;                                           // [25]
    self->mNextFeedSlotToFree = 0;                                           // [26]
    self->mNextFeedSlotToCleanup = 0;                                        // [27]

    // [phase 7] the feed ring: only these two fields, for all 20 slots. (The console's
    // 32-bit clear of pChunkInfo is a by-name null here -- on the host that member is
    // 8 bytes and a 4-byte clear would leave half a pointer.)
    for (u32 luFeed = 0; luFeed < KU_MAX_DECODERFEEDS; ++luFeed)
    {
        self->mFeedDesc[luFeed].feedState = FEEDSTATE_FREE;
        self->mFeedDesc[luFeed].pChunkInfo = 0;
    }

    // [phase 8] register the per-frame timer. AddTimer returns 0 on SUCCESS (only its low
    // byte is tested). A failure returns 0 WITHOUT freeing -- the teardown path frees.
    if (TimerManager::AddTimer(&self->mpSystemUseGetSystemAccessor->mTimerManager,
                               &self->mTimerClient, &SndPlayer1::RwacTimerClient,
                               self, KSZ_TIMER_NAME, 1, 1) != 0)
        return 0;

    self->mbTimerAdded = 1;
    return 1;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9C2D8 -- four instructions. Stow the requested count and cascade 0 to
// the next lower stage, of which there is none: this is the source stage. The context and
// the discontinuity flag are decode-attested UNUSED.
// -------------------------------------------------------------------------------------
int SndPlayer1::PreProcess(SndPlayer1 *self, AudioProcessContext * /*ctx*/,
                           bool /*discontinuity*/, int aiRequestedCount)
{
    // A halfword store; the caller clamps its cascade to 256, so it is lossless there.
    self->mSamplesRequested = static_cast<u16>(aiRequestedCount);
    return 0;
}

// -------------------------------------------------------------------------------------
// WaitForStartTime @0x82B9C148 -- how much silence, if any, precedes a scheduled start.
//
// ⚠️ NaN POLARITY IS LOAD-BEARING and both tests are written in the form that preserves it.
// A NaN start time must DECLINE (return false), not decode. Restructuring either compare
// into a negated form would flip that.
//
// `this` is decode-attested unused; it stays a member because that is the console's shape.
// -------------------------------------------------------------------------------------
bool SndPlayer1::WaitForStartTime(AudioProcessContext *ctx, f64 adStartTime,
                                  u32 *apuSamples)
{
    const f64 ldDelta = adStartTime - ctx->mdStreamTime;

    if (ldDelta <= 0.0)          // ordered; false for NaN, which falls through as it must
    {
        *apuSamples = 0;
        return true;             // the start has been reached -- decode this frame
    }

    // The product is rounded to SINGLE before the comparison; keeping it double on the host
    // would compare a slightly different value at the boundary.
    const f32 lfFrames = static_cast<f32>(ctx->mpFormat->mfSampleRate * ldDelta);

    if (!(lfFrames < KF_MIX_FRAME))
        return false;            // a whole frame or more away (or NaN): decline, and leave
                                 // *apuSamples deliberately UNWRITTEN

    // Multiply at single precision, truncate toward zero into 64 bits, keep the low word.
    *apuSamples = static_cast<u32>(
        static_cast<s64>(ctx->mfResampleGain * lfFrames));
    return true;
}

// -------------------------------------------------------------------------------------
// Declick @0x82B9C1C0 -- ramp every channel's last emitted sample down to silence, a slice
// per mix frame. Armed by Process when a request stopped mid-waveform. Always returns
// BUFFERSTATUS_AVAILABLE.
//
// ⚠️ The remaining count is reduced by the count just PUBLISHED, not decremented by one.
// And the ramp step is taken over the FULL remaining count, never over this frame's slice
// -- that is what keeps the ramp one straight line across the several frames it spans.
// -------------------------------------------------------------------------------------
int SndPlayer1::Declick(AudioProcessContext *ctx)
{
    SampleBuffer *lpDst = ctx->mpDstBuffer;      // read BEFORE the swap
    f32 *lpDeclick = GetDeclickBuffer();

    const u32 luRemaining = mNumDeclickSamples;

    // An UNSIGNED min of the remaining ramp against this frame's request.
    u32 luCount = mSamplesRequested;
    if (luRemaining < luCount)
        luCount = luRemaining;

    // The channel bound here is the PlugIn base's mOutputChannels, NOT mMaxChannels.
    if (mOutputChannels != 0)
    {
        const f32 lfStep = 1.0f / static_cast<f32>(static_cast<s32>(luRemaining));

        for (u32 luChannel = 0; luChannel < mOutputChannels; ++luChannel)
        {
            f32 lfValue = lpDeclick[luChannel];
            const f32 lfDelta = lfValue * lfStep;   // computed ONCE, before the inner loop

            if (luCount != 0)   // a zero slice skips the ramp AND the store-back
            {
                f32 *lpChannel = lpDst->mpSamples + lpDst->muStride * luChannel;
                for (u32 luSample = 0; luSample < luCount; ++luSample)
                {
                    lfValue -= lfDelta;
                    lpChannel[luSample] = lfValue;
                }
                lpDeclick[luChannel] = lfValue;     // the resume point for the next frame
            }
        }
    }

    mNumDeclickSamples = static_cast<u8>(luRemaining - luCount);   // SUBTRACT, not decrement
    if (mNumDeclickSamples == 0)
        mDcOffsetsGathered = 0;   // disarm Process's declick dispatch

    SampleBuffer *lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    ctx->mNumSamples = luCount;
    ctx->mfSampleRate = mPreviousSampleRate;
    ctx->mbChannelCount = mOutputChannels;
    return 1;   // BUFFERSTATUS_AVAILABLE
}

// -------------------------------------------------------------------------------------
// AdvanceCurrentRequest @0x82B9C2E8 -- step the request cursor and refresh the cached
// "current request" fields.
//
// The two COUNT caches are cleared unconditionally; the handle and sample rate are
// refreshed ONLY for an active slot and are deliberately left STALE otherwise.
// -------------------------------------------------------------------------------------
void SndPlayer1::AdvanceCurrentRequest()
{
    // The increment is truncated to a byte BEFORE the wrap test, and the test is `==`,
    // never `>=`.
    mCurrentRequest = static_cast<u8>(mCurrentRequest + 1);
    if (mCurrentRequest == mMaxRequests)
        mCurrentRequest = 0;

    RequestInternal *lpRequest = GetRequestInternal(mCurrentRequest);

    mCurrentRequestSamplesPlayed = 0;
    mCurrentRequestNumSamples = 0;

    if (!IsRequestActive(lpRequest->state))
        return;   // the handle and sample-rate caches stay STALE

    mCurrentRequestSamplesPlayed = 0;   // the binary stores it a second time
    mCurrentRequestHandle     = lpRequest->requestHandle;
    mCurrentRequestSampleRate = lpRequest->sampleRate;
    mCurrentRequestNumSamples = lpRequest->numSamples;
}

// -------------------------------------------------------------------------------------
// GetFeedSlot @0x82BA0380 -- claim the next feed slot, if it is free.
// -------------------------------------------------------------------------------------
bool SndPlayer1::GetFeedSlot(u32 *apuOutSlot)
{
    const u32 luSlot = mNextFeedSlotToFill;
    if (mFeedDesc[luSlot].feedState != FEEDSTATE_FREE)
        return false;   // still busy -- the caller must back off

    *apuOutSlot = luSlot;

    u32 luNext = (mNextFeedSlotToFill + 1u) & 0xFFu;
    if (luNext == KU_MAX_DECODERFEEDS)
        luNext = 0;
    mNextFeedSlotToFill = static_cast<u8>(luNext);
    return true;
}

// -------------------------------------------------------------------------------------
// GetPpuTicksEvent @0x82BDD2D0 (vt[2]) -- `lwz r3, 0x50(r3) ; blr`: the timer client's
// accumulated CPU ticks.
// -------------------------------------------------------------------------------------
int SndPlayer1::VFunc2()
{
    return static_cast<int>(mTimerClient.mCpuTicks);
}

// =====================================================================================
// FLAG (DEFER) -- the streaming half.
//
// Process @0x82BA0568, EventEvent @0x82BA5C48 (vt[1]) and ReleaseEvent @0x82BA4178 (vt[0])
// are FULLY DECODED in progress/scratch_dossiers/sndplayer1_bodies_decode.md, but every one
// of them reaches rw::audio::core::StreamPool, which this tree has not homed -- Process
// through the loop/end handlers, EventEvent through PlayHandler's stream open, ReleaseEvent
// through StreamLostCallback. Writing them against an invented pool API would be exactly the
// fabrication the project forbids, so they are honest deferrals until that ONE type lands,
// and the descriptor is NOT registered meanwhile.
//
// ⭐ CORRECTION 2026-08-29: rw::core::filesys::Stream, which an earlier revision of this
// note listed alongside StreamPool, IS homed (src/SDKs/EATech/rwcore/filesys/stream.h). Only
// the pool is missing. What is attested about it from PlayHandler + AcquireStream @0x82B6BAB0:
// GetInstance(u32 guid), AcquireStream(pool, f32 priority /*f1*/, StreamLostFn, void* context)
// returning a 32-byte-stride ENTRY { +0x08 f32 priority, +0x0C the lost-callback,
// +0x14 rw::core::filesys::Stream*, +0x18 u16 refcount, +0x1A u8 inUse }, and a ReleaseStream.
// ⚠️ That 0x20 entry stride holds a pointer and a callback, so it does NOT survive x64 --
// whoever homes the pool must index a typed array, never that literal.
//
// These are UNREACHABLE as committed: nothing constructs a SndPlayer1, because registration
// 21 is still commented out at the RWAC site. The destructor's omitted teardown therefore
// leaks nothing today -- but it WOULD if the type were registered before this note is
// retired, which is precisely why it is not.
// =====================================================================================
SndPlayer1::~SndPlayer1()
{
    // ReleaseEvent @0x82BA4178: StreamLostCallback(this), then System::RemoveTimer when
    // mbTimerAdded == 1, then System::Free(mpRequestHandle). Deferred with the stream half.
}

int SndPlayer1::Event(int /*aiEventId*/, void * /*apParam*/)
{
    // EventEvent @0x82BA5C48: six events (PLAY / STOP / ISREQUESTDONE / GETREQUESTBUFFERED
    // / MODIFYSTARTTIME / PLAY1). Deferred with the stream half.
    return 0;
}

void SndPlayer1::Destroy(int /*aFlags*/)
{
    // vt[3] `vector deleting destructor' @0x82B9EAF8 -- the compiler generates the host
    // equivalent from ~SndPlayer1; this slot exists so the base's dispatch shape holds.
}

int SndPlayer1::Process(SndPlayer1 * /*self*/, AudioProcessContext * /*ctx*/,
                        bool /*discontinuity*/)
{
    // Process @0x82BA0568: decoded in full, deferred with the stream half. Returning
    // BUFFERSTATUS_UNAVAILABLE is the console's own "nothing to produce" answer, so an
    // unreachable call would at least not publish a fabricated frame.
    return 0;
}

void SndPlayer1::RwacTimerClient(void * /*apContext*/, f32 /*afTimeToNextCall*/)
{
    // @0x82BA6980: the per-frame pump -- request cleanup, feed cleanup, StartRequest, and
    // the streaming read machine. Deferred with the stream half.
}

char **SndPlayer1::GetPlugInDescRunTime()
{
    // @0x82B9BE60 -> off_82F901C4 'SnP1'. The real host record lands with the streaming
    // half; returning null here would be registered as a poison descriptor, so this getter
    // is deliberately absent from the registration site rather than returning a placeholder.
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
