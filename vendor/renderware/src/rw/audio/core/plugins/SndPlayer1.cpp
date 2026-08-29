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
#include "rw/audio/core/Mixer.h"        // Mixer (the process context) + SampleBuffer + StackAllocator
#include "rw/audio/core/TimerManager.h" // TimerManager::AddTimer
#include "rw/audio/core/Decoder.h"      // Decoder::Decode / GetSamplesRemaining + DecoderBuffer

#include <cstddef> // offsetof (the SampleBuffer/DecoderBuffer aliasing pins)
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
// FLAG (DEFER) -- what is LEFT of the streaming half.  ⭐ SHRUNK 2026-08-29.
//
// Process @0x82BA0568 IS NOW BODIED (below). It was deferred on the belief that it reached
// StreamPool; it does not. Its whole external surface -- Decoder, Mixer, the System's
// StackAllocator, rw::core::filesys::Stream -- is homed.
//
// Still deferred: EventEvent @0x82BA5C48 (vt[1]), ReleaseEvent @0x82BA4178 (vt[0]) and
// RwacTimerClient @0x82BA6980. These DO reach rw::audio::core::StreamPool -- EventEvent
// through PlayHandler's stream open, ReleaseEvent through StreamLostCallback, the timer
// through the pool entry's priority republish -- and that one type is not homed yet.
//
// ⭐ WHAT THE POOL DECODE FOUND, and it reframes this whole deferral
// (progress/scratch_dossiers/streampool_decode_codex.md, verified three independent ways):
// StreamPool::GetInstance @0x82B6BA68 reads its registry list head from the .bss global at
// 0x83271C7C, and NO INSTRUCTION IN THE ENTIRE IMAGE EVER WRITES THAT GLOBAL. So it returns
// null for every guid on the real console too. AcquireStream @0x82B6BAB0 then dereferences
// its `this` with no null guard (`lbz r11,0x28(r30)` @0x82B6BAD4), and PlayHandler passes the
// pool straight in, testing only the RESULT (@0x82BA431C).
//
// Therefore SndPlayer1's stream-open path is COMPILED-IN DEAD CODE: reaching it would fault
// on retail hardware. The guard is RequestExternal::playType (@0x82BA42E4) -- 0 (resident)
// skips the pool entirely, while 1 (streamed) and 2 (hybrid) walk into it. Retail works, so
// retail only ever gives a 'SnP1' voice resident requests; real streaming goes through the
// game's own fork 'JStr' (SndPlayer1_CgsStreamMod) and the module's IStreamProvider, which
// an independent decode confirmed never touches this pool.
//
// ⚠️ CONSEQUENCE FOR WHOEVER FINISHES THIS: the pool home must be FAITHFUL, not defensive.
// An empty registry whose lookup fails is EXACT -- it fails for the same reason the console's
// does. Do NOT add a null-pool guard the console lacks; that would hide a genuine content
// divergence behind silently different behaviour.
//
// These remain UNREACHABLE as committed: nothing constructs a SndPlayer1, because registration
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

// -------------------------------------------------------------------------------------
// Decoder::Decode takes a DecoderBuffer*; the console hands it the Mixer's SampleBuffer
// directly, because on the X360 the two records alias over the three fields Decode
// touches: the sample pointer at +0x04, the cursor at +0x0C and the stride at +0x0E.
//
// ⭐ THAT ALIASING SURVIVES x64, but only by arithmetic that is easy to break. SampleBuffer
// leads with a System* (8 bytes) where DecoderBuffer leads with a u32 that the ABI then pads
// to 8, so BOTH land their sample pointer at +8, their 16-bit cursor at +20 and their stride
// at +22. The pins below exist so that a future field edit to either record fails the build
// instead of silently handing the decoder a misaligned view of the mix buffer.
// -------------------------------------------------------------------------------------
static_assert(offsetof(SampleBuffer, mpSamples) == offsetof(DecoderBuffer, mpData),
              "SampleBuffer/DecoderBuffer: the sample pointer must alias (console +0x04)");
static_assert(offsetof(SampleBuffer, muStride) == offsetof(DecoderBuffer, muStride),
              "SampleBuffer/DecoderBuffer: the stride must alias (console +0x0E)");
static_assert(offsetof(SampleBuffer, muUnk0C) == offsetof(DecoderBuffer, muSampleCursor),
              "SampleBuffer/DecoderBuffer: the sample cursor must alias (console +0x0C)");

static DecoderBuffer *AsDecoderBuffer(SampleBuffer *apBuffer)
{
    return reinterpret_cast<DecoderBuffer *>(apBuffer);
}

