// =====================================================================================
// rw::audio::core::GainFader bodies -- the timed gain-fade plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch (decode report
// progress/scratch_dossiers/gainfader_lowpassbutterworth_decode_codex.md). The vendor
// gainfader.h supplies the authoritative names and matches the layout member-for-member.
//   GetSize          @0x82B97360 -- console 0x70; host sizeof (the stage carve)
//   CreateInstance   @0x82BA2C08 -- store-for-store
//   Process          @0x82B97378 -- branch-for-branch
//   EventEvent       @0x82BA2C50 -- vt[1]; EVENT_STARTFADE only
//   StartFadeHandler @0x82B9DF18 -- the deferred replay
// See plugins/GainFader.h for the byte-exact layout.
//
// The three fade CURVES are the already-committed shared kernels in MixKernels
// (GainVectorLinearAmplitude @0x82B69938 / GainVectorLinearPower @0x82B69AB0 /
// GainVectorSine @0x82B69C30) -- this plug-in is their only consumer on the committed
// surface, so wiring it up also lights those bodies for the first time.
// =====================================================================================

#include "rw/audio/core/plugins/GainFader.h"
#include "rw/audio/core/Mixer.h"      // the process context (mdStreamTime, src/dst, mpFormat)
#include "rw/audio/core/MixKernels.h" // the three GainVector* fade curves

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F3E4 -- the GainFader v-table (slots: ReleaseEvent / EventEvent /
// complete-dtor / vector-deleting-dtor). Installed by the compiler here; modelled as an
// honest placeholder only for the teardown reinstall convention the siblings use.
static void *const KGF_BasePlugInVTable = nullptr;   // off_820AA810

static const f32 KF_ONE = 1.0f;    // flt_82001C98
static const f32 KF_ZERO = 0.0f;   // flt_82001CC0
static const f64 KD_ZERO = 0.0;    // dbl_82001CA8

enum { KI_FRAME_SAMPLES = 256 };   // the console's hard-coded frame

namespace
{
    // The console's fctiwz: truncate toward zero. CONSOLE-SEMANTICS NOTE: fctiwz saturates
    // on NaN/out-of-range where a C++ cast is undefined; over this plug-in's real domain
    // (finite sample rates and fade times) the two agree exactly.
    inline s32 PpcFctiwz(f64 adValue) { return static_cast<s32>(adValue); }
}

// off_82F8CC50 -- the "GainFader" runtime descriptor, REAL (record dump in
// progress/scratch_dossiers/plugindesc_layout_codex.md, re-read byte-for-byte by the
// GainFader decode). Metadata FLAG'd null per the descriptor-wave convention.
static PlugInDescRunTime g_GainFaderDesc = {
    "GainFader",
    reinterpret_cast<void *>(&GainFader::GetSize),        // @0x82B97360
    reinterpret_cast<void *>(&GainFader::CreateInstance), // @0x82BA2C08
    0,
    reinterpret_cast<void *>(&GainFader::Process),        // @0x82B97378
    0, 0, 0, 0,
    0,
    0x47614630u,       // 'GaF0'
    4, 0, 1, 1, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97368 -- return &off_82F8CC50.
// -------------------------------------------------------------------------------------
char **GainFader::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_GainFaderDesc);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B97360 -- li r3, 0x70 ; blr
// -------------------------------------------------------------------------------------
int GainFader::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit): the console immediate under-allocates the
    // widened host object -- GetSize is the stage factory's allocation stride, so return
    // host sizeof (the RawPuller2/Send/Rechannel precedent).
    return static_cast<int>(sizeof(GainFader));   // X360: li r3, 0x70 (112)
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2C08 -- placement-init. Opens at unity gain, with no request
// outstanding and the fade already "finished" (so Process holds at mLastGain until an
// EVENT_STARTFADE arrives). The console's null test guards ONLY the vtable store; every
// following store dereferences self, so null is not a supported input.
// -------------------------------------------------------------------------------------
int GainFader::CreateInstance(GainFader *self)
{
    self->mLastGain = KF_ONE;                                     // stfs +0x64
    self->mpAttribute = &self->mAttribute[0];                     // stw  +0x0C
    self->mAttribute[ATTRIBUTE_GETCURRENTGAIN].mfValue = KF_ONE;  // stfs +0x28
    self->mUnservicedRequest = 0;                                 // stb  +0x68
    self->mFadeState = FADESTATE_FINISHED;                        // stb  +0x69
    return 1;
}

