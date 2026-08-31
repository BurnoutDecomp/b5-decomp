// =====================================================================================
// rw::audio::core::LowPassIir2 -- 2nd-order low-pass biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative for every store and every coefficient computation. No Feb-2007 leak
// source and no DecFIGS DWARF exist. Bodies are store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B97DB0
//   CalculateFilterCoefficients @0x82B97DC0
//   CreateInstance              @0x82BA3130
//   Process                     @0x82B9E5C8
//   `vector deleting destructor' @0x82BA1998
// See Iir2Filters.h for the byte-exact layout. The shared biquad kernel/state and
// per-channel ClearBuffer live in rw::audio::core::Iir2 (Iir2.h/.cpp).
// =====================================================================================

#include "rw/audio/core/Iir2Filters.h"
#include "rw/audio/core/PlugIn.h"           // PlugInDescRunTime (the real descriptor record)
#include "rw/audio/core/plugins/Limiter1.h" // Limiter1::GetSize (the ICF-shared GetSize body)
#include "rw/audio/core/Voice.h"   // the owning Voice (mfFadeStart decay accumulator)

#include <cmath>

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8D3D4 -- this shape's run-time plug-in descriptor (a static rwaudio rodata
// record; its contents are undecoded and not reconstructed here). The accessor just
// returns its address, exactly as the X360 build does. FLAGGED: descriptor payload is
// undecoded rodata.
// off_82F8D3D4 -- the "LowPassIir2" runtime descriptor, REAL (descriptor-record
// wave; console GetSize = the ICF-folded Limiter1::GetSize @0x82B97DA8). Metadata FLAG'd null.
// The dispatch thunk for the record's Process slot: the console callback takes
// the instance in r3; the PC body is a member, so this static forward IS the
// dispatched (instance, ctx) shape.
static int LowPassIir2ProcessThunk(LowPassIir2 *self, AudioProcessContext *ctx)
{
    return self->Process(ctx);
}

// The console record's GetSize slot is the ICF-folded Limiter1::GetSize @0x82B97DA8 (the console
// footprints coincide); the HOST objects differ, so the fold is unfolded here --
// the slot must size THIS type's host object (the stage-carve audit).
static int LowPassIir2GetSizeThunk() { return static_cast<int>(sizeof(LowPassIir2)); }

