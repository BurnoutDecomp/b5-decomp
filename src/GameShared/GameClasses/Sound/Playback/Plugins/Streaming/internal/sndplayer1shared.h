#ifndef CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_SNDPLAYER1SHARED_H
#define CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_SNDPLAYER1SHARED_H

#include "types.hpp"

#include "rw/audio/core/PlugIn.h"       // rw::audio::core::PlugIn (base) + Attribute_t
#include "rw/audio/core/TimerHandle.h"  // rw::audio::core::TimerHandle (mCpuTicks @ console +0x50)

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
// LAYOUT GROUND TRUTH (two sources, reconciled):
//   * ProStreet08Milestone.pdb `rw::audio::core::SndPlayer1` [sizeof 464,
//     : public PlugIn] -- authoritative member NAMES/TYPES (mCurrentRequestHandle
//     IS a float: the request-handle API is float-valued, see `float*
//     mpRequestHandle`; the old "a handle is not a float" doubt is settled by the
//     ARTIST asm's lfs/stfs pairs @0x8268CD90-0x8268CD9C).
//   * BURNOUT_X360_ARTIST asm (AdvanceCurrentRequest @0x8268CD20, GetPpuTicksEvent
//     @0x8268FE88) -- authoritative Burnout OFFSETS. Burnout DIVERGES from
//     ProStreet by -80 bytes across the request tail (mCurrentRequestHandle @0x150
//     vs PDB 0x1A0) => mFeedDesc is [15] not [20] (0x5C..0x14B = 15 * 16), and by
//     +8 bytes inside the 0x160..0x177 middle block (one extra word vs ProStreet;
//     see mUnknown170). Every asm-attested offset lands exactly on this layout:
//     0x150/0x154/0x158/0x15C (request cache), 0x178/0x17A/0x17C (u16 triple, the
//     asm's lhz 0x17C = mRequestInternalOffset), 0x181/0x182 (mCurrentRequest /
//     mMaxRequests), +0x50 = mTimerHandle.mCpuTicks (GetPpuTicksEvent).
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

class SndPlayer1_CgsStreamMod : public PlugIn
{
public:
    // X360 dword_82FFBA08 -- the stream-file path prefix the RWAC init stage seeds
    // ("SOUND\\STREAMS\\", RootSoundModule::Prepare @0x826FAF44 region; the PS3
    // DecFIGS names the store target spPathPrefix). Defined in sndplayer1shared.cpp
    // (null until the RWAC stage runs). Added 2026-08-25, faithful-audio-engine
    // phase A4.
    static const char* spPathPrefix;

    // PDB rw::audio::core::SndPlayer1::SndPlayer1FeedDesc [16 console bytes].
    // FLAG (erased pointee types): pChunkInfo / pRwCoreStream are
    // rw::core::filesys::Stream::ChunkInfo* / Stream* in the PDB; held as void*
    // here (Stream's nested ChunkInfo cannot be forward-declared) until the
    // rw::core::filesys surface is homed.
    struct SndPlayer1FeedDesc
    {
        void* pChunkInfo;             // +0x00  rw::core::filesys::Stream::ChunkInfo*
        void* pRwCoreStream;          // +0x04  rw::core::filesys::Stream*
        s32   chunkSamplesPlayed;     // +0x08
        u8    decoderRequestHandle;   // +0x0C
        u8    feedState;              // +0x0D
        u8    requestIndex;           // +0x0E  (pad to 16)
    };

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

    enum ERequestState
    {
        E_REQUESTSTATE_FREE     = 0,
        E_REQUESTSTATE_COMPLETE = 4
    };

    // @ 0x8268CD20. Advance the current-request cursor around the ring and cache
    // the new current request's parameters (if it is still active).
    SndPlayer1_CgsStreamMod* AdvanceCurrentRequest();

    // @ 0x8268FE88. Return the plugin timer's tick counter (`*(this+0x50)` ==
    // mTimerHandle.mCpuTicks by name).
    int GetPpuTicksEvent() const;

    // ---- layout (console offsets in comments; PDB names) ----
    Attribute_t      mAttribute[3];                          // +0x28
    TimerHandle      mTimerHandle;                           // +0x40 (mCpuTicks @ +0x50)
    void*            mpRequestExternal;                      // +0x58 (SndPlayer1::RequestExternal*, un-homed)
    SndPlayer1FeedDesc mFeedDesc[15];                        // +0x5C (BURNOUT: 15 slots; ProStreet has 20)
    Decoder*         mpLoadedDecoder;                        // +0x14C
    f32              mCurrentRequestHandle;                  // +0x150 (asm stfs 0x150)
    f32              mCurrentRequestSampleRate;              // +0x154 (asm stfs 0x154)
    s32              mCurrentRequestSamplesPlayed;           // +0x158 (asm stw 0x158)
    s32              mCurrentRequestNumSamples;               // +0x15C (asm stw 0x15C)
    f32*             mpRequestHandle;                        // +0x160
    f32              mLastRequestHandleProcessed;            // +0x164
    f32              mLastRequestHandleSuccessfullyProcessed;// +0x168
    f32              mPreviousSampleRate;                    // +0x16C
    u32              mUnknown170;                            // +0x170 FLAG: Burnout-only extra word vs
                                                             //        ProStreet (its exact position inside
                                                             //        0x160..0x177 is unverified)
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
