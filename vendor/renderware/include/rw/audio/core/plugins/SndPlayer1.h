#pragma once

// =====================================================================================
// rw::audio::core::SndPlayer1 -- the engine's sample player: the SOURCE stage of every
// splice voice ('SnP1' is stage 0 of all 88 of them), and the gate the whole splice-voice
// staging waits on.
//
// It owns a ring of REQUESTS (each a scheduled sound: start time, decoder, sample range,
// loop point) and a ring of FEEDS (each a chunk handed to the decoder). A play event queues
// a request; the per-frame timer starts it, creates its decoder and pumps chunks in; and
// Process pulls decoded samples out into the mixer's buffer.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the asm is
// authoritative. Full body decode, adversarially verified:
// progress/scratch_dossiers/sndplayer1_bodies_decode.md -- READ ITS VERIFICATION SECTION
// before touching this type; it enumerates about a dozen places where the natural
// transliteration of this class is WRONG on x64.
//
// ⭐ NAMES come from the Feb-2007 vendor header (rwaudiocore 2.11.00
// rw/audio/core/plugins/sndplayer1.h) -- the same class one minor version older. The ARTIST
// build is 3.03-era and ADDS fields; those are marked [3.03] below. Where the two disagree
// the ARTIST asm wins, and the one place it matters is called out at mpRequestHandle.
//
// Descriptor off_82F901C4 'SnP1', plugInType 0 (SOURCE stage), 1 constructor parameter,
// 3 attributes, 6 events.
// =====================================================================================

#include "types.hpp"                        // f32, f64, s32, u32, u16, u8
#include "rw/audio/core/PlugIn.h"           // PlugIn (polymorphic base) + Attribute_t + System
#include "rw/audio/core/TimerHandle.h"      // TimerHandle (the per-frame timer client)
#include "rw/audio/core/Voice.h"            // VoiceStageConfig (GetSize's argument)

#include <cstddef> // size_t

namespace rw
{
namespace audio
{
namespace core
{

class Mixer;
typedef Mixer AudioProcessContext;
class Decoder;
class SampleBuffer;

// ⚠️ NOT HOMED YET. The streaming half of this class talks to a StreamPool and to
// rw::core::filesys::Stream (its ChunkInfo and RequestId included). Neither surface exists
// in this tree, so the members that reference them are declared as opaque pointers with
// their console types named in the comment. They are STORAGE ONLY here -- every body that
// would dereference them lives in the streaming slice, which lands with those homes.
class StreamPool;
namespace filesysfwd { class Stream; }

// The host layout of one instance's two variable-length tails. See ComputeLayout.
struct SndPlayer1Layout
{
    u16 muDeclickBufferOffset;   // -> mDeclickBufferOffset
    u16 muRequestInternalOffset; // -> mRequestInternalOffset
    u32 muTotalSize;             // -> GetSize's return
};

class SndPlayer1 : public PlugIn
{
public:
    enum { KU_GUID = 0x536E5031u };                        // 'SnP1'
    enum { KU_MAX_DECODERFEEDS = 20 };                     // vendor MAX_DECODERFEEDS
    enum { KI_MAX_REQUEST_HANDLE_VALUE = 4194304 };        // vendor MAX_REQUEST_HANDLE_VALUE

    // The one constructor parameter: how deep to make the request ring. The splice configs
    // pass &1.0f (mono voices) and &2.0f (stereo voices).
    struct ConstructorParams { f32 maxRequests; };

    enum Attribute
    {
        ATTRIBUTE_GETCURRENTREQUEST  = 0,   // f32 slot
        ATTRIBUTE_GETSAMPLEPOSITION  = 1,   // ⚠️ written as f64 ACROSS the whole 8-byte slot
        ATTRIBUTE_GETSAMPLELENGTH    = 2,   // ⚠️ likewise f64
        ATTRIBUTE_MAX = 3
    };

