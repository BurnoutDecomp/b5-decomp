#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/CgsStreamingPlugin.h"
#include "rw/audio/core/Decoder.h"
#include "rw/audio/core/DecoderRegistry.h"
#include "rw/audio/core/Mixer.h"
#include "rw/audio/core/TimerManager.h"
#include "rw/audio/core/Voice.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <new>

extern "C" CgsSound::Playback::IStreamProvider* off_82FFBA0C;

CgsSound::Playback::IStreamProvider::StreamSpec::StreamSpec() = default;

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268CD20
//   (rw::audio::core::SndPlayer1_CgsStreamMod::AdvanceCurrentRequest)
//
// Advances the "current request" cursor around the request ring, then caches the
// new current request's parameters into the mCurrentRequest* fields -- but only if
// that request is still active (state is neither COMPLETE(4) nor FREE(0)). When
// the request is inactive, handle/sampleRate keep their previous values and only
// samplesPlayed/numSamples are cleared -- matching the asm exactly.
//
//   asm: lbz 0x181 (+1, wrap vs lbz 0x182) -> stb 0x181
//        stw 0, 0x158 ; stw 0, 0x15C
//        entry = this + lhz(0x17C) + current*32          (rotlwi r9,r9,5)
//        lbz entry+0x1E; ==4 || ==0 -> return
//        stw 0, 0x158
//        lfs/stfs entry+0xC  -> 0x150   (float request handle)
//        lfs/stfs entry+0x10 -> 0x154   (float sample rate)
//        lwz/stw entry+0x14 -> 0x15C   (numSamples)
//
// (2026-08-25, audio-faithfulness wave 2: the former TU-local member-less rival
// struct + raw byte-offset transliteration is retired; the single header home
// carries the PDB-named layout and this body reads it BY NAME. The console *32
// entry stride is sizeof(RequestInternal) over the runtime-seeded
// mRequestInternalOffset tail walk on the host.)

namespace rw { namespace audio { namespace core {

    // X360 dword_82FFBA08 (see the header note). Null until the RWAC init stage
    // seeds "SOUND\\STREAMS\\".
    const char* SndPlayer1_CgsStreamMod::spPathPrefix = 0;

    SndPlayer1_CgsStreamMod* SndPlayer1_CgsStreamMod::AdvanceCurrentRequest()
    {
        // Bump + wrap the current-request cursor.
        u8 lu8Current = static_cast<u8>(mCurrentRequest + 1);
        mCurrentRequest = lu8Current;
        if (lu8Current == mMaxRequests)
        {
            lu8Current = 0;
            mCurrentRequest = 0;
        }

        mCurrentRequestSamplesPlayed = 0;
        mCurrentRequestNumSamples    = 0;

        // The RequestInternal ring lives in the object's tail allocation at the
        // runtime-seeded byte offset mRequestInternalOffset (console stride 32 ==
        // sizeof(RequestInternal) there; host stride is the host sizeof).
        const RequestInternal* lpEntry = reinterpret_cast<const RequestInternal*>(
            reinterpret_cast<char*>(this) + mRequestInternalOffset
            + lu8Current * sizeof(RequestInternal));

        if (lpEntry->state != E_REQUESTSTATE_COMPLETE &&
            lpEntry->state != E_REQUESTSTATE_FREE)
        {
            mCurrentRequestSamplesPlayed = 0;
            mCurrentRequestHandle        = lpEntry->requestHandle;   // lfs/stfs (float)
            mCurrentRequestSampleRate    = lpEntry->sampleRate;      // lfs/stfs (float)
            mCurrentRequestNumSamples    = lpEntry->numSamples;
        }
        return this;
    }

}}}

// =====================================================================================
// PHASE E (2026-08-29) -- the complete game streaming-player plug-in.
//
// Decode report: progress/scratch_dossiers/streammod_gainarray_decode_codex.md (TARGET 1).
// =====================================================================================

#include "rw/audio/core/BitGetter.h" // the packed-header bit reader

namespace rw { namespace audio { namespace core {

namespace
{
    inline u32 AlignUp8(u32 auValue) { return (auValue + 7u) & ~7u; }

    inline u32 AlignUp4(u32 auValue) { return (auValue + 3u) & ~3u; }
    inline u32 AlignUp128(u32 auValue) { return (auValue + 127u) & ~127u; }
    inline u32 MinU32(u32 a, u32 b) { return a < b ? a : b; }

    u64 PpcFctidzBits(f64 value)
    {
        if (value != value)
            return 0x8000000000000000ULL;
        if (value >= 9223372036854775808.0)
            return 0x7FFFFFFFFFFFFFFFULL;
        if (value <= -9223372036854775808.0)
            return 0x8000000000000000ULL;
        return static_cast<u64>(static_cast<s64>(value));
    }

    s32 PpcFctidzLowS32(f64 value)
    {
        return static_cast<s32>(static_cast<u32>(PpcFctidzBits(value)));
    }

    u32 PpcFctidzLowU32(f64 value)
    {
        return static_cast<u32>(PpcFctidzBits(value));
    }

    u16 CheckedU16(u32 value)
    {
        assert(value <= (std::numeric_limits<u16>::max)());
        return static_cast<u16>(value);
    }

    u8 AdvanceRequestEquality(u8 index, u8 maxRequests)
    {
        u8 next = static_cast<u8>(index + 1u);
        if (next == maxRequests)
            next = 0;
        return next;
    }

    u32 ReadBe32(const u8* p)
    {
        return (static_cast<u32>(p[0]) << 24) |
               (static_cast<u32>(p[1]) << 16) |
               (static_cast<u32>(p[2]) << 8) |
                static_cast<u32>(p[3]);
    }

    void WriteBe32(u8* p, u32 value)
    {
        p[0] = static_cast<u8>(value >> 24);
        p[1] = static_cast<u8>(value >> 16);
        p[2] = static_cast<u8>(value >> 8);
        p[3] = static_cast<u8>(value);
    }

    f64 LoadAttributeDouble(const PlugIn::Attribute_t& slot)
    {
        static_assert(sizeof(slot) == sizeof(f64), "RWAC attribute slot");
        f64 value;
        std::memcpy(&value, &slot, sizeof(value));
        return value;
    }

    void StoreAttributeDouble(PlugIn::Attribute_t& slot, f64 value)
    {
        static_assert(sizeof(slot) == sizeof(f64), "RWAC attribute slot");
        std::memcpy(&slot, &value, sizeof(value));
    }

