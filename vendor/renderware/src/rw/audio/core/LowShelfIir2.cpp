// =====================================================================================
// rw::audio::core::LowShelfIir2 -- 2nd-order low-shelf biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No LegacySrc reference source and no DecFIGS DWARF exist. Bodies are
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

// The console record's GetSize slot is the ICF-folded Delay::GetSize @0x82B96A38 (the console
// footprints coincide); the HOST objects differ, so the fold is unfolded here --
// the slot must size THIS type's host object (the stage-carve audit).
static int LowShelfIir2GetSizeThunk() { return static_cast<int>(sizeof(LowShelfIir2)); }

static PlugInDescRunTime g_LowShelfIir2Desc = {
    "LowShelfIir2",
    reinterpret_cast<void *>(&LowShelfIir2GetSizeThunk),                                       // @0x82B96A38 (console ICF fold; host unfolded)
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
// (block at +0x9C, a1[39..43] -> a1,a2,b0,b1,b2):
//   beta  = 2 * sqrt(A) * (s*0.70710653)
//   denom = beta + (A-1)*c + (A+1)
//   k     = 1/denom
//   a2 = -(beta - ((A-1)*c + (A+1))) * k
//   a1 = ((A+1)*c + (A-1)) * k * -2
//   b0 = ( beta - ((A-1)*c - (A+1)) ) * k * A
//   b2 = (-(beta + ((A-1)*c - (A+1))) ) * k * A   [fnmsubs form: -(beta - -((A-1)c-(A+1)))]
//   b1 = (-((A+1)*c - (A-1)) * k * A) * 2
// (single-precision rounding matches the asm's frsp/fsqrts.)
// -------------------------------------------------------------------------------------
void LowShelfIir2::CalculateFilterCoefficients(f64 omega, f64 gain)
{
    const f32 s = static_cast<f32>(sin(omega)); // fp31 (frsp'd)
    const f64 c = cos(omega);
    const f64 A = Fsqrts(gain);

    const f64 beta = (Fsqrts(A) * (s * 0.70710653)) * 2.0;
    const f64 k = 1.0 / ((beta + ((A - 1.0) * c)) + (A + 1.0));

    mCoeffs.a2 = static_cast<f32>(-(beta - (((A - 1.0) * c) + (A + 1.0))) * k);
    mCoeffs.a1 = static_cast<f32>((((A + 1.0) * c) + (A - 1.0)) * k * -2.0);
    mCoeffs.b0 = static_cast<f32>(((beta - (((A - 1.0) * c) - (A + 1.0))) * k) * A);
    mCoeffs.b2 = static_cast<f32>((-(beta - -(((A - 1.0) * c) - (A + 1.0))) * k) * A);
    mCoeffs.b1 = static_cast<f32>(((-(((A + 1.0) * c) - (A - 1.0)) * k) * A) * 2.0);
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
        self->mState[i].ClearBuffer();

    self->mBase.mpAttributes = &self->mfCutoffFreq;   // Initialize<T>(self, 0x28)

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
                mState[ch].ClearBuffer();
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
            mState[ch].Filter(dst->mpSamples + dst->muStride * ch,
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