// -------------------------------------------------------------------------------------
// SndPlayer1::Process @0x82BA0568 -- 428 instructions.
//
// ⭐ NO LONGER DEFERRED (2026-08-29). This body reaches the Decoder, the Mixer, the System's
// StackAllocator and rw::core::filesys::Stream -- every one of them homed. It does NOT touch
// StreamPool; only the event surface does. See the streaming-half note above.
//
// Faithfulness notes that a reader will otherwise "fix" into being wrong:
//   * The third parameter really is unused: r5 is overwritten with 0 at 0x82BA05C4 before any
//     read.
//   * The format handshake compares with `!=`, which is what the console's `bne` after `fcmpu`
//     gives -- a NaN sample rate is UNORDERED, takes the not-equal path, and re-handshakes.
//   * The StackAllocator dereference is NOT hoisted to entry. The console reads
//     System::mpObjectTable only at the carve and again at each restore/re-carve; hoisting it
//     would add reads on the invalid, no-feed and future-start paths.
//   * There is no null check before `mpLoadedDecoder = request->pDecoder`, and none is added.
//   * When `produced == 0` and nothing was skipped and samples were requested, the function
//     returns 0 leaving ctx->mNumSamples STALE. That is the console's own behaviour and the
//     caller's contract; zeroing it here would be a silent divergence.
//   * The feed-retire loop below can retire every consecutive FED record when the loaded
//     decoder is null. Reproduced, not guarded.
// -------------------------------------------------------------------------------------
int SndPlayer1::Process(SndPlayer1 *self, AudioProcessContext *ctx, bool /*abUnused*/)
{
    // === 1. DECLICK DISPATCH (0x82BA057C..0x82BA0598) -- BOTH bytes must be non-zero ===
    if (self->mNumDeclickSamples != 0 && self->mDcOffsetsGathered != 0)
        return self->Declick(ctx);

    // === 2. ENTRY STATE (0x82BA059C..0x82BA05C4) ===
    self->mpLoadedDecoder = 0;                                   // EVERY entry
    RequestInternal *lpRequest = self->GetRequestInternal(self->mCurrentRequest);
    s32 liSkipped = 0;
    s32 liProduced = 0;
    s32 liRemainingInFeed = 0;
    u8 *lpCarveNew = 0;    // doubles as the console's "a carve is outstanding" flag (r21)
    u8 *lpCarveSaved = 0;  // the saved scratch top (r22)

    if (IsRequestActive(lpRequest->state))
    {
        // === 3. ZERO-LENGTH RETIRE SCAN (0x82BA05C8..0x82BA0660) ===
        // Zero-length requests are RETIRED, not played. Termination is guaranteed: the ring
        // is finite and every retired slot becomes COMPLETE, which the validity test rejects.
        while (lpRequest->numSamples == 0)
        {
            lpRequest->state = REQUESTSTATE_COMPLETE;
            self->AdvanceCurrentRequest();
            lpRequest = self->GetRequestInternal(self->mCurrentRequest);  // recomputed
            if (!IsRequestActive(lpRequest->state))
                goto epilogue;
        }

        // === 4. FORMAT HANDSHAKE (0x82BA0664..0x82BA0680 -> 0x82BA0BD0) ===
        if (lpRequest->sampleRate != self->mPreviousSampleRate ||
            lpRequest->numChannels != self->mOutputChannels)
        {
            ctx->mNumSamples = 0;                       // publish an EMPTY available frame
            ctx->mbChannelCount = lpRequest->numChannels;
            ctx->mfSampleRate = lpRequest->sampleRate;
            self->mPreviousSampleRate = lpRequest->sampleRate;
            self->mOutputChannels = lpRequest->numChannels;
            return 1;                                   // BUFFERSTATUS_AVAILABLE
        }

        // === 5. FEED CONSUME SCAN (0x82BA0684..0x82BA06F8) ===
        // The fill cursor is snapshotted ONCE; every advance of the free cursor is committed
        // to the object as it happens (the console stores it inside the loop, not after).
        if (self->mFeedDesc[self->mNextFeedSlotToFree].feedState == FEEDSTATE_FREE)
        {
            const u8 lu8Fill = self->mNextFeedSlotToFill;
            while (self->mNextFeedSlotToFree != lu8Fill)
            {
                u8 lu8Next = static_cast<u8>(self->mNextFeedSlotToFree + 1u);
                if (lu8Next == KU_MAX_DECODERFEEDS)
                    lu8Next = 0;
                self->mNextFeedSlotToFree = lu8Next;
                if (self->mFeedDesc[lu8Next].feedState != FEEDSTATE_FREE)
                    break;
            }
        }
        if (self->mFeedDesc[self->mNextFeedSlotToFree].feedState != FEEDSTATE_FED)
            goto epilogue;

        // === 6. START-TIME GATE (0x82BA0700..0x82BA07E4) ===
        if (lpRequest->startTime != 0.0)
        {
            u32 luFrames = 0;
            if (!self->WaitForStartTime(ctx, lpRequest->startTime, &luFrames))
            {
                self->mCurrentRequestSamplesPlayed = 0;
                goto epilogue;
            }
            if (luFrames != 0)
            {
                // A near-future start silences the gap. The cap is UNSIGNED (`cmplw`).
                if (luFrames >= self->mSamplesRequested)
                    luFrames = self->mSamplesRequested;
                SampleBuffer *lpDst = ctx->mpDstBuffer;
                for (u32 luChannel = 0; luChannel < lpRequest->numChannels; ++luChannel)
                {
                    std::memset(lpDst->mpSamples + lpDst->muStride * luChannel, 0,
                                luFrames * sizeof(f32));
                }
                ctx->mNumSamples = luFrames;
                SampleBuffer *lpOldSrc = ctx->mpSrcBuffer;   // the console's buffer SWAP
                ctx->mpSrcBuffer = lpDst;
                ctx->mpDstBuffer = lpOldSrc;
                ctx->mbChannelCount = lpRequest->numChannels;
                ctx->mfSampleRate = lpRequest->sampleRate;
                self->mCurrentRequestSamplesPlayed = 0;
                return 1;
            }
            lpRequest->startTime = 0.0;                  // start reached: disarm it
        }

        // === 7. SCRATCH CARVE (0x82BA07EC..0x82BA0828) -- no decoder-null guard ===
        {
            StackAllocator *lpStack = static_cast<StackAllocator *>(
                self->mpSystemUseGetSystemAccessor->mpObjectTable);
            const u32 luBytes =
                (static_cast<u32>(lpRequest->decoderInstanceSize) + 0x7Fu) & ~0x7Fu;
            lpCarveSaved = lpStack->mpTop;
            lpCarveNew = lpCarveSaved - luBytes;
            lpStack->mpTop = lpCarveNew;
        }

        self->mpLoadedDecoder = lpRequest->pDecoder;
        Decoder *lpDecoder = self->mpLoadedDecoder;
        s32 liAvailable = lpDecoder->GetSamplesRemaining(
            self->mFeedDesc[self->mNextFeedSlotToFree].decoderRequestHandle);

        // === 8. SKIP / DECODE BUDGET (0x82BA0870..0x82BA08F4) -- SIGNED comparisons ===
        s32 liToSkip = lpRequest->numSamplesToSkipPlayer;
        if (liAvailable < liToSkip)
            liToSkip = liAvailable;
        s32 liToDecode = static_cast<s32>(self->mSamplesRequested);
        if (liToDecode >= liAvailable - liToSkip)
            liToDecode = liAvailable - liToSkip;

        lpRequest = self->GetRequestInternal(self->mCurrentRequest);  // re-materialised
        SampleBuffer *lpDst = ctx->mpDstBuffer;
        while (liToSkip != 0)                            // `!= 0`, not `> 0`
        {
            const s32 liCount = (liToSkip < 256) ? liToSkip : 256;
            liToSkip -= liCount;
            liSkipped += lpDecoder->Decode(AsDecoderBuffer(lpDst), liCount);
        }

        // The main decode runs even at count zero -- the console does not guard it.
        liProduced = lpDecoder->Decode(AsDecoderBuffer(lpDst), liToDecode);

        SampleBuffer *lpOldSrc = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = lpDst;
        ctx->mNumSamples = liProduced;
        ctx->mpDstBuffer = lpOldSrc;
        ctx->mbChannelCount = lpRequest->numChannels;
        ctx->mfSampleRate = lpRequest->sampleRate;

        // === 9. PUBLISH / ACCOUNT (0x82BA0924..0x82BA0988) ===
        self->mCurrentRequestHandle = lpRequest->requestHandle;
        if (self->mCurrentRequestSamplesPlayed == 0)
        {
            // Seeded from the STREAM skip plus the DECODER skip (loads +0x24 then +0x20).
            self->mCurrentRequestSamplesPlayed =
                lpRequest->numSamplesToSkipStream + lpRequest->numSamplesToSkipDecoder;
        }
        liRemainingInFeed = (liAvailable - liProduced) - liSkipped;
        self->mCurrentRequestSamplesPlayed += liProduced + liSkipped;
        self->mCurrentRequestSampleRate = lpRequest->sampleRate;
        self->mCurrentRequestNumSamples = lpRequest->numSamples;
        self->mFeedDesc[self->mNextFeedSlotToFree].chunkSamplesPlayed += liProduced + liSkipped;

        // === 10. END OF REQUEST: loop, or retire and pre-load the next (0x82BA098C..) ===
        if (self->mCurrentRequestSamplesPlayed == lpRequest->numSamples)
        {
            if (lpRequest->loopStart >= 0)
            {
                self->mCurrentRequestSamplesPlayed = lpRequest->loopStart;
            }
            else
            {
                lpRequest->state = REQUESTSTATE_COMPLETE;
                if (self->mpLoadedDecoder != 0)
                {
                    self->mpLoadedDecoder = 0;
                    static_cast<StackAllocator *>(
                        self->mpSystemUseGetSystemAccessor->mpObjectTable)->mpTop = lpCarveSaved;
                }
                self->AdvanceCurrentRequest();
                RequestInternal *lpNext = self->GetRequestInternal(self->mCurrentRequest);
                // The RE-CARVE is guarded where the first carve was not.
                if (IsRequestActive(lpNext->state) && lpNext->pDecoder != 0)
                {
                    StackAllocator *lpStack = static_cast<StackAllocator *>(
                        self->mpSystemUseGetSystemAccessor->mpObjectTable);
                    const u32 luBytes =
                        (static_cast<u32>(lpNext->decoderInstanceSize) + 0x7Fu) & ~0x7Fu;
                    lpCarveSaved = lpStack->mpTop;
                    lpCarveNew = lpCarveSaved - luBytes;
                    lpStack->mpTop = lpCarveNew;
                    self->mpLoadedDecoder = lpNext->pDecoder;
                }
            }
        }

        // === 11. FEED ROLL (0x82BA0A50..0x82BA0AF4) ===
        while (liRemainingInFeed == 0)
        {
            SndPlayer1FeedDesc &lFeed = self->mFeedDesc[self->mNextFeedSlotToFree];
            if (lFeed.feedState != FEEDSTATE_FED)
                break;
            lFeed.feedState = FEEDSTATE_DECODECOMPLETED;
            u8 lu8Next = static_cast<u8>(self->mNextFeedSlotToFree + 1u);
            if (lu8Next == KU_MAX_DECODERFEEDS)
                lu8Next = 0;
            Decoder *lpLoaded = self->mpLoadedDecoder;   // read BEFORE the cursor store
            self->mNextFeedSlotToFree = lu8Next;
            if (lpLoaded != 0 && self->mFeedDesc[lu8Next].feedState == FEEDSTATE_FED)
            {
                liRemainingInFeed = lpLoaded->GetSamplesRemaining(
                    self->mFeedDesc[lu8Next].decoderRequestHandle);
            }
            // Otherwise the count stays zero and the console loops again -- so a NULL loaded
            // decoder retires every consecutive FED record. Reproduced, not repaired.
        }
    }

epilogue:
    // === 12. EPILOGUE (0x82BA0AFC..0x82BA0BCC) ===
    if (self->mpLoadedDecoder != 0)
    {
        self->mpLoadedDecoder = 0;
        if (lpCarveNew != 0)                             // only when a carve is outstanding
        {
            static_cast<StackAllocator *>(
                self->mpSystemUseGetSystemAccessor->mpObjectTable)->mpTop = lpCarveSaved;
        }
    }

    // The cached format is published on EVERY exit, including the unavailable one.
    ctx->mbChannelCount = self->mOutputChannels;
    ctx->mfSampleRate = self->mPreviousSampleRate;
    if (liProduced == 0)
    {
        if (liSkipped == 0 && self->mSamplesRequested != 0)
            return 0;          // ⚠️ ctx->mNumSamples DELIBERATELY left stale (console)
        ctx->mNumSamples = 0;
        return 1;
    }

    // === 13. LAST-SAMPLE CAPTURE for the declick ramp -- min(output, max) channels ===
    u32 luChannels = ctx->mbChannelCount;
    if (luChannels >= self->mMaxChannels)
        luChannels = self->mMaxChannels;
    SampleBuffer *lpSrc = ctx->mpSrcBuffer;
    f32 *lpDeclick = self->GetDeclickBuffer();
    for (u32 luChannel = 0; luChannel < luChannels; ++luChannel)
    {
        lpDeclick[luChannel] =
            lpSrc->mpSamples[lpSrc->muStride * luChannel + liProduced - 1];
    }
    self->mDcOffsetsGathered = 1;   // arms ONLY this byte; mNumDeclickSamples is untouched
    return 1;
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
