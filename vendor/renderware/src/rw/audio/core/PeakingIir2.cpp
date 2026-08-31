// =====================================================================================
// rw::audio::core::PeakingIir2 -- 2nd-order peaking (parametric) biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist. Bodies are
// store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B9A460
//   GetSize                     @0x82B982D8
//   CalculateFilterCoefficients @0x82B9A470
//   CreateInstance              @0x82BA3708
//   Process                     @0x82B9F168
//   `vector deleting destructor' @0x82BA1B90
// Layout in Iir2Filters.h. Shared biquad kernel/state in rw::audio::core::Iir2.
// =====================================================================================

#include "rw/audio/core/Iir2Filters.h"
#include "rw/audio/core/PlugIn.h"  // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/Voice.h"   // the owning Voice (mfFadeStart decay accumulator)

#include <cmath>

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8F5AC -- run-time plug-in descriptor (undecoded rodata; accessor returns its
// address). FLAGGED: descriptor payload is undecoded rodata.
// off_82F8F5AC -- the "PeakingIir2" runtime descriptor, REAL (descriptor-record
// wave). Metadata FLAG'd null.
// The dispatch thunk for the record's Process slot: the console callback takes
// the instance in r3; the PC body is a member, so this static forward IS the
// dispatched (instance, ctx) shape.
static int PeakingIir2ProcessThunk(PeakingIir2 *self, AudioProcessContext *ctx)
{
    return self->Process(ctx);
}

static PlugInDescRunTime g_PeakingIir2Desc = {
    "PeakingIir2",
    reinterpret_cast<void *>(&PeakingIir2::GetSize),        // @0x82B982D8
    reinterpret_cast<void *>(&PeakingIir2::CreateInstance), // @0x82BA3708
    0,
    reinterpret_cast<void *>(&PeakingIir2ProcessThunk),        // @0x82B9F168
    0, 0, 0, 0,
    0,
    0x50493230u,       // 'PI20'
    4, 0, 3, 0, 0, 0,
    0
};

static inline f64 Fsqrts(f64 x) { return static_cast<f32>(sqrt(x)); }

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A460 -- return &off_82F8F5AC.
// -------------------------------------------------------------------------------------
char **PeakingIir2::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_PeakingIir2Desc);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B982D8 -- the per-instance allocation size, 200 bytes (0xC8).
// -------------------------------------------------------------------------------------
int PeakingIir2::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit, phase-D follow-up): the console
    // immediate under-allocates the widened host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof (the RawPuller2/Send/
    // SinePlayer precedent).
    return static_cast<int>(sizeof(PeakingIir2));   // X360: li r3, 0xC8 (200)
}

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B9A470
// omega = pre-warped centre; gain = linear peak gain; q = quality factor. With
// s = sin(omega), c = cos(omega), A = sqrt(gain), alpha = (s/(2*q)) / A,
// v13 = A * (s/(2*q)), denom = alpha + 1, k = 1/denom, the asm stores (block at +0xA4,
// a1[41..45] -> mfA1,mfA2,mfB0,mfB1,mfB2):
//   mfA2 = (1 - alpha) * k
//   mfB0 = (v13 + 1) * k
//   mfA1 = k * (-2*c)
//   mfB1 = k * (-2*c)
//   mfB2 = (1 - v13) * k
// -------------------------------------------------------------------------------------
void PeakingIir2::CalculateFilterCoefficients(f64 omega, f64 gain, f64 q)
{
    const f32 s = static_cast<f32>(sin(omega)); // fp31 (frsp'd)
    const f64 c = cos(omega);
    const f64 A = Fsqrts(gain);

    const f64 sOver2Q = s / (q * 2.0);
    const f64 alpha = sOver2Q / A;
    const f64 v13 = A * sOver2Q;
    const f64 k = 1.0 / (alpha + 1.0);

    mCoeffs.mfA2 = static_cast<f32>((1.0 - alpha) * k);
    mCoeffs.mfB0 = static_cast<f32>((v13 + 1.0) * k);
    mCoeffs.mfA1 = static_cast<f32>(k * (c * -2.0));
    mCoeffs.mfB1 = static_cast<f32>(k * (c * -2.0));
    mCoeffs.mfB2 = static_cast<f32>((1.0 - v13) * k);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3708
// Initialize<PeakingIir2>(self, 0x28) clears the 6 state slots and bases attributes at
// self+0x28; then clears the active flag, seeds cutoff 96000 / gain 1 / Q 3 (live +
// cached), folds (1500 - oldAttr) into the voice's decay accumulator (mfFadeStart) and parks
// the live attribute at 1500. (Initialize<T> reproduced inline; FLAGGED.)
// -------------------------------------------------------------------------------------
PeakingIir2 *PeakingIir2::CreateInstance(PeakingIir2 *self)
{
    for (int i = 0; i < KI_IIR2_MAX_CHANNELS; ++i)
        Iir2::ClearBuffer(&self->mState[i]);

    // PlugIn::Initialize<T>(self, 0x28) stores self+0x28 in the base attribute-table
    // slot (ARTIST helper @0x82BA1540, stw r11,0xC). The three authored attributes are
    // deliberately eight bytes apart: cutoff +0x28, gain +0x30, Q +0x38.
    self->mBase.mpAttributes = &self->mfCutoffFreq;

    const f32 oldAttr = self->mBase.mDecaySamples;       // lfs 0x18

    self->mbActive = 0;                               // stw 0 -> 0xA0
    self->mfCutoffFreq = 96000.0f;                    // stfs flt_820AA8F0 -> 0x28
    self->mfLastOmega = 96000.0f;                     // stfs -> 0xB8
    self->mfGain = 1.0f;                              // stfs flt_82001C98 -> 0x30
    self->mfLastGain = 1.0f;                          // stfs -> 0xBC
    self->mfQ = 3.0f;                                 // stfs flt_82004270 -> 0x38
    self->mfLastQ = 3.0f;                             // stfs -> 0xC0

    // Fold this shape's decay-tail delta into the owning voice's accumulator:
    // voice+0x28 == Voice::mfFadeStart, BY NAME (2026-08-25 wave 4; was a raw
    // reinterpret over the old void* mpInput misreading).
    self->mBase.mpVoice->mfFadeStart = (1500.0f - oldAttr) + self->mBase.mpVoice->mfFadeStart;

    self->mBase.mDecaySamples = 1500.0f;                  // stfs flt_820266C4 -> 0x18
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9F168
// omega = cutoff/sampleRate*2pi, clamped to [lo, hi]. Bypass (clear-on-deactivate) when
// gain == 1; otherwise activate, clamp Q into [0.2, 20], recompute on a changed
// (omega, gain, Q) and filter every channel, then ping-pong.
// -------------------------------------------------------------------------------------
int PeakingIir2::Process(AudioProcessContext *ctx)
{
    const f32 KF_TWO_PI = 6.2831855f;
    const f32 KF_OMEGA_HI = 3.1384511f;
    const f32 KF_OMEGA_LO = 0.003141593f;
    const f32 KF_Q_MIN = 0.2f;
    const f32 KF_Q_MAX = 20.0f;

    f32 omega = (mfCutoffFreq / ctx->mpFormat->mfSampleRate) * KF_TWO_PI;
    if (omega < KF_OMEGA_LO)
        omega = KF_OMEGA_LO;
    if (omega > KF_OMEGA_HI)
        omega = KF_OMEGA_HI;

    const f32 gain = mfGain;

    if (gain == 1.0f)
    {
        if (mbActive == 1)
        {
            for (u32 ch = 0; ch < mBase.mbChannelCount; ++ch)
                Iir2::ClearBuffer(&mState[ch]);
            mbActive = 0;
        }
        mfLastOmega = omega;
        mfLastGain = mfGain;
        mfLastQ = mfQ;
    }
    else
    {
        if (!mbActive)
            mbActive = 1;

        // Clamp Q into [0.2, 20] (the asm reads Q, defaults the bound to 0.2, and only
        // when Q < 0.2 or Q > 20 stores the bound back into mfQ).
        if (mfQ < KF_Q_MIN)
            mfQ = KF_Q_MIN;
        else if (mfQ > KF_Q_MAX)
            mfQ = KF_Q_MAX;

        if (omega != mfLastOmega || gain != mfLastGain || mfQ != mfLastQ)
        {
            CalculateFilterCoefficients(omega, gain, mfQ);
            mfLastOmega = omega;
            mfLastGain = mfGain;
            mfLastQ = mfQ;
        }

        AudioChannelBuffer *src = ctx->mpSrcBuffer;
        AudioChannelBuffer *dst = ctx->mpDstBuffer;
        for (u32 ch = 0; ch < mBase.mbChannelCount; ++ch)
        {
            Iir2::Filter(&mState[ch],
                         dst->mpSamples + dst->muStride * ch,
                         src->mpSamples + src->muStride * ch,
                         &mCoeffs);
        }
        AudioChannelBuffer *tmp = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = tmp;
    }
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
