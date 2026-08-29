#ifndef CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_SNDPLAYER1SHARED_H
#define CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_SNDPLAYER1SHARED_H

#include "types.hpp"

#include "rw/audio/core/PlugIn.h"       // rw::audio::core::PlugIn (base) + Attribute_t
#include "rw/audio/core/TimerHandle.h"  // rw::audio::core::TimerHandle (mCpuTicks @ console +0x50)
#include "rw/audio/core/Voice.h"        // VoiceStageConfig (GetSize's config argument)

#include <cstddef> // size_t (the host layout computation)

// ============================================================================
// rw::audio::core::SndPlayer1_CgsStreamMod -- the streaming sound-player plugin
// (the Burnout build of rwaudiocore's SndPlayer1, fed by the Cgs stream module).
//
// SINGLE HEADER HOME (2026-08-25, audio-faithfulness wave 2): the two TUs
// sndplayer1.cpp + sndplayer1shared.cpp previously each defined a TU-local rival
// `SndPlayer1_CgsStreamMod` (one `u8 mPad[80] + s32`, one member-less) -- a live
// ODR violation inside the game link, with the bodies as raw byte-offset
// transliterations. Both now include THIS definition.
//
// LAYOUT GROUND TRUTH -- ⭐ REPLACED 2026-08-28 (phase E) BY THE DecFIGS DWARF FOR
// THIS EXACT CLASS: references/DecFIGS/dwarfdump/GameShared/GameClasses/Sound/
// Playback/Plugins/Streaming/internal/sndplayer1.h declares
// rw::audio::core::SndPlayer1_CgsStreamMod member-for-member, in order, with its
// nested SndPlayer1FeedDesc/RequestInternal records and its named constants. That is
// the authoritative source and it agrees with every ARTIST-attested offset.
//
// It SUPERSEDES the previous reconciliation, which extrapolated from the ProStreet
// PDB's `rw::audio::core::SndPlayer1` -- a DIFFERENT class (the vendor player) -- and
// then explained the deltas as "Burnout divergences". Two of those explanations were
// wrong, and both are fixed here: mFeedDesc is [20] of a POINTER-FREE 12-byte record
// (not [15] of an invented 16-byte one with two stream pointers), and the middle
// block's "Burnout-only extra word" is the named pair muDataReadForNewSize +
// mau8NewSize[4]. The old shape survived review because 15*16 == 20*12, so everything
// after mFeedDesc still landed on its correct offset.
//
// Still ARTIST-attested and unchanged: 0x150/0x154/0x158/0x15C (request cache),
// 0x178/0x17A/0x17C (the u16 triple; the asm's lhz 0x17C = mRequestInternalOffset),
// 0x181/0x182 (mCurrentRequest / mMaxRequests), and +0x50 = mTimerHandle.mCpuTicks
// (GetPpuTicksEvent @0x8268FE88).
//
// HOST-WIDTH FLAG: pointer members widen on the 64-bit host; members are pinned
// BY NAME + SEQUENCE (console offsets in comments only, no static_asserts). The
// RequestInternal ring is reached via mRequestInternalOffset -- a runtime-seeded
// BYTE offset into the object's tail allocation -- with sizeof(RequestInternal)
// stride, so the walk stays self-consistent once a host-side constructor seeds it.
// Nothing on the PC side constructs this type yet (the rw::audio engine bring-up
// owns that); these are the plugin's leaf methods landing ahead of it.
// ============================================================================

namespace rw
{
namespace audio
{
namespace core
{

class Decoder;
class Mixer;

class SndPlayer1_CgsStreamMod : public PlugIn
{
public:
    // X360 dword_82FFBA08 -- the stream-file path prefix the RWAC init stage seeds
    // ("SOUND\\STREAMS\\", RootSoundModule::Prepare @0x826FAF44 region; the PS3
    // DecFIGS names the store target spPathPrefix). Defined in sndplayer1shared.cpp
    // (null until the RWAC stage runs). Added 2026-08-25, faithful-audio-engine
    // phase A4.
    static const char* spPathPrefix;