    template <class T>
    T* ReserveDeferred(System* system, u32 bytes)
    {
        T* command = reinterpret_cast<T*>(
            system->mpDeferredRingBase + system->muDeferredRingCursor);
        system->muDeferredRingCursor += bytes;
        return command;
    }

    typedef s32 (*JStrCommandHandler)(void*);

    struct PlayParams
    {
        f64 startTime;
        f64 streamFileOffset;
        const char* pStreamFilePath;
        void* pRamData;
        u32 streamPoolGuid;
        f32 expelMode;
        f32 requestHandle;
    };

    struct IsRequestDoneParams
    {
        f32 requestHandle;
        f32 isRequestDone;
    };

    struct GetRequestBufferedParams
    {
        f32 requestHandle;
        f32 streamBytesBuffered;
        f32 isFullyBuffered;
    };

    struct ModifyStartTimeParams
    {
        f64 newStartTime;
        f32 requestHandle;
    };

    struct JStrCommandBase
    {
        JStrCommandHandler handler;
        SndPlayer1_CgsStreamMod* self;
    };

    struct JStrStopCommand
    {
        JStrCommandBase base;
    };

    struct JStrModifyStartTimeCommand
    {
        JStrCommandBase base;
        f64 startTime;
        f32 requestHandle;
    };

    struct JStrPlayCommand
    {
        JStrCommandBase base;
        f64 startTime;
        f64 streamFileOffset;
        u32 streamPoolGuid;
        const void* pRamData;
        u16 recordBytes;
        u8 expelMode;
        u8 pad;
        f32 requestHandle;
    };
}

// -------------------------------------------------------------------------------------
// ComputeHostLayout -- the single source of truth for where the two variable-length tails
// live. The console spells the same thing with its own extents (declick at 0x188, then
// align_up(0x188 + 4*channels, 8), then + 0x20 per request); BOTH of those are console
// sizes containing 32-bit pointers, so the host recomputes all three from its own sizeofs.
// GetSize and CreateInstance share this so the allocation and the placement cannot drift.
// -------------------------------------------------------------------------------------
SndPlayer1_CgsStreamMod::HostLayout
SndPlayer1_CgsStreamMod::ComputeHostLayout(u32 auChannels, s32 aiMaxRequests)
{
    HostLayout lLayout;
    // One declick float per channel, immediately past the fixed object.
    lLayout.declickOffset = AlignUp8(static_cast<u32>(sizeof(SndPlayer1_CgsStreamMod)));
    lLayout.requestOffset =
        AlignUp8(lLayout.declickOffset + static_cast<u32>(sizeof(f32)) * auChannels);
    lLayout.totalBytes = lLayout.requestOffset
        + static_cast<u32>(sizeof(RequestInternal)) * static_cast<u32>(aiMaxRequests);
    return lLayout;
}

// -------------------------------------------------------------------------------------
// GetSize @0x826A4210 -- the stage-carve stride. The constructor parameter's first float
// is the request-ring depth (default 1); the config's +8 byte is the channel count.
// -------------------------------------------------------------------------------------
int SndPlayer1_CgsStreamMod::GetSize(const VoiceStageConfig* config)
{
    const s32 liMaxRequests = config->mpContext
        ? PpcFctidzLowS32(*static_cast<const f32*>(config->mpContext))
        : 1;
    const u32 luChannels = static_cast<u8>(config->mFlagAndField8);
    return static_cast<int>(ComputeHostLayout(luChannels, liMaxRequests).totalBytes);
}

// -------------------------------------------------------------------------------------
// PreProcess @0x8268CD10 -- stow the requested count; a source stage pulls nothing from
// upstream. The 16-bit store is deliberate (the caller caps its request at 256).
// -------------------------------------------------------------------------------------
int SndPlayer1_CgsStreamMod::PreProcess(SndPlayer1_CgsStreamMod* self, Mixer* /*ctx*/,
                                        bool /*discontinuity*/, int outputSamplesRequested)
{
    self->mSamplesRequested = static_cast<u16>(outputSamplesRequested);
    return 0;
}

// -------------------------------------------------------------------------------------
// WaitForStartTime @0x8268CAF8 -- how much silence, if any, precedes a scheduled start.
//
// Three outcomes: the start has already passed (play now, no silence); it lands inside
// this frame (emit that many silent samples first); or it is still a full frame or more
// away (report false, and the caller reports the buffer unavailable).
// -------------------------------------------------------------------------------------
bool SndPlayer1_CgsStreamMod::WaitForStartTime(Mixer* ctx, f64 adStartTime, u32* apuSilence)
{
    const f64 ldDelta = adStartTime - ctx->mdStreamTime;
    if (ldDelta <= 0.0)
    {
        *apuSilence = 0;
        return true;
    }
    const f32 lfFrames = static_cast<f32>(ctx->mpFormat->mfSampleRate * ldDelta);
    if (!(lfFrames < 256.0f))
        return false;
    // The resample gain scales the frame count into this stage's own sample domain.
    *apuSilence = PpcFctidzLowU32(
        static_cast<f64>(ctx->mfResampleGain * lfFrames));
    return true;
}

// -------------------------------------------------------------------------------------
// Declick @0x8268CB78 -- ramp each channel from its captured last sample down to zero.
//
// Process captures the final sample of every channel it produced; when playback stops or a
// request is torn down, this fades that DC offset out over mNumDeclickSamples rather than
// letting the output step to silence. The ramp spans as many frames as it needs.
// -------------------------------------------------------------------------------------
int SndPlayer1_CgsStreamMod::Declick(Mixer* ctx)
{
    const u32 luEmit = (mNumDeclickSamples < mSamplesRequested)
        ? static_cast<u32>(mNumDeclickSamples)
        : static_cast<u32>(mSamplesRequested);

    for (u32 luChannel = 0; luChannel < mOutputChannels; ++luChannel)
    {
        f32& lrLast = DeclickBuffer()[luChannel];
        const f32 lfStep = lrLast / static_cast<f32>(mNumDeclickSamples);
        f32* lpDst = ctx->mpDstBuffer->mpSamples + ctx->mpDstBuffer->muStride * luChannel;
        for (u32 luSample = 0; luSample < luEmit; ++luSample)
            lpDst[luSample] = (lrLast -= lfStep);
    }

    mNumDeclickSamples = static_cast<u8>(mNumDeclickSamples - luEmit);

    SampleBuffer* lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    ctx->mNumSamples = luEmit;
    ctx->mfSampleRate = mPreviousSampleRate;
    ctx->mbChannelCount = mOutputChannels;

    if (!mNumDeclickSamples)
        mDcOffsetsGathered = 0;   // the fade is done; stop diverting Process into here
    return 1;                     // BUFFERSTATUS_AVAILABLE
}

// -------------------------------------------------------------------------------------
// GetFeedSlot @0x826A4348 -- claim the next feed-ring slot, if it is free.
// -------------------------------------------------------------------------------------
bool SndPlayer1_CgsStreamMod::GetFeedSlot(s32* apiSlot)
{
    if (mFeedDesc[mNextFeedSlotToFill].feedState)
        return false;   // the ring has caught up with itself
    *apiSlot = mNextFeedSlotToFill;
    mNextFeedSlotToFill = static_cast<u8>((mNextFeedSlotToFill + 1) % KU_MAX_DECODERFEEDS);
    return true;
}

// -------------------------------------------------------------------------------------
// UnpackHeader @0x8268C990 -- parse the packed sample header into the request pair.
//
// MSB-first bit layout: 4 version, 4 codec, 6 channels (STORED PLUS ONE), 18 sample rate,
// 2 play type, 1 loop flag, 29 sample count, then the optional 32-bit loop/resident words.
// The payload begins at the next whole byte past the packed header.
// -------------------------------------------------------------------------------------
void SndPlayer1_CgsStreamMod::UnpackHeader(u32 auRequestIndex, void* apPacked)
{
    BitGetter lBits;
    lBits.mpBitBuffer = static_cast<const u8*>(apPacked);
    lBits.mBitPosition = 0;

    RequestInternal& lrRequest = Request(auRequestIndex);
    RequestExternal& lrExternal = mpRequestExternal[auRequestIndex];

    (void)BitGetter::GetBits(&lBits, 4);                                   // version
    lrExternal.codec = static_cast<u8>(BitGetter::GetBits(&lBits, 4));
    lrRequest.numChannels = static_cast<u8>(BitGetter::GetBits(&lBits, 6) + 1);
    lrRequest.sampleRate = static_cast<f32>(BitGetter::GetBits(&lBits, 18));
    lrExternal.playType = static_cast<u8>(BitGetter::GetBits(&lBits, 2));
    const bool lbLoop = BitGetter::GetBits(&lBits, 1) != 0;
    lrRequest.numSamples = static_cast<s32>(BitGetter::GetBits(&lBits, 29));
    lrRequest.loopStart = lbLoop ? static_cast<s32>(BitGetter::GetBits(&lBits, 32)) : -1;

    if (lrExternal.playType == 2)
        lrExternal.gigaSamplesInRam = static_cast<s32>(BitGetter::GetBits(&lBits, 32));

    if (lbLoop)
    {
        // A loop point only carries a stream offset when the loop actually lives in the
        // STREAMED part of the asset (always for playType 1; for the hybrid playType 2
        // only when the loop starts past the resident prefix).
        if (lrExternal.playType == 1
            || (lrExternal.playType == 2
                && lrRequest.loopStart >= lrExternal.gigaSamplesInRam))
        {
            lrExternal.loopStartStreamOffset =
                static_cast<s32>(BitGetter::GetBits(&lBits, 32));
        }
        else
        {
            lrExternal.loopStartStreamOffset = 0;
        }
    }

    lrExternal.pSampleData = static_cast<u8*>(apPacked) + (lBits.mBitPosition >> 3);
}

}}}