    enum RequestState
    {
        REQUESTSTATE_FREE         = 0,
        REQUESTSTATE_QUEUED       = 1,
        REQUESTSTATE_FEEDING      = 2,
        REQUESTSTATE_FEEDCOMPLETE = 3,
        REQUESTSTATE_COMPLETE     = 4
    };
    // FREE and COMPLETE are dead; the three in between are live.
    static bool IsRequestActive(u8 au8State)
    {
        return au8State != REQUESTSTATE_FREE && au8State != REQUESTSTATE_COMPLETE;
    }

    enum FeedState
    {
        FEEDSTATE_FREE            = 0,
        FEEDSTATE_FED             = 1,
        FEEDSTATE_DECODECOMPLETED = 2
    };

    // ---- RequestInternal: one scheduled sound. Console stride 0x30, but it carries a
    // Decoder* and a leading f64, so ⚠️ NEVER use 0x30 on the host -- always sizeof().
    struct RequestInternal
    {
        f64 startTime;                 // +0x00 scheduled start; cleared once reached
        Decoder *pDecoder;             // +0x08 *** WIDENS *** StartRequest stores the factory result
        f32 requestHandle;             // +0x0C the caller's handle for this request
        f32 sampleRate;                // +0x10 UnpackHeader's 18-bit field
        s32 numSamples;                // +0x14 UnpackHeader's 29-bit field
        s32 loopStart;                 // +0x18 -1 == no loop
        s32 numSamplesToSkipPlayer;    // +0x1C [3.03] Process's skip budget
        s32 numSamplesToSkipDecoder;   // +0x20 [3.03]
        s32 numSamplesToSkipStream;    // +0x24 [3.03]
        u16 decoderInstanceSize;       // +0x28 ⚠️ a 32->16 TRUNCATION of Decoder+0x20, and
                                       //       that total GROWS on the host -- see the
                                       //       report's hazard list before trusting it
        u8  state;                     // +0x2A RequestState -- the ONLY field the ctor writes
        u8  numChannels;               // +0x2B UnpackHeader's 6-bit field, plus one
    };

    // ---- RequestExternal: the streaming state for one request. Console stride 0x50 with
    // SEVEN pointers, so ⚠️ never 0x50 on the host.
    struct RequestExternal
    {
        f64 streamFileOffset;          // +0x00 (the console puts THIS at allocation base + 4)
        char *pSampleData;             // +0x08 *** WIDENS *** payload, past the packed header
        s32 loopStartStreamOffset;     // +0x0C
        s32 gigaSamplesInRam;          // +0x10 resident prefix of a hybrid asset
        s32 numSamplesFed;             // +0x14
        s32 numBytesFed;               // +0x18
        char *pStreamLoopFileName;     // +0x1C *** WIDENS ***
        StreamPool *pStreamPool;       // +0x20 *** WIDENS *** (un-homed; storage only)
        void *streamHandle;            // +0x24 *** WIDENS *** StreamPool::StreamHandle
        void *pRwCoreStream;           // +0x28 *** WIDENS *** rw::core::filesys::Stream*
        u32  streamerRequestId;        // +0x2C rw::core::filesys::Stream::RequestId
        char *pNextChunk;              // +0x30 *** WIDENS ***
        char *pLoopStartChunk;         // +0x34 *** WIDENS ***
        s32 mSeekBlockA;               // +0x38 [3.03] SeekTableParser output
        s32 mSeekBlockB;               // +0x3C [3.03]
        s32 mSeekStreamOffset;         // +0x40 [3.03]
        s32 mSeekBlockD;               // +0x44 [3.03]
        u8  codec;                     // +0x48 indexes the 8-entry decoder GUID table
        u8  playType;                  // +0x49 0 resident, 1 streamed, 2 hybrid
        u8  feedSlotLatest;            // +0x4A
        u8  expelMode;                 // +0x4B
        u8  mNoSeekTable;              // +0x4C [3.03]
    };