// -------------------------------------------------------------------------------------
// EventEvent @0x82BA2C50 (vt[1]) -- queue a fade. Only EVENT_STARTFADE (0) is accepted;
// every other id returns immediately WITHOUT touching the ring (`bnelr`).
//
// The caller's StartFadeParams block carries fadeType as a FLOAT; the console truncates it
// with fctiwz on the way into the command record. No capacity/bounds check exists here --
// faithfully reproduced.
// -------------------------------------------------------------------------------------
int GainFader::Event(int aiEventId, void *apParam)
{
    if (aiEventId != EVENT_STARTFADE)
        return 0;                                   // bnelr

    System *lpSystem = mpSystemUseGetSystemAccessor;             // lwz 4(self)
    const u32 luCursor = lpSystem->muDeferredRingCursor;         // lwz +0x10B8
    StartFadeCommand *lpCommand =
        reinterpret_cast<StartFadeCommand *>(lpSystem->mpDeferredRingBase + luCursor);

    // RECORD STRIDE (X360-literal trap): the console `addi r9,r9,0x20` IS its own
    // sizeof(StartFadeCommand). On the host the widened pointers make the record larger,
    // and ExecuteCommands advances by the handler's RETURN -- StartFadeHandler likewise
    // returns sizeof(StartFadeCommand) -- so the enqueue stride must be the HOST sizeof.
    // The cursor is advanced before the fields are written, exactly as the asm orders it.
    lpSystem->muDeferredRingCursor =
        luCursor + static_cast<u32>(sizeof(StartFadeCommand));   // X360: cursor += 0x20

    const StartFadeParams *lpParams = static_cast<const StartFadeParams *>(apParam);
    lpCommand->mpHandler = &GainFader::StartFadeHandler;         // stw +0x00
    lpCommand->mpTarget = this;                                  // stw +0x04
    lpCommand->startTime = lpParams->startTime;                  // lfd/stfd +0x08
    lpCommand->fadeTime = lpParams->fadeTime;                    // lfs/stfs +0x10
    lpCommand->endGain = lpParams->endGain;                      // lfs/stfs +0x14
    lpCommand->fadeType = PpcFctiwz(lpParams->fadeType);         // fctiwz/stfiwx +0x18
    return 0;                                                     // (console forms none)
}

// -------------------------------------------------------------------------------------
// StartFadeHandler @0x82B9DF18 -- the deferred replay.
//
// A fade with BOTH a zero start time AND a zero duration is not a fade at all: it is an
// immediate jump to endGain, so it lands straight in mLastGain and finishes. Anything else
// is latched as the pending Request for the next Process to service. Both compares are
// ordered (fcmpu + bne), so a NaN in either field takes the real-fade path.
// -------------------------------------------------------------------------------------
int GainFader::StartFadeHandler(void *apCommand)
{
    StartFadeCommand *lpCommand = static_cast<StartFadeCommand *>(apCommand);
    GainFader *lpSelf = lpCommand->mpTarget;

    u8 lu8Unserviced;
    if (lpCommand->startTime == KD_ZERO && lpCommand->fadeTime == KF_ZERO)
    {
        lpSelf->mLastGain = lpCommand->endGain;      // stfs +0x64
        lpSelf->mFadeState = FADESTATE_FINISHED;     // stb  +0x69
        lu8Unserviced = 0;
    }
    else
    {
        lpSelf->mLastRequest.startTime = lpCommand->startTime; // stfd +0x30
        lpSelf->mLastRequest.fadeTime = lpCommand->fadeTime;   // stfs +0x38
        lpSelf->mLastRequest.endGain = lpCommand->endGain;     // stfs +0x3C
        lpSelf->mLastRequest.fadeType = lpCommand->fadeType;   // stw  +0x40
        lu8Unserviced = 1;
    }
    lpSelf->mUnservicedRequest = lu8Unserviced;      // stb +0x68 (both paths)

    // The console `li r3, 0x20` is its own sizeof(StartFadeCommand); it MUST stay identical
    // to the producer's advance above, so both sides use the HOST sizeof.
    return static_cast<int>(sizeof(StartFadeCommand));
}