    // ⭐ CORRECTED 2026-08-28 (phase E) FROM THE DecFIGS DWARF FOR THIS EXACT CLASS:
    // references/DecFIGS/dwarfdump/GameShared/GameClasses/Sound/Playback/Plugins/
    // Streaming/internal/sndplayer1.h declares
    //   struct SndPlayer1FeedDesc { bool mbStreamed; int chunkSamplesPlayed;
    //                               u8 decoderRequestHandle; u8 feedState;
    //                               u8 requestIndex; }
    // and `SndPlayer1FeedDesc mFeedDesc[20]` -- so the record is POINTER-FREE with a
    // 12-byte console stride and there are TWENTY slots.
    //
    // The previous shape here (two invented void* members, 16-byte stride, [15]) was
    // extrapolated from the ProStreet PDB's rw::audio::core::SndPlayer1 -- the VENDOR
    // player, whose feed record really does carry stream pointers -- and it happened to
    // span the same 0xF0 bytes (15*16 == 20*12), which is why the offsets after it still
    // landed correctly and the error went unnoticed. It was still wrong twice over: it
    // invented two members this class does not have, and it under-counted the slots, so
    // any feed-ring walk would have run 15 of 20 and mis-strided every entry.
    //
    // Being pointer-free is a useful property: this record does NOT widen on the host,
    // so its console offsets remain literally true.
    struct SndPlayer1FeedDesc
    {
        bool  mbStreamed;             // +0x00
        s32   chunkSamplesPlayed;     // +0x04
        u8    decoderRequestHandle;   // +0x08
        u8    feedState;              // +0x09
        u8    requestIndex;           // +0x0A  (+0x0B pad -> console stride 12)
    };

    enum { KU_MAX_DECODERFEEDS = 20 };        // DWARF MAX_DECODERFEEDS
    enum { KI_MAX_REQUEST_HANDLE_VALUE = 4194304 }; // DWARF MAX_REQUEST_HANDLE_VALUE
    enum { KU_NEWSIZE_BUFFER_SIZE = 4 };      // DWARF KU_NEWSIZE_BUFFER_SIZE

    // The request-ring entry. PDB `SndPlayer1::RequestInternal` NAMES; BURNOUT
    // console stride is 32 (asm rotlwi r9,r9,5 == index*32), i.e. the ProStreet
    // 48-byte form minus the three skip words (playerSkipRemaining / decoderSkip /
    // streamSkip) -- the remaining sequence lands state @ +0x1E exactly as the asm
    // reads (lbz 0x1E).
    struct RequestInternal
    {
        f64      startTime;           // +0x00
        Decoder* pDecoder;            // +0x08
        f32      requestHandle;       // +0x0C  (float-valued request handle)
        f32      sampleRate;          // +0x10
        s32      numSamples;          // +0x14
        s32      loopStart;           // +0x18
        u16      decoderInstanceSize; // +0x1C
        u8       state;               // +0x1E  (E_REQUESTSTATE_*)
        u8       numChannels;         // +0x1F
    };

    // DWARF RequestState. A request is ACTIVE while it is neither FREE nor COMPLETE.
    enum ERequestState
    {
        E_REQUESTSTATE_FREE         = 0,
        E_REQUESTSTATE_QUEUED       = 1,
        E_REQUESTSTATE_FEEDING      = 2,
        E_REQUESTSTATE_FEEDCOMPLETE = 3,
        E_REQUESTSTATE_COMPLETE     = 4
    };
    static bool IsRequestActive(u8 au8State)
    {
        return au8State != E_REQUESTSTATE_FREE && au8State != E_REQUESTSTATE_COMPLETE;
    }

    // DWARF StreamState -- the streaming read machine in RwacTimerClient.
    enum EStreamState
    {
        E_STREAMSTATE_READ_HEADER  = 0,   // accumulating the 4-byte chunk size
        E_STREAMSTATE_READ_CHUNK   = 1,   // reading the chunk body
        E_STREAMSTATE_SUBMIT_CHUNK = 2    // chunk complete, hand it to the feed ring
    };