static PlugInDescRunTime g_LowPassIir2Desc = {
    "LowPassIir2",
    reinterpret_cast<void *>(&LowPassIir2GetSizeThunk),                                       // @0x82B97DA8 (console ICF fold; host unfolded)
    reinterpret_cast<void *>(&LowPassIir2::CreateInstance), // @0x82BA3130
    0,
    reinterpret_cast<void *>(&LowPassIir2ProcessThunk),        // @0x82B9E5C8
    0, 0, 0, 0,
    0,
    0x4C493230u,       // 'LI20'
    4, 0, 1, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97DB0 -- return &off_82F8D3D4.
// -------------------------------------------------------------------------------------
char **LowPassIir2::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_LowPassIir2Desc);
}

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B97DC0
// omega = pre-warped cutoff angular frequency (rad/sample). Computes the 5 biquad
// coefficients into mCoeffs (block at +0x90). With s = sin(omega), c = cos(omega) and
// k = 1 / (1 + s/2) the asm computes (a1[36..40] -> mfA1,mfA2,mfB0,mfB1,mfB2):
//   mfA1 = -2*c * k
//   mfA2 = (1 - s/2) * k
//   mfB1 = (1 - c) * k
//   mfB0 = (1 - c) / (2*(1 + s/2))
//   mfB2 = (1 - c) / (2*(1 + s/2))
// (sin is rounded to single precision before reuse: fp31 holds s as a 32-bit value.)
// -------------------------------------------------------------------------------------
void LowPassIir2::CalculateFilterCoefficients(f64 omega)
{
    const f32 s = static_cast<f32>(sin(omega)); // fp31 (frsp'd)
    const f64 c = cos(omega);

    const f64 half = s * 0.5;
    const f64 k = 1.0 / (half + 1.0);

    mCoeffs.mfA1 = static_cast<f32>((c * -2.0) * k);
    mCoeffs.mfA2 = static_cast<f32>((1.0 - half) * k);
    mCoeffs.mfB1 = static_cast<f32>((1.0 - c) * k);
    mCoeffs.mfB0 = static_cast<f32>((1.0 - c) / ((half + 1.0) * 2.0));
    mCoeffs.mfB2 = static_cast<f32>((1.0 - c) / ((half + 1.0) * 2.0));
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3130
// Initialize<LowPassIir2>(self, 0x28) clears the 6 state slots and points mpAttributes
// at self+0x28; then this seeds the cutoff fields, folds (450 - oldDecay) into the owning
// voice's decay accumulator (mfFadeStart), and parks mDecaySamples at 450.
// Initialize<T> is the templated PlugIn helper defined in another TU; its observable
// effect (clear states, set attribute base) is reproduced inline. FLAGGED: the real
// Initialize<T> body lives in the PlugIn allocator TU.
// -------------------------------------------------------------------------------------
LowPassIir2 *LowPassIir2::CreateInstance(LowPassIir2 *self)
{
    for (int i = 0; i < KI_IIR2_MAX_CHANNELS; ++i)
        Iir2::ClearBuffer(&self->mState[i]);

    self->mBase.mpAttributes = &self->mfCutoffFreq;   // Initialize<T>(self, 0x28)

    const f32 oldAttr = self->mBase.mDecaySamples;       // lfs 0x18

    self->mfCutoffFreq = 1000000.0f;                  // stfs flt_820068C0 -> 0x28
    self->mfLastCutoffOmega = 1000000.0f;             // stfs -> 0xA4

    // Fold this shape's decay-tail delta into the owning voice's accumulator:
    // voice+0x28 == Voice::mfFadeStart, BY NAME (2026-08-25 wave 4; was a raw
    // reinterpret over the old void* mpInput misreading).  // *(*(self+8)+0x28)
    self->mBase.mpVoice->mfFadeStart = (450.0f - oldAttr) + self->mBase.mpVoice->mfFadeStart;

    self->mBase.mDecaySamples = 450.0f;                   // stfs flt_8203869C -> 0x18
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9E5C8
// omega = cutoff / sampleRate * 2pi. Below ~Nyquist it (re)computes coefficients on a
// cutoff change and filters every active channel, then ping-pongs the src/dst buffer
// slots. Above the band it clears the running state (on the first frame that crosses
// the threshold) and parks omega. Constants: 2pi=6.2831855, hi=3.1384511, lo=0.003141593.
// -------------------------------------------------------------------------------------
int LowPassIir2::Process(AudioProcessContext *ctx)
{
    const f32 KF_TWO_PI = 6.2831855f;
    const f32 KF_OMEGA_HI = 3.1384511f;
    const f32 KF_OMEGA_LO = 0.003141593f;

    f32 omega = (mfCutoffFreq / ctx->mpFormat->mfSampleRate) * KF_TWO_PI;

    if (omega < KF_OMEGA_HI)
    {
        if (omega < KF_OMEGA_LO)
            omega = KF_OMEGA_LO;

        if (omega != mfLastCutoffOmega)
        {
            CalculateFilterCoefficients(omega);
            mfLastCutoffOmega = omega;
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
        // Ping-pong the source/destination buffer slots.
        AudioChannelBuffer *tmp = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = tmp;
    }
    else
    {
        if (mfLastCutoffOmega < KF_OMEGA_HI)
        {
            for (u32 ch = 0; ch < mBase.mbChannelCount; ++ch)
                Iir2::ClearBuffer(&mState[ch]);
        }
        mfLastCutoffOmega = omega;
    }
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
