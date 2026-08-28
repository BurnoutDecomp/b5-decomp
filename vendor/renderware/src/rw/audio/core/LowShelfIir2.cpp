// =====================================================================================
// rw::audio::core::LowShelfIir2 -- 2nd-order low-shelf biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist. Bodies are
// store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B97E70
//   CalculateFilterCoefficients @0x82B97E80
//   CreateInstance              @0x82BA3198
//   Process                     @0x82B9E720
//   `scalar deleting destructor' @0x82BA19E0
// Layout in Iir2Filters.h. Shared biquad kernel/state in rw::audio::core::Iir2.
// =====================================================================================

#include "rw/audio/core/Iir2Filters.h"
#include "rw/audio/core/PlugIn.h"        // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/plugins/Delay.h" // Delay::GetSize (the ICF-shared GetSize body)
#include "rw/audio/core/Voice.h"   // the owning Voice (mfFadeStart decay accumulator)

#include <cmath>

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8D4A0 -- run-time plug-in descriptor (undecoded rodata; accessor returns its
// address). FLAGGED: descriptor payload is undecoded rodata.
// off_82F8D4A0 -- the "LowShelfIir2" runtime descriptor, REAL (descriptor-record
// wave; console GetSize = the ICF-folded Delay::GetSize @0x82B96A38). Metadata FLAG'd null.
// The dispatch thunk for the record's Process slot: the console callback takes
// the instance in r3; the PC body is a member, so this static forward IS the
// dispatched (instance, ctx) shape.
static int LowShelfIir2ProcessThunk(LowShelfIir2 *self, AudioProcessContext *ctx)
{
    return self->Process(ctx);
}

static PlugInDescRunTime g_LowShelfIir2Desc = {
    "LowShelfIir2",
    reinterpret_cast<void *>(&Delay::GetSize),               // @0x82B96A38 (folded/shared)
    reinterpret_cast<void *>(&LowShelfIir2::CreateInstance), // @0x82BA3198
    0,
    reinterpret_cast<void *>(&LowShelfIir2ProcessThunk),        // @0x82B9E720
    0, 0, 0, 0,
    0,
    0x4C533230u,       // 'LS20'
    4, 0, 2, 0, 0, 0,
    0
};

// sqrtf helper matching the X360 fsqrts (single-precision square root).
static inline f64 Fsqrts(f64 x) { return static_cast<f32>(sqrt(x)); }

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97E70 -- return &off_82F8D4A0.
// -------------------------------------------------------------------------------------
char **LowShelfIir2::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_LowShelfIir2Desc);
}

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B97E80
// omega = pre-warped cutoff; gain = linear shelf gain. With s = sin(omega),
// c = cos(omega), A = sqrt(gain), and the constant 0.70710653 (= 1/sqrt(2)) the asm
// builds the RBJ low-shelf coefficients, normalising by k = 1/denominator. Stores
// (block at +0x9C, a1[39..43] -> mfA1,mfA2,mfB0,mfB1,mfB2):
//   beta  = 2 * sqrt(A) * (s*0.70710653)
//   denom = beta + (A-1)*c + (A+1)
//   k     = 1/denom
//   mfA2 = -(beta - ((A-1)*c + (A+1))) * k
//   mfA1 = ((A+1)*c + (A-1)) * k * -2
//   mfB0 = ( beta - ((A-1)*c - (A+1)) ) * k * A
//   mfB2 = (-(beta + ((A-1)*c - (A+1))) ) * k * A   [fnmsubs form: -(beta - -((A-1)c-(A+1)))]
//   mfB1 = (-((A+1)*c - (A-1)) * k * A) * 2
// (single-precision rounding matches the asm's frsp/fsqrts.)
// -------------------------------------------------------------------------------------
void LowShelfIir2::CalculateFilterCoefficients(f64 omega, f64 gain)
{
    const f32 s = static_cast<f32>(sin(omega)); // fp31 (frsp'd)
    const f64 c = cos(omega);
    const f64 A = Fsqrts(gain);

    const f64 beta = (Fsqrts(A) * (s * 0.70710653)) * 2.0;
    const f64 k = 1.0 / ((beta + ((A - 1.0) * c)) + (A + 1.0));

    mCoeffs.mfA2 = static_cast<f32>(-(beta - (((A - 1.0) * c) + (A + 1.0))) * k);
    mCoeffs.mfA1 = static_cast<f32>((((A + 1.0) * c) + (A - 1.0)) * k * -2.0);
    mCoeffs.mfB0 = static_cast<f32>(((beta - (((A - 1.0) * c) - (A + 1.0))) * k) * A);
    mCoeffs.mfB2 = static_cast<f32>((-(beta - -(((A - 1.0) * c) - (A + 1.0))) * k) * A);
    mCoeffs.mfB1 = static_cast<f32>(((-(((A + 1.0) * c) - (A - 1.0)) * k) * A) * 2.0);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3198
// Initialize<LowShelfIir2>(self, 0x28) clears the 6 state slots and bases attributes at
// self+0x28; then clears the active flag, seeds cutoff 0 / gain 1 (live + cached),
// folds (700 - oldAttr) into the owning voice's decay accumulator (mfFadeStart), parks the live
// attribute at 700. (Initialize<T> reproduced inline; FLAGGED.)
// -------------------------------------------------------------------------------------
LowShelfIir2 *LowShelfIir2::CreateInstance(LowShelfIir2 *self)
{
    for (int i = 0; i < KI_IIR2_MAX_CHANNELS; ++i)
        Iir2::ClearBuffer(&self->mState[i]);

    const f32 oldAttr = self->mBase.mDecaySamples;       // lfs 0x18

    self->mbActive = 0;                               // stw 0 -> 0x98
    self->mfCutoffFreq = 0.0f;                        // stfs flt_82001CC0 -> 0x28
    self->mfLastOmega = 0.0f;                         // stfs -> 0xB0
    self->mfGain = 1.0f;                              // stfs flt_82001C98 -> 0x30
    self->mfLastGain = 1.0f;                          // stfs -> 0xB4

    // Fold this shape's decay-tail delta into the owning voice's accumulator:
    // voice+0x28 == Voice::mfFadeStart, BY NAME (2026-08-25 wave 4; was a raw
    // reinterpret over the old void* mpInput misreading).
    self->mBase.mpVoice->mfFadeStart = (700.0f - oldAttr) + self->mBase.mpVoice->mfFadeStart;

    self->mBase.mDecaySamples = 700.0f;                   // stfs flt_8205820C -> 0x18
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9E720
// omega = cutoff/sampleRate*2pi. Bypass (clear-on-deactivate) when omega is sub-audible
// OR gain == 1; otherwise activate, clamp omega to the high edge, recompute on a
// changed (omega, gain) and filter every channel, then ping-pong.
// -------------------------------------------------------------------------------------
int LowShelfIir2::Process(AudioProcessContext *ctx)
{
    const f32 KF_TWO_PI = 6.2831855f;
    const f32 KF_OMEGA_HI = 3.1384511f;
    const f32 KF_OMEGA_LO = 0.003141593f;

    f32 omega = (mfCutoffFreq / ctx->mpFormat->mfSampleRate) * KF_TWO_PI;
    const f32 gain = mfGain;

    if (omega <= KF_OMEGA_LO || gain == 1.0f)
    {
        if (mbActive == 1)
        {
            for (u32 ch = 0; ch < mBase.mbChannelCount; ++ch)
                Iir2::ClearBuffer(&mState[ch]);
            mbActive = 0;
        }
        mfLastOmega = omega;
        mfLastGain = mfGain;
    }
    else
    {
        if (!mbActive)
            mbActive = 1;
        if (omega > KF_OMEGA_HI)
            omega = KF_OMEGA_HI;
        if (omega != mfLastOmega || gain != mfLastGain)
        {
            CalculateFilterCoefficients(omega, gain);
            mfLastGain = mfGain;
            mfLastOmega = omega;
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