    // One buffered stream chunk (console stride 8).
    struct Chunk
    {
        u32 size;   // +0x00 -- the allocated capacity, grown if a chunk exceeds it
        u8 *buf;    // +0x04
    };

    enum { KU_NUM_CHUNKS = 6 };
    enum { KU_DEFAULT_CHUNK_BYTES = 6500 };

    // The per-request streaming state (console stride 0x88). Kept beside the internal
    // request ring rather than inside it because only streamed play types use most of it.
    struct RequestExternal
    {
        f64   streamFileOffset;        // +0x00
        u8   *pSampleData;             // +0x08 -- payload start (past the packed header)
        s32   loopStartStreamOffset;   // +0x0C
        s32   gigaSamplesInRam;        // +0x10 -- how much of a "giga" asset is resident
        s32   numSamplesFed;           // +0x14
        s32   numBytesFed;             // +0x18
        char *pStreamLoopFileName;     // +0x1C
        void *pReadStream;             // +0x20 -- CgsFileSystem::ReadStream* (un-homed here)
        u32   streamState;             // +0x24 -- EStreamState
        u8   *pStreamBuffer;           // +0x28
        Chunk chunks[KU_NUM_CHUNKS];   // +0x2C .. +0x5B
        u32   readBufferSelect;        // +0x5C
        u32   writeBufferSelect;       // +0x60
        u32   unlockBufferSelect;      // +0x64
        u32   readSize;                // +0x68
        u32   readPointer;             // +0x6C
        u32   queuedChunks;            // +0x70
        u32   lockedChunks;            // +0x74
        u8    codec;                   // +0x78
        u8    playType;                // +0x79 -- 0 resident, 1 streamed, 2 "giga" hybrid
        u8    latestFeedSlot;          // +0x7A
        u8    expelMode;               // +0x7B
        u8   *pNextChunk;              // +0x7C
        u8   *pLoopStartChunk;         // +0x80
    };

    // @ 0x8268CD20. Advance the current-request cursor around the ring and cache
    // the new current request's parameters (if it is still active).
    SndPlayer1_CgsStreamMod* AdvanceCurrentRequest();

    // @ 0x8268FE88. Return the plugin timer's tick counter (`*(this+0x50)` ==
    // mTimerHandle.mCpuTicks by name).
    int GetPpuTicksEvent() const;

    // ---- the self-contained callbacks/helpers (phase E) --------------------------------
    static int GetSize(const VoiceStageConfig *config);                 // @0x826A4210
    static int PreProcess(SndPlayer1_CgsStreamMod *self, Mixer *ctx,
                          bool discontinuity, int outputSamplesRequested); // @0x8268CD10
    bool WaitForStartTime(Mixer *ctx, f64 adStartTime, u32 *apuSilence); // @0x8268CAF8
    int  Declick(Mixer *ctx);                                            // @0x8268CB78
    bool GetFeedSlot(s32 *apiSlot);                                      // @0x826A4348
    void UnpackHeader(u32 auRequestIndex, void *apPacked);               // @0x8268C990

    // ---- the instance's two variable-length tails --------------------------------------
    // Both live past the fixed object, at byte offsets recorded in the 16-bit fields the
    // console keeps for exactly this purpose -- so the walk stays self-consistent at host
    // widths without any console literal appearing in the arithmetic.
    RequestInternal &Request(u32 auIndex)
    {
        return *reinterpret_cast<RequestInternal *>(
            reinterpret_cast<u8 *>(this) + mRequestInternalOffset
            + sizeof(RequestInternal) * auIndex);
    }
    const RequestInternal &Request(u32 auIndex) const
    {
        return *reinterpret_cast<const RequestInternal *>(
            reinterpret_cast<const u8 *>(this) + mRequestInternalOffset
            + sizeof(RequestInternal) * auIndex);
    }
    f32 *DeclickBuffer()
    {
        return reinterpret_cast<f32 *>(reinterpret_cast<u8 *>(this) + mDeclickBufferOffset);
    }