namespace rw { namespace audio { namespace core {

int SndPlayer1_CgsStreamMod::CreateInstance(PlugIn* apBase, void* apContext)
{
    SndPlayer1_CgsStreamMod* self =
        ::new (static_cast<void*>(apBase)) SndPlayer1_CgsStreamMod;
    const s32 count = apContext
        ? PpcFctidzLowS32(*static_cast<const f32*>(apContext))
        : 1;
    CGS_ASSERT(count > 0 && count <= 255, "JStr request count");

    self->mpAttribute = self->mAttribute;
    const HostLayout layout = ComputeHostLayout(self->mOutputChannels, count);
    self->mDeclickBufferOffset = CheckedU16(layout.declickOffset);
    self->mRequestInternalOffset = CheckedU16(layout.requestOffset);
    self->mTimerAdded = 0;

    self->mpRequestExternal = static_cast<RequestExternal*>(
        System::Alloc(self->mpSystemUseGetSystemAccessor,
                      static_cast<u32>(count) * sizeof(RequestExternal),
                      "SndPlayer1_CgsStreamMod RequestExternal array", 16, 0));
    if (!self->mpRequestExternal)
        return 0;

    self->mMaxRequests = static_cast<u8>(count);
    for (s32 i = 0; i < count; ++i)
    {
        self->Request(i).state = E_REQUESTSTATE_FREE;
        RequestExternal& external = self->mpRequestExternal[i];
        external.pReadStream = 0;
        external.streamState = E_STREAMSTATE_READ_HEADER;
        external.readBufferSelect = 0;
        external.writeBufferSelect = 0;
        external.unlockBufferSelect = 0;
        external.readSize = KU_NEWSIZE_BUFFER_SIZE;
        external.readPointer = 0;
        external.queuedChunks = 0;
        external.lockedChunks = 0;
        self->muDataReadForNewSize = 0;
        for (u32 j = 0; j < KU_NUM_CHUNKS; ++j)
        {
            external.chunks[j].size = KU_DEFAULT_CHUNK_BYTES;
            external.chunks[j].buf = static_cast<u8*>(
                System::Alloc(self->mpSystemUseGetSystemAccessor,
                              KU_DEFAULT_CHUNK_BYTES,
                              "SndPlayer1_CgsStreamMod Chunk", 16, 0));
            CGS_ASSERT(external.chunks[j].buf != 0, "JStr chunk allocation");
        }
    }

    self->mCurrentRequest = 0;
    self->mNextRequestToFree = 0;
    self->mNextFreeRequest = 0;
    self->mCurrentRequestSamplesPlayed = 0;
    self->mCurrentRequestNumSamples = 0;
    self->mMaxChannels = self->mOutputChannels;
    self->mAttribute[0].mfValue = 0.0f;
    self->mRequestHandle = 0.0f;
    self->mLastRequestHandleProcessed = 0.0f;
    self->mLastRequestHandleSuccessfullyProcessed = 0.0f;
    self->mCurrentRequestHandle = 0.0f;
    StoreAttributeDouble(self->mAttribute[1], 0.0);
    StoreAttributeDouble(self->mAttribute[2], 0.0);
    self->mDcOffsetsGathered = 0;
    self->mNumDeclickSamples = 0;
    self->mNextFeedSlotToFill = 0;
    self->mNextFeedSlotToFree = 0;
    self->mCurrentRequestSampleRate = 48000.0f;
    self->mPreviousSampleRate = 48000.0f;
    for (u32 i = 0; i < KU_MAX_DECODERFEEDS; ++i)
    {
        self->mFeedDesc[i].feedState = 0;
        self->mFeedDesc[i].mbStreamed = false;
    }

    if (TimerManager::AddTimer(
            &self->mpSystemUseGetSystemAccessor->mTimerManager,
            &self->mTimerHandle, &RwacTimerClient, self,
            "SndPlayer", 1, 1) != 0)
        return 0;
    self->mTimerAdded = 1;
    return 1;
}

SndPlayer1_CgsStreamMod::~SndPlayer1_CgsStreamMod()
{
    StreamLostCallback(this);
    if (mTimerAdded == 1)
        System::RemoveTimer(mpSystemUseGetSystemAccessor, &mTimerHandle);
    if (mpRequestExternal)
    {
        for (u32 i = 0; i < mMaxRequests; ++i)
        {
            for (u32 j = 0; j < KU_NUM_CHUNKS; ++j)
            {
                if (mpRequestExternal[i].chunks[j].buf)
                    System::Free(mpSystemUseGetSystemAccessor,
                                 mpRequestExternal[i].chunks[j].buf, 0);
            }
        }
        System::Free(mpSystemUseGetSystemAccessor, mpRequestExternal, 0);
    }
}

int SndPlayer1_CgsStreamMod::VFunc2()
{
    return GetPpuTicksEvent();
}

void SndPlayer1_CgsStreamMod::Destroy(int /*aiFlags*/)
{
}

int SndPlayer1_CgsStreamMod::Event(int aiEventId, void* apParameter)
{
    System* system = mpSystemUseGetSystemAccessor;
    switch (aiEventId)
    {
    case 0:
    {
        PlayParams& params = *static_cast<PlayParams*>(apParameter);
        f32 next = mRequestHandle + 1.0f;
        if (!(next <= 4194304.0f))
            next = 1.0f;
        mRequestHandle = next;
        params.requestHandle = next;

        const u32 pathBytes = params.pStreamFilePath
            ? static_cast<u32>(std::strlen(spPathPrefix) +
                               std::strlen(params.pStreamFilePath) + 1)
            : 1u;
        const u32 bytes = AlignUp4(
            static_cast<u32>(sizeof(JStrPlayCommand)) + pathBytes);
        JStrPlayCommand* command =
            ReserveDeferred<JStrPlayCommand>(system, bytes);
        command->base.handler = &PlayHandler;
        command->base.self = this;
        command->startTime = params.startTime;
        command->streamFileOffset = params.streamFileOffset;
        command->streamPoolGuid = params.streamPoolGuid;
        command->pRamData = params.pRamData;
        command->recordBytes = CheckedU16(bytes);
        command->expelMode =
            static_cast<u8>(PpcFctidzLowU32(params.expelMode));
        command->requestHandle = next;
        char* path = reinterpret_cast<char*>(command) + sizeof(*command);
        if (!params.pStreamFilePath)
            path[0] = 0;
        else
        {
            std::strcpy(path, spPathPrefix);
            std::strcat(path, params.pStreamFilePath);
        }
        break;
    }
    case 1:
    {
        JStrStopCommand* command =
            ReserveDeferred<JStrStopCommand>(system, sizeof(JStrStopCommand));
        command->base.handler = &StopHandler;
        command->base.self = this;
        break;
    }
    case 2:
    {
        IsRequestDoneParams& params =
            *static_cast<IsRequestDoneParams*>(apParameter);
        const f32 requestHandle = params.requestHandle;
        params.isRequestDone = 0.0f;
        const bool reachesDurationTest =
            (requestHandle == mAttribute[0].mfValue) ||
            (!(requestHandle > mLastRequestHandleProcessed) &&
             !(requestHandle <= mLastRequestHandleSuccessfullyProcessed));
        if ((requestHandle < mAttribute[0].mfValue) ||
            (reachesDurationTest && LoadAttributeDouble(mAttribute[2]) == 0.0))
            params.isRequestDone = 1.0f;
        break;
    }
    case 3:
    {
        GetRequestBufferedParams& params =
            *static_cast<GetRequestBufferedParams*>(apParameter);
        params.streamBytesBuffered = 0.0f;
        params.isFullyBuffered = 0.0f;
        for (u32 i = 0; i < mMaxRequests; ++i)
        {
            RequestInternal& request = Request(i);
            if (request.requestHandle != params.requestHandle ||
                !IsRequestActive(request.state))
                continue;
            RequestExternal& external = mpRequestExternal[i];
            if (external.playType == 1 || external.playType == 2)
            {
                params.streamBytesBuffered =
                    static_cast<f32>(external.numBytesFed);
                return 0;
            }
            if (external.playType == 0)
            {
                params.isFullyBuffered = 1.0f;
                return 0;
            }
        }
        break;
    }
    case 4:
    {
        ModifyStartTimeParams& params =
            *static_cast<ModifyStartTimeParams*>(apParameter);
        JStrModifyStartTimeCommand* command =
            ReserveDeferred<JStrModifyStartTimeCommand>(
                system, sizeof(JStrModifyStartTimeCommand));
        command->base.handler = &ModifyStartTimeHandler;
        command->base.self = this;
        command->startTime = params.newStartTime;
        command->requestHandle = params.requestHandle;
        break;
    }
    default:
        break;
    }
    return 0;
}

s32 SndPlayer1_CgsStreamMod::PlayHandler(void* apCommand)
{
    JStrPlayCommand* command = static_cast<JStrPlayCommand*>(apCommand);
    SndPlayer1_CgsStreamMod* self = command->base.self;
    const u32 index = self->mNextFreeRequest;
    self->mLastRequestHandleProcessed = command->requestHandle;
    RequestInternal& request = self->Request(index);
    if (request.state != E_REQUESTSTATE_FREE)
        return command->recordBytes;

    RequestExternal& external = self->mpRequestExternal[index];
    request.requestHandle = command->requestHandle;
    request.pDecoder = 0;
    request.startTime = command->startTime;
    external.streamFileOffset = command->streamFileOffset;
    external.expelMode = command->expelMode;
    request.state = E_REQUESTSTATE_QUEUED;
    external.numSamplesFed = 0;
    external.numBytesFed = 0;
    external.gigaSamplesInRam = 0;
    external.pStreamLoopFileName = 0;
    self->mLastRequestHandleSuccessfullyProcessed = command->requestHandle;
    self->UnpackHeader(index, const_cast<void*>(command->pRamData));
    if (external.playType == 1 || external.playType == 2)
    {
        CgsSound::Playback::IStreamProvider::StreamSpec spec;
        spec.mpFilename =
            reinterpret_cast<const char*>(command) + sizeof(JStrPlayCommand);
        spec.mppvBuffer = reinterpret_cast<void**>(&external.pStreamBuffer);
        spec.mpPlugin = self;
        spec.mi32PriorityLow = 0;
        spec.mi32PriorityHigh = 50;
        external.pReadStream = off_82FFBA0C->DoOpenStream(spec);
        if (!external.pReadStream)
        {
            request.state = E_REQUESTSTATE_FREE;
            return command->recordBytes;
        }
        if (request.loopStart >= 0)
        {
            const u32 bytes = static_cast<u32>(std::strlen(spec.mpFilename) + 1);
            external.pStreamLoopFileName = static_cast<char*>(
                System::Alloc(self->mpSystemUseGetSystemAccessor, bytes,
                              "SndPlayer1_CgsStreamMod StreamLoopFileName",
                              16, 0));
            if (!external.pStreamLoopFileName)
            {
                request.numSamples = 0;
                request.state = E_REQUESTSTATE_FREE;
                return command->recordBytes;
            }
            std::memcpy(external.pStreamLoopFileName, spec.mpFilename, bytes);
        }
    }

    request.state = E_REQUESTSTATE_QUEUED;
    self->mNextFreeRequest =
        AdvanceRequestEquality(self->mNextFreeRequest, self->mMaxRequests);
    return command->recordBytes;
}

s32 SndPlayer1_CgsStreamMod::ModifyStartTimeHandler(void* apCommand)
{
    JStrModifyStartTimeCommand* command =
        static_cast<JStrModifyStartTimeCommand*>(apCommand);
    SndPlayer1_CgsStreamMod* self = command->base.self;
    for (u32 i = 0; i < self->mMaxRequests; ++i)
    {
        RequestInternal& request = self->Request(i);
        if (request.requestHandle == command->requestHandle &&
            IsRequestActive(request.state))
        {
            if (!(request.startTime <=
                  self->mpSystemUseGetSystemAccessor->mfSystemTime))
                request.startTime = command->startTime;
            break;
        }
    }
    return sizeof(JStrModifyStartTimeCommand);
}

void SndPlayer1_CgsStreamMod::RemoveRequest(u32 auIndex)
{
    RequestInternal& request = Request(auIndex);
    RequestExternal& external = mpRequestExternal[auIndex];
    if (request.pDecoder)
    {
        request.pDecoder->Release();
        request.pDecoder = 0;
    }
    for (u32 i = 0; i < KU_MAX_DECODERFEEDS; ++i)
    {
        if (mFeedDesc[i].requestIndex == auIndex)
            mFeedDesc[i].feedState = 0;
    }
    off_82FFBA0C->DoCloseStream(external.pReadStream);
    if (external.pStreamLoopFileName)
        System::Free(mpSystemUseGetSystemAccessor,
                     external.pStreamLoopFileName, 0);
    request.state = E_REQUESTSTATE_FREE;
    if (external.expelMode == 1)
        Voice::ExpelAfterDecay(mpVoice);
}

void SndPlayer1_CgsStreamMod::RequestCleanup()
{
    while (Request(mNextRequestToFree).state == E_REQUESTSTATE_COMPLETE)
    {
        RemoveRequest(mNextRequestToFree);
        mNextRequestToFree =
            AdvanceRequestEquality(mNextRequestToFree, mMaxRequests);
    }
}

void SndPlayer1_CgsStreamMod::StreamLostCallback(void* apContext)
{
    SndPlayer1_CgsStreamMod* self =
        static_cast<SndPlayer1_CgsStreamMod*>(apContext);
    for (u32 i = 0; i < self->mMaxRequests; ++i)
    {
        if (self->Request(i).state != E_REQUESTSTATE_FREE)
            self->RemoveRequest(i);
    }
    self->mCurrentRequest = 0;
    self->mNextFreeRequest = 0;
    self->mNextRequestToFree = 0;
}

s32 SndPlayer1_CgsStreamMod::StopHandler(void* apCommand)
{
    SndPlayer1_CgsStreamMod* self =
        static_cast<JStrStopCommand*>(apCommand)->base.self;
    for (u32 i = 0; i < self->mMaxRequests; ++i)
    {
        if (self->Request(i).state != E_REQUESTSTATE_FREE)
            self->RemoveRequest(i);
    }
    self->mCurrentRequest = 0;
    self->mNextFreeRequest = 0;
    self->mNextRequestToFree = 0;
    self->mCurrentRequestSamplesPlayed = 0;
    self->mCurrentRequestNumSamples = 0;
    self->mNextFeedSlotToFill = 0;
    self->mNextFeedSlotToFree = 0;
    self->mNumDeclickSamples = 16;
    return sizeof(JStrStopCommand);
}

}}}