// -------------------------------------------------------------------------------------
// Process @0x82B97378 -- build this frame's per-sample gain vector and apply it.
// r5 (discontinuity) is unused; every exit returns BUFFERSTATUS_AVAILABLE (1).
//
// Three stages: service any outstanding request, arm a pending fade once its start time is
// within the frame, then either render the fade curve or hold flat at mLastGain. The gain
// vector is built in the DESTINATION buffer as scratch and applied IN PLACE to the source;
// this plug-in does NOT swap the buffer slots.
// -------------------------------------------------------------------------------------
int GainFader::Process(GainFader *self, AudioProcessContext *ctx, bool /*discontinuity*/)
{
    const f32 lfSampleRate = ctx->mpFormat->mfSampleRate;   // lfs 0xC(ctx->+0x30018)

    // ---- stage 1: adopt the latched request --------------------------------------------
    if (self->mUnservicedRequest == 1)
    {
        self->mStartGain = self->mLastGain;                  // the ramp starts where we are
        self->mFadeTime = self->mLastRequest.fadeTime;
        self->mFadeSamplesTotal = PpcFctiwz(self->mFadeTime * lfSampleRate);
        if (self->mFadeSamplesTotal <= 0)
            self->mFadeSamplesTotal = 1;                     // never a zero-length divisor
        self->mStartTime = self->mLastRequest.startTime;
        self->mFadeType = static_cast<u8>(self->mLastRequest.fadeType);
        self->mEndGain = self->mLastRequest.endGain;
        self->mFadeState = FADESTATE_PENDING;
        self->mUnservicedRequest = 0;
    }

    // ---- stage 2: arm the pending fade when its start falls inside this frame -----------
    if (self->mFadeState == FADESTATE_PENDING)
    {
        // A zero start time means "now" -- the delta is left at 0 rather than measured
        // against the stream clock. The compare is ordered, so a NaN start time is NOT
        // treated as zero.
        f64 ldDelta = KD_ZERO;
        if (self->mStartTime != KD_ZERO)
            ldDelta = self->mStartTime - ctx->mdStreamTime;

        const s32 liUntilStart = PpcFctiwz(static_cast<f64>(lfSampleRate) * ldDelta);
        if (liUntilStart < KI_FRAME_SAMPLES)
        {
            // A start already in the past gives a POSITIVE index into the fade (the fade is
            // resumed mid-way); one inside this frame gives a negative index, which is
            // clamped to 0 so the curve starts at the frame boundary.
            const s32 liFrameIndex = -liUntilStart;
            self->mCurrentFadeSample = liFrameIndex;
            if (liFrameIndex > (self->mFadeSamplesTotal - 1))
            {
                self->mFadeState = FADESTATE_FINISHED;   // already elapsed entirely
            }
            else
            {
                if (liFrameIndex > 0)
                    self->mCurrentFadeSample = 0;
                self->mFadeState = FADESTATE_FADING;
            }
        }
    }

    // ---- stage 3: render the gain vector into the destination scratch -------------------
    f32 *lpGainVector = ctx->mpDstBuffer->mpSamples;

    if (self->mFadeState != FADESTATE_FINISHED && self->mFadeState != FADESTATE_PENDING)
    {
        if (self->mFadeType == FADETYPE_LINEARAMPLITUDE)
        {
            GainVectorLinearAmplitude(lpGainVector, KI_FRAME_SAMPLES, self->mStartGain,
                                      self->mEndGain, self->mCurrentFadeSample,
                                      self->mFadeSamplesTotal);
        }
        else if (self->mFadeType == FADETYPE_LINEARPOWER)
        {
            GainVectorLinearPower(lpGainVector, KI_FRAME_SAMPLES, self->mStartGain,
                                  self->mEndGain, self->mCurrentFadeSample,
                                  self->mFadeSamplesTotal);
        }
        else
        {
            // The console's final arm is an ELSE, not an == 2 test: ANY other byte value
            // selects the sine curve.
            GainVectorSine(lpGainVector, KI_FRAME_SAMPLES, self->mStartGain,
                           self->mEndGain, self->mCurrentFadeSample,
                           self->mFadeSamplesTotal);
        }

        self->mCurrentFadeSample += KI_FRAME_SAMPLES;
        if (self->mCurrentFadeSample >= self->mFadeSamplesTotal)
            self->mFadeState = FADESTATE_FINISHED;
    }
    else
    {
        // Holding flat. Unity gain is a no-op, so the console returns immediately WITHOUT
        // writing the vector or touching the samples -- the ordered compare means a NaN
        // hold level does not take this fast path.
        if (self->mLastGain == KF_ONE)
            return 1;
        for (s32 liSample = 0; liSample != KI_FRAME_SAMPLES; ++liSample)
            lpGainVector[liSample] = self->mLastGain;
    }

    // ---- apply, in place on the SOURCE, and publish the level ---------------------------
    SampleBuffer *lpSrc = ctx->mpSrcBuffer;
    for (u32 luChannel = 0; luChannel < self->mInputChannels; ++luChannel)
    {
        f32 *lpSamples = lpSrc->mpSamples + lpSrc->muStride * luChannel;
        for (s32 liSample = 0; liSample != KI_FRAME_SAMPLES; ++liSample)
            lpSamples[liSample] *= lpGainVector[liSample];
    }

    // The frame's final gain becomes both the hold level and the readable attribute.
    self->mLastGain = lpGainVector[KI_FRAME_SAMPLES - 1];
    self->mAttribute[ATTRIBUTE_GETCURRENTGAIN].mfValue = lpGainVector[KI_FRAME_SAMPLES - 1];
    return 1;   // BUFFERSTATUS_AVAILABLE
}

} // namespace core
} // namespace audio
} // namespace rw