    // ⭐ THE ONE HOST LAYOUT COMPUTATION. GetSize and CreateInstance must agree exactly,
    // so both derive from this. The console spells the same thing with its own extents
    // (`align_up(0x188 + 4*channels, 8)` then `+ 0x20 * requests`), and BOTH of those are
    // console sizes containing 32-bit pointers -- transliterating either would place the
    // declick buffer and the request ring INSIDE the host object. (Exactly the bug found
    // and fixed in Resample and GinsuPlayer this same wave.)
    struct HostLayout
    {
        u32 declickOffset;   // -> mDeclickBufferOffset
        u32 requestOffset;   // -> mRequestInternalOffset
        u32 totalBytes;      // -> GetSize
    };
    static HostLayout ComputeHostLayout(u32 auChannels, s32 aiMaxRequests);

    // ---- layout (console offsets in comments; ⭐ names/order now DWARF-authoritative,
    //      from the DecFIGS header for THIS class rather than the ProStreet PDB for the
    //      vendor SndPlayer1) ----
    Attribute_t      mAttribute[3];                          // +0x28
    TimerHandle      mTimerHandle;                           // +0x40 (mCpuTicks @ +0x50)
    void*            mpRequestExternal;                      // +0x58 (RequestExternal*, un-homed)
    SndPlayer1FeedDesc mFeedDesc[KU_MAX_DECODERFEEDS];       // +0x5C (20 slots, 12-byte stride)
    Decoder*         mpLoadedDecoder;                        // +0x14C
    f32              mCurrentRequestHandle;                  // +0x150 (asm stfs 0x150)
    f32              mCurrentRequestSampleRate;              // +0x154 (asm stfs 0x154)
    s32              mCurrentRequestSamplesPlayed;           // +0x158 (asm stw 0x158)
    s32              mCurrentRequestNumSamples;               // +0x15C (asm stw 0x15C)
    // ⭐ CORRECTED: this is an EMBEDDED float, not a pointer. The DWARF declares
    // `float mRequestHandle`, and the asm reads/writes it with lfs/stfs directly at
    // 0x826DB598..0x826DB5B8 -- a pointer load would be an lwz. (The vendor SndPlayer1
    // does have a `float *mpRequestHandle`; that is where the old spelling came from.)
    f32              mRequestHandle;                         // +0x160
    f32              mLastRequestHandleProcessed;            // +0x164
    f32              mLastRequestHandleSuccessfullyProcessed;// +0x168
    f32              mPreviousSampleRate;                    // +0x16C
    // ⭐ CORRECTED: the former "mUnknown170" is not unknown. The DWARF names this pair,
    // and the timer fills both at 0x826EA200..0x826EA238 -- it is the streamed-content
    // "new size" re-read: a byte counter plus the 4-byte big-endian size field being
    // accumulated out of the stream.
    u32              muDataReadForNewSize;                   // +0x170
    u8               mau8NewSize[KU_NEWSIZE_BUFFER_SIZE];    // +0x174
    u16              mSamplesRequested;                      // +0x178
    u16              mDeclickBufferOffset;                   // +0x17A
    u16              mRequestInternalOffset;                 // +0x17C (asm lhz 0x17C; byte offset of the
                                                             //        RequestInternal ring in the tail alloc)
    u8               mMaxChannels;                           // +0x17E
    u8               mNextFreeRequest;                       // +0x17F
    u8               mNextRequestToFree;                     // +0x180
    u8               mCurrentRequest;                        // +0x181 (asm lbz/stb 0x181)
    u8               mMaxRequests;                           // +0x182 (asm lbz 0x182)
    u8               mDcOffsetsGathered;                     // +0x183
    u8               mNumDeclickSamples;                     // +0x184
    u8               mNextFeedSlotToFill;                    // +0x185
    u8               mNextFeedSlotToFree;                    // +0x186
    u8               mTimerAdded;                            // +0x187
};

} // namespace core
} // namespace audio
} // namespace rw

#endif // CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_SNDPLAYER1SHARED_H