namespace rw { namespace audio { namespace core {

void SndPlayer1_CgsStreamMod::FeedCleanup()
{
    for (u32 i = 0; i < KU_MAX_DECODERFEEDS; ++i)
    {
        SndPlayer1FeedDesc& feed = mFeedDesc[i];
        if (feed.feedState != 2)
            continue;
        RequestInternal& request = Request(feed.requestIndex);
        if (request.pDecoder->GetSamplesRemaining(feed.decoderRequestHandle) != 0)
            continue;
        feed.feedState = 0;
        if (feed.mbStreamed)
        {
            RequestExternal& external = mpRequestExternal[feed.requestIndex];
            if (external.lockedChunks != 0)
            {
                u32 next = external.unlockBufferSelect + 1;
                if (next == KU_NUM_CHUNKS)
                    next = 0;
                if (next != external.writeBufferSelect)
                {
                    external.unlockBufferSelect = next;
                    --external.lockedChunks;
                }
            }
            feed.mbStreamed = false;
        }
    }
}

u8* SndPlayer1_CgsStreamMod::SubmitChunk(
    u8* apChunk, u32 auRequestIndex, bool abResetDecoder)
{
    const u32 nextOffset = ReadBe32(apChunk);
    const u32 samples = ReadBe32(apChunk + 4);
    RequestExternal& external = mpRequestExternal[auRequestIndex];
    SndPlayer1FeedDesc& feed = mFeedDesc[external.latestFeedSlot];
    feed.feedState = 1;
    feed.chunkSamplesPlayed = 0;
    feed.requestIndex = static_cast<u8>(auRequestIndex);
    feed.decoderRequestHandle = Request(auRequestIndex).pDecoder->Feed(
        apChunk + 8, static_cast<s32>(samples),
        static_cast<u8>(!abResetDecoder), 0,
        static_cast<const u8*>(0), 0);
    external.numSamplesFed += static_cast<s32>(samples);
    return apChunk + nextOffset;
}

bool SndPlayer1_CgsStreamMod::StreamNextChunk(
    u32 auIndex, bool abResetDecoder)
{
    RequestInternal& request = Request(auIndex);
    RequestExternal& external = mpRequestExternal[auIndex];
    if (request.state == E_REQUESTSTATE_QUEUED &&
        (!external.pReadStream || !external.pReadStream->IsValid()))
        return false;
    if (external.queuedChunks == 0 ||
        mFeedDesc[mNextFeedSlotToFill].feedState != 0)
        return false;
    s32 slot;
    if (!GetFeedSlot(&slot))
        return false;
    external.latestFeedSlot = static_cast<u8>(slot);
    external.numBytesFed += static_cast<s32>(external.readSize);
    mFeedDesc[slot].mbStreamed = true;
    SubmitChunk(external.chunks[external.readBufferSelect].buf,
                auIndex, abResetDecoder);
    if (++external.readBufferSelect == KU_NUM_CHUNKS)
        external.readBufferSelect = 0;
    --external.queuedChunks;
    return true;
}

bool SndPlayer1_CgsStreamMod::HandleLoopStart(u32 auIndex)
{
    RequestInternal& request = Request(auIndex);
    RequestExternal& external = mpRequestExternal[auIndex];
    if (external.playType == 1)
        return StreamNextChunk(auIndex, true);
    if (external.playType == 2 &&
        request.loopStart >= external.gigaSamplesInRam)
        return StreamNextChunk(auIndex, true);
    external.pLoopStartChunk = external.pNextChunk;
    s32 slot;
    GetFeedSlot(&slot);
    external.latestFeedSlot = static_cast<u8>(slot);
    external.pNextChunk =
        SubmitChunk(external.pNextChunk, auIndex, true);
    return true;
}

bool SndPlayer1_CgsStreamMod::HandleSampleEnd(
    u32 auIndex, bool* apbCompleted)
{
    RequestInternal& request = Request(auIndex);
    RequestExternal& external = mpRequestExternal[auIndex];
    if (request.loopStart < 0)
    {
        *apbCompleted = true;
        return true;
    }
    *apbCompleted = false;

    if (external.playType == 0)
    {
        if (request.loopStart == 0)
            external.pLoopStartChunk = external.pSampleData;
        s32 slot;
        GetFeedSlot(&slot);
        external.latestFeedSlot = static_cast<u8>(slot);
        external.numSamplesFed = request.loopStart;
        external.pNextChunk =
            SubmitChunk(external.pLoopStartChunk, auIndex, true);
        return true;
    }

    if (external.playType == 1)
    {
        const u64 seekTo = PpcFctidzBits(external.streamFileOffset) +
            static_cast<u64>(static_cast<s64>(external.loopStartStreamOffset));
        external.pReadStream->Seek(seekTo);
        external.numSamplesFed = request.loopStart;
        return StreamNextChunk(auIndex, true);
    }

    external.numSamplesFed = request.loopStart;
    if (request.loopStart < external.gigaSamplesInRam)
    {
        if (request.loopStart == 0)
            external.pLoopStartChunk = external.pSampleData;
        s32 slot;
        GetFeedSlot(&slot);
        external.latestFeedSlot = static_cast<u8>(slot);
        external.pNextChunk =
            SubmitChunk(external.pLoopStartChunk, auIndex, true);
    }
    if (external.gigaSamplesInRam < request.numSamples)
    {
        const u64 seekTo = PpcFctidzBits(external.streamFileOffset) +
            static_cast<u64>(static_cast<s64>(external.loopStartStreamOffset));
        external.pReadStream->Seek(seekTo);
        if (request.loopStart >= external.gigaSamplesInRam &&
            !StreamNextChunk(auIndex, true))
            return false;
    }
    return true;
}

bool SndPlayer1_CgsStreamMod::StartRequest(u32 auIndex)
{
    static const u32 codecGuids[6] = {
        0x58617330u, 0x454C3330u, 0x50364230u,
        0x45586D30u, 0x58617331u, 0x454C3331u
    };

    System* system = mpSystemUseGetSystemAccessor;
    RequestInternal& request = Request(auIndex);
    RequestExternal& external = mpRequestExternal[auIndex];
    System::Lock(system);
    DecoderRegistry* registry = System::GetDecoderRegistry(system);
    DecoderDesc* handle =
        DecoderRegistry::GetDecoderHandle(registry, codecGuids[external.codec]);
    request.pDecoder = DecoderRegistry::DecoderFactory(
        registry, handle, request.numChannels, KU_MAX_DECODERFEEDS, system);
    if (!request.pDecoder)
    {
        System::Unlock(system);
        return false;
    }

    const u32 instanceBytes = request.pDecoder->GetInstanceSize();
    CGS_ASSERT(instanceBytes <= 0xFFFFu, "JStr decoder instance size");
    request.decoderInstanceSize = static_cast<u16>(instanceBytes);

    bool ok = true;
    if (external.playType == 0 || external.playType == 2)
    {
        s32 slot;
        GetFeedSlot(&slot);
        external.latestFeedSlot = static_cast<u8>(slot);
        external.pNextChunk =
            SubmitChunk(external.pSampleData, auIndex, true);
    }
    else
    {
        ok = StreamNextChunk(auIndex, true);
    }
    if (!ok)
    {
        request.pDecoder->Release();
        request.pDecoder = 0;
    }
    System::Unlock(system);
    return ok;
}

int SndPlayer1_CgsStreamMod::Process(
    SndPlayer1_CgsStreamMod* self, Mixer* mixer, bool /*abDiscontinuity*/)
{
    if (self->mNumDeclickSamples && self->mDcOffsetsGathered)
        return self->Declick(mixer);

    RequestInternal* request = &self->Request(self->mCurrentRequest);
    self->mpLoadedDecoder = 0;
    while (IsRequestActive(request->state) && request->numSamples == 0)
    {
        request->state = E_REQUESTSTATE_COMPLETE;
        self->AdvanceCurrentRequest();
        request = &self->Request(self->mCurrentRequest);
    }

    u32 produced = 0;
    u32 remaining = 0;
    u8* savedTop = 0;
    bool carved = false;
    StackAllocator* stack = static_cast<StackAllocator*>(
        self->mpSystemUseGetSystemAccessor->mpObjectTable);

    if (!IsRequestActive(request->state))
        goto finish;
    if (request->sampleRate != self->mPreviousSampleRate ||
        request->numChannels != self->mOutputChannels)
    {
        mixer->mNumSamples = 0;
        mixer->mbChannelCount = request->numChannels;
        mixer->mfSampleRate = request->sampleRate;
        self->mPreviousSampleRate = request->sampleRate;
        self->mOutputChannels = request->numChannels;
        return 1;
    }

    while (self->mFeedDesc[self->mNextFeedSlotToFree].feedState == 0 &&
           self->mNextFeedSlotToFree != self->mNextFeedSlotToFill)
    {
        if (++self->mNextFeedSlotToFree == KU_MAX_DECODERFEEDS)
            self->mNextFeedSlotToFree = 0;
    }
    if (self->mFeedDesc[self->mNextFeedSlotToFree].feedState != 1)
        goto finish;

    if (request->startTime != 0.0)
    {
        u32 silence = 0;
        if (!self->WaitForStartTime(mixer, request->startTime, &silence))
        {
            self->mCurrentRequestSamplesPlayed = 0;
            goto finish;
        }
        if (silence)
        {
            if (silence > self->mSamplesRequested)
                silence = self->mSamplesRequested;
            for (u32 channel = 0; channel < request->numChannels; ++channel)
            {
                std::memset(
                    mixer->mpDstBuffer->mpSamples +
                        channel * mixer->mpDstBuffer->muStride,
                    0, silence * sizeof(f32));
            }
            mixer->mNumSamples = silence;
            SampleBuffer* temp = mixer->mpSrcBuffer;
            mixer->mpSrcBuffer = mixer->mpDstBuffer;
            mixer->mpDstBuffer = temp;
            mixer->mbChannelCount = request->numChannels;
            mixer->mfSampleRate = request->sampleRate;
            self->mCurrentRequestSamplesPlayed = 0;
            return 1;
        }
        request->startTime = 0.0;
    }

    savedTop = stack->mpTop;
    stack->mpTop -= AlignUp128(request->decoderInstanceSize);
    carved = true;
    self->mpLoadedDecoder = request->pDecoder;
    {
        SndPlayer1FeedDesc& feed =
            self->mFeedDesc[self->mNextFeedSlotToFree];
        const u32 available = static_cast<u32>(
            request->pDecoder->GetSamplesRemaining(feed.decoderRequestHandle));
        const u32 ask = MinU32(self->mSamplesRequested, available);
        produced = static_cast<u32>(request->pDecoder->Decode(
            reinterpret_cast<DecoderBuffer*>(mixer->mpDstBuffer),
            static_cast<s32>(ask)));
        remaining = available - produced;
        SampleBuffer* temp = mixer->mpSrcBuffer;
        mixer->mpSrcBuffer = mixer->mpDstBuffer;
        mixer->mpDstBuffer = temp;
        mixer->mNumSamples = produced;
        mixer->mbChannelCount = request->numChannels;
        mixer->mfSampleRate = request->sampleRate;
        self->mCurrentRequestHandle = request->requestHandle;
        self->mCurrentRequestSampleRate = request->sampleRate;
        self->mCurrentRequestSamplesPlayed += produced;
        self->mCurrentRequestNumSamples = request->numSamples;
        feed.chunkSamplesPlayed += produced;
    }

    if (self->mCurrentRequestSamplesPlayed == request->numSamples)
    {
        if (request->loopStart >= 0)
        {
            self->mCurrentRequestSamplesPlayed = request->loopStart;
        }
        else
        {
            request->state = E_REQUESTSTATE_COMPLETE;
            self->mpLoadedDecoder = 0;
            stack->mpTop = savedTop;
            carved = false;
            self->AdvanceCurrentRequest();
            request = &self->Request(self->mCurrentRequest);
            if (IsRequestActive(request->state) && request->pDecoder)
            {
                savedTop = stack->mpTop;
                stack->mpTop -= AlignUp128(request->decoderInstanceSize);
                carved = true;
                self->mpLoadedDecoder = request->pDecoder;
            }
        }
    }

    while (remaining == 0 &&
           self->mFeedDesc[self->mNextFeedSlotToFree].feedState == 1)
    {
        self->mFeedDesc[self->mNextFeedSlotToFree].feedState = 2;
        if (++self->mNextFeedSlotToFree == KU_MAX_DECODERFEEDS)
            self->mNextFeedSlotToFree = 0;
        SndPlayer1FeedDesc& next =
            self->mFeedDesc[self->mNextFeedSlotToFree];
        if (self->mpLoadedDecoder && next.feedState == 1)
        {
            remaining = static_cast<u32>(
                self->mpLoadedDecoder->GetSamplesRemaining(
                    next.decoderRequestHandle));
        }
    }

finish:
    if (carved)
        stack->mpTop = savedTop;
    self->mpLoadedDecoder = 0;
    mixer->mbChannelCount = self->mOutputChannels;
    mixer->mfSampleRate = self->mPreviousSampleRate;
    if (!produced)
    {
        if (self->mSamplesRequested)
            return 0;
        mixer->mNumSamples = 0;
        return 1;
    }
    const u32 channels = MinU32(mixer->mbChannelCount, self->mMaxChannels);
    for (u32 channel = 0; channel < channels; ++channel)
    {
        self->DeclickBuffer()[channel] =
            mixer->mpSrcBuffer->mpSamples[
                channel * mixer->mpSrcBuffer->muStride + produced - 1];
    }
    self->mDcOffsetsGathered = 1;
    return 1;
}

}}}