    // ---- SndPlayer1FeedDesc: one chunk in flight. Console stride 0x10, count 20.
    // ⚠️ This is NOT the SndPlayer1_CgsStreamMod feed record -- that fork's is 12 bytes and
    // pointer-free. This one carries TWO pointers, so it widens.
    struct SndPlayer1FeedDesc
    {
        void *pChunkInfo;              // +0x00 *** WIDENS *** Stream::ChunkInfo*
        void *pRwCoreStream;           // +0x04 *** WIDENS *** Stream*
        s32   chunkSamplesPlayed;      // +0x08
        u8    decoderRequestHandle;    // +0x0C Decoder::Feed's return
        u8    feedState;               // +0x0D FeedState
        u8    requestIndex;            // +0x0E the owning request
        u8    mPad0F;                  // +0x0F (no writer found)
    };

    // ---- the four vtable slots (the committed PlugIn base maps slot 0 to the destructor
    // and slot 3 to Destroy, and PlugIn::CreateInstance's failure path calls exactly those
    // two, so the overrides keep that mapping) -------------------------------------------
    virtual ~SndPlayer1();                              // vt[0] ReleaseEvent @0x82BA4178
    virtual int  Event(int aiEventId, void *apParam);   // vt[1] EventEvent   @0x82BA5C48
    virtual int  VFunc2();                              // vt[2] GetPpuTicksEvent @0x82BDD2D0
    virtual void Destroy(int aFlags);                   // vt[3] deleting dtor @0x82B9EAF8

    static char **GetPlugInDescRunTime();                              // @0x82B9BE60
    static int    GetSize(const VoiceStageConfig *apConfig);           // @0x82BA0220
    static int    CreateInstance(SndPlayer1 *self,
                                 const ConstructorParams *apParams);   // @0x82BA6C80
    static int    PreProcess(SndPlayer1 *self, AudioProcessContext *ctx,
                             bool discontinuity, int aiRequestedCount);// @0x82B9C2D8
    static int    Process(SndPlayer1 *self, AudioProcessContext *ctx,
                          bool discontinuity);                         // @0x82BA0568

    bool WaitForStartTime(AudioProcessContext *ctx, f64 adStartTime,
                          u32 *apuSamples);                            // @0x82B9C148
    int  Declick(AudioProcessContext *ctx);                            // @0x82B9C1C0
    void AdvanceCurrentRequest();                                      // @0x82B9C2E8
    bool GetFeedSlot(u32 *apuOutSlot);                                 // @0x82BA0380

    // The per-frame timer client the constructor registers.
    static void RwacTimerClient(void *apContext, f32 afTimeToNextCall); // @0x82BA6980

    // ⭐ THE ONE HOST LAYOUT HELPER. GetSize and CreateInstance MUST both go through it, or
    // the allocation and the placement disagree and the request ring overruns its buffer.
    //
    // The console spells it with two literals -- declick at 0x1D8, requests at
    // align_up(0x1D8 + 4*channels, 8), total + 0x30 per request. BOTH are console extents
    // stuffed with 32-bit pointers (0x1D8 spans ~49 pointer slots; 0x30 holds a Decoder*),
    // so neither survives. This is the same hazard class as Resample, GinsuPlayer, SubMix
    // and the stream-mod; it is solved the same way everywhere.
    static SndPlayer1Layout ComputeLayout(u8 au8Channels, u32 auMaxRequests);

    // ⚠️ The console's float->int for the request count is `fctidz` + `stfiwx`: a SATURATING
    // 64-bit truncate whose LOW 32 BITS are stored. NaN yields 0 and a huge positive yields
    // -1, where an x64 cast would give INT_MIN for both. GetSize and CreateInstance share
    // this helper precisely so they cannot disagree.
    static u32 TruncateRequestCount(f32 afValue);

