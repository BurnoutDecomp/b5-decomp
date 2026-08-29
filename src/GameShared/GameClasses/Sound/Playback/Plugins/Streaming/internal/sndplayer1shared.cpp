#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h"

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
// PHASE E (2026-08-28) -- the self-contained half of the plug-in.
//
// These are the bodies that need nothing beyond the already-committed engine surface. The
// rest (CreateInstance, Process, the streaming timer client, and the decoder/stream
// helpers) depend on the Decoder request records and the CgsFileSystem ReadStream surface
// and land with the SndPlayer1 wave. The descriptor is deliberately NOT registered until
// every slot it would publish is real -- a record with a missing callback is the
// poison-in-waiting the registration site has warned about since the descriptor wave.
//
// Decode report: progress/scratch_dossiers/streammod_gainarray_decode_codex.md (TARGET 1).
// =====================================================================================

#include "rw/audio/core/Mixer.h"     // Mixer (the process context) + SampleBuffer
#include "rw/audio/core/BitGetter.h" // the packed-header bit reader

namespace rw { namespace audio { namespace core {

namespace
{
    inline u32 AlignUp8(u32 auValue) { return (auValue + 7u) & ~7u; }
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
        ? static_cast<s32>(*static_cast<const f32*>(config->mpContext))
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
    if (lfFrames >= 256.0f)
        return false;
    // The resample gain scales the frame count into this stage's own sample domain.
    *apuSilence = static_cast<u32>(ctx->mfResampleGain * lfFrames);
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
    RequestExternal& lrExternal =
        static_cast<RequestExternal*>(mpRequestExternal)[auRequestIndex];

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