namespace rw { namespace audio { namespace core {

void SndPlayer1_CgsStreamMod::RwacTimerClient(
    void* apContext, f32 /*afTimeToNextCall*/)
{
    SndPlayer1_CgsStreamMod* self =
        static_cast<SndPlayer1_CgsStreamMod*>(apContext);
    if (self->mpVoice->mucState == 2)
        return;

    self->FeedCleanup();
    self->RequestCleanup();
    u8 index = self->mCurrentRequest;
    RequestInternal* request = &self->Request(index);
    if (!IsRequestActive(request->state))
    {
        StoreAttributeDouble(self->mAttribute[1], 0.0);
        StoreAttributeDouble(self->mAttribute[2], 0.0);
        return;
    }

    self->mAttribute[0].mfValue = self->mCurrentRequestHandle;
    StoreAttributeDouble(
        self->mAttribute[2],
        static_cast<f64>(self->mCurrentRequestNumSamples) /
            self->mCurrentRequestSampleRate);
    StoreAttributeDouble(
        self->mAttribute[1],
        static_cast<f64>(self->mCurrentRequestSamplesPlayed) /
            self->mCurrentRequestSampleRate);

    while (IsRequestActive(request->state) && request->numSamples == 0)
    {
        index = AdvanceRequestEquality(index, self->mMaxRequests);
        request = &self->Request(index);
    }
    if (!IsRequestActive(request->state))
        return;

    RequestExternal* external = &self->mpRequestExternal[index];
    CGS_ASSERT(external->pReadStream != 0,
               "pRequestExternal->mpReadStream");
    for (u32 budget = 0;
         budget < KU_NUM_CHUNKS &&
         external->pReadStream->IsValid() &&
         external->lockedChunks < KU_NUM_CHUNKS;
         ++budget)
    {
        if (external->streamState == E_STREAMSTATE_SUBMIT_CHUNK)
        {
            u32 next = external->writeBufferSelect + 1;
            if (next == KU_NUM_CHUNKS)
                next = 0;
            if (next != external->unlockBufferSelect)
            {
                self->muDataReadForNewSize = 0;
                external->streamState = E_STREAMSTATE_READ_HEADER;
                external->writeBufferSelect = next;
                ++external->queuedChunks;
                ++external->lockedChunks;
            }
            continue;
        }

        if (external->streamState == E_STREAMSTATE_READ_CHUNK)
        {
            Chunk& chunk = external->chunks[external->writeBufferSelect];
            CGS_ASSERT(external->readPointer + external->readSize <= chunk.size,
                       "JStr chunk read bounds");
            const u32 got = external->pReadStream->Read(
                external->readSize, chunk.buf + external->readPointer);
            if (got == 0)
                continue;
            CGS_ASSERT(got <= external->readSize, "JStr read result");
            external->readSize -= got;
            external->readPointer += got;
            if (external->readSize == 0)
                external->streamState = E_STREAMSTATE_SUBMIT_CHUNK;
            continue;
        }

        if (external->streamState >= 3)
            continue;

        const u32 got = external->pReadStream->Read(
            KU_NEWSIZE_BUFFER_SIZE - self->muDataReadForNewSize,
            self->mau8NewSize + self->muDataReadForNewSize);
        self->muDataReadForNewSize += got;
        if (self->muDataReadForNewSize != KU_NEWSIZE_BUFFER_SIZE)
            continue;

        const u32 size = ReadBe32(self->mau8NewSize) & 0x7FFFFFFFu;
        Chunk& chunk = external->chunks[external->writeBufferSelect];
        if (size > chunk.size)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "**** STREAM WARNING: reallocating Chunk from "
                    << chunk.size << " to " << size << " ****\n";
            }
            System::Free(self->mpSystemUseGetSystemAccessor, chunk.buf, 0);
            chunk.buf = static_cast<u8*>(
                System::Alloc(self->mpSystemUseGetSystemAccessor, size,
                              "SndPlayer1_CgsStreamMod Chunk", 16, 0));
            CGS_ASSERT(chunk.buf != 0, "JStr resized chunk");
            chunk.size = size;
        }
        WriteBe32(chunk.buf, size);
        external->readSize = size - 4;
        external->readPointer = 4;
        external->streamState = E_STREAMSTATE_READ_CHUNK;
    }