    // ---- the two variable-length tails. NOT members: they live past the object at the
    // stored 16-bit offsets, which is the mechanism that keeps the widening invisible to
    // every consumer (the vendor header has these same two accessors).
    RequestInternal *GetRequestInternal(u32 auIndex)
    {
        RequestInternal *lpArray = reinterpret_cast<RequestInternal *>(
            reinterpret_cast<char *>(this) + mRequestInternalOffset);
        return &lpArray[auIndex];
    }
    f32 *GetDeclickBuffer()
    {
        return reinterpret_cast<f32 *>(
            reinterpret_cast<char *>(this) + mDeclickBufferOffset);
    }

    // The two f64 attribute slots. ⚠️ Slots 1 and 2 are written by the timer as DOUBLES
    // spanning the whole 8-byte Attribute_t, while the generic PlugIn::GetAttribute reads
    // the slot's leading f32. On the big-endian console the double's high word lands where
    // that f32 is; on the little-endian host it does not. These accessors keep the console's
    // f64 storage; any consumer that wants the generic f32 view must go through
    // PlugIn::GetAttribute and is subject to that divergence, which is recorded rather than
    // papered over.
    void SetSamplePositionAttribute(f64 adValue)
    {
        *reinterpret_cast<f64 *>(&mAttribute[ATTRIBUTE_GETSAMPLEPOSITION]) = adValue;
    }
    void SetSampleLengthAttribute(f64 adValue)
    {
        *reinterpret_cast<f64 *>(&mAttribute[ATTRIBUTE_GETSAMPLELENGTH]) = adValue;
    }

    // ---- layout (console offsets documentary; host widths, by-name access) -------------
    Attribute_t mAttribute[ATTRIBUTE_MAX];             // +0x28 .. +0x3F
    TimerHandle mTimerClient;                          // +0x40
    RequestExternal *mpRequestExternal;                // +0x58 == allocation base + 4
    SndPlayer1FeedDesc mFeedDesc[KU_MAX_DECODERFEEDS]; // +0x5C .. +0x19B
    Decoder *mpLoadedDecoder;                          // +0x19C (Process zeroes it each entry)
    f32 mCurrentRequestHandle;                         // +0x1A0
    f32 mCurrentRequestSampleRate;                     // +0x1A4
    s32 mCurrentRequestSamplesPlayed;                  // +0x1A8
    s32 mCurrentRequestNumSamples;                     // +0x1AC
    // ⚠️ A POINTER, not an inline float. The 2.11 vendor header has `float mRequestHandle`
    // here; the 3.03 ARTIST build dereferences this and System::Free's it, so the older
    // shape must NOT be used. It is the base of the one private allocation, whose first
    // word is the running request-handle counter and whose tail is the RequestExternal array.
    f32 *mpRequestHandle;                              // +0x1B0
    f32 mLastRequestHandleProcessed;                   // +0x1B4
    f32 mLastRequestHandleSuccessfullyProcessed;       // +0x1B8
    f32 mPreviousSampleRate;                           // +0x1BC
    u16 mSamplesRequested;                             // +0x1C0 (PreProcess; not ctor-set)
    u16 mDeclickBufferOffset;                          // +0x1C2
    u16 mRequestInternalOffset;                        // +0x1C4
    u8  mMaxChannels;                                  // +0x1C6
    u8  mNextFreeRequest;                              // +0x1C7
    u8  mNextRequestToFree;                            // +0x1C8
    u8  mCurrentRequest;                               // +0x1C9
    u8  mMaxRequests;                                  // +0x1CA ⚠️ the LOW BYTE of the count
    u8  mDcOffsetsGathered;                            // +0x1CB
    u8  mNumDeclickSamples;                            // +0x1CC
    u8  mNextFeedSlotToFill;                           // +0x1CD
    u8  mNextFeedSlotToFree;                           // +0x1CE
    u8  mNextFeedSlotToCleanup;                        // +0x1CF [3.03]
    u8  mbTimerAdded;                                  // +0x1D0
};

} // namespace core
} // namespace audio
} // namespace rw