    for (;;)
    {
        if (!IsRequestActive(request->state) ||
            self->mFeedDesc[self->mNextFeedSlotToFill].feedState != 0)
            return;
        external = &self->mpRequestExternal[index];
        if (request->state == E_REQUESTSTATE_QUEUED)
        {
            if (!self->StartRequest(index))
                return;
            request->state = E_REQUESTSTATE_FEEDING;
            if (external->codec == 3 && request->startTime == 0.0 &&
                index == self->mCurrentRequest)
            {
                request->startTime =
                    self->mpSystemUseGetSystemAccessor->mfSystemTime +
                    0.005333333333333333;
            }
        }
        if (request->state != E_REQUESTSTATE_FEEDING ||
            self->mFeedDesc[self->mNextFeedSlotToFill].feedState != 0)
        {
            index = AdvanceRequestEquality(index, self->mMaxRequests);
            if (index == self->mCurrentRequest)
                return;
            request = &self->Request(index);
            continue;
        }

        bool ok;
        if (external->numSamplesFed == request->loopStart)
        {
            ok = self->HandleLoopStart(index);
        }
        else if (external->numSamplesFed == request->numSamples)
        {
            bool complete = false;
            ok = self->HandleSampleEnd(index, &complete);
            if (ok && complete)
            {
                request->state = E_REQUESTSTATE_FEEDCOMPLETE;
                index = AdvanceRequestEquality(index, self->mMaxRequests);
                if (index == self->mCurrentRequest)
                    return;
                request = &self->Request(index);
                continue;
            }
        }
        else
        {
            ok = self->StreamNextChunk(index, false);
        }
        if (!ok)
            return;
    }
}

static PlugInDescRunTime g_JStrDesc = {
    "SndPlayer1_CgsStreamMod",
    reinterpret_cast<void*>(&SndPlayer1_CgsStreamMod::GetSize),
    reinterpret_cast<void*>(&SndPlayer1_CgsStreamMod::CreateInstance),
    reinterpret_cast<void*>(&SndPlayer1_CgsStreamMod::PreProcess),
    reinterpret_cast<void*>(&SndPlayer1_CgsStreamMod::Process),
    0, 0, 0, 0,
    0,
    0x4A537472u,
    0, 1, 3, 5, 0, 1,
    0
};

char** SndPlayer1_CgsStreamMod::GetPlugInDescRunTime()
{
    return reinterpret_cast<char**>(&g_JStrDesc);
}

}}}
