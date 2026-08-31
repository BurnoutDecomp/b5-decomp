// =====================================================================================
// rw::audio::core::BandPassIir2 -- 2nd-order band-pass biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist. Bodies are
// store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B96A40
//   CalculateFilterCoefficients @0x82B96A50
//   CreateInstance              @0x82BA2398
//   Process                     @0x82B9D778
//   `vector deleting destructor' @0x82BA1638
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

// off_82F8C3D0 -- run-time plug-in descriptor (undecoded rodata; accessor returns its
// address). FLAGGED: descriptor payload is undecoded rodata.
// off_82F8C3D0 -- the "BandPassIir2" runtime descriptor, REAL (descriptor-record
// wave; the console GetSize is the ICF-folded Delay::GetSize @0x82B96A38 -- the
// host record carries the same shared body). Metadata FLAG'd null.
// The dispatch thunk for the record's Process slot: the console callback takes
// the instance in r3; the PC body is a member, so this static forward IS the
// dispatched (instance, ctx) shape.
static int BandPassIir2ProcessThunk(BandPassIir2 *self, AudioProcessContext *ctx)
{
    return self->Process(ctx);
}

// The console record's GetSize slot is the ICF-folded Delay::GetSize @0x82B96A38 (the console
// footprints coincide); the HOST objects differ, so the fold is unfolded here --
// the slot must size THIS type's host object (the stage-carve audit).
static int BandPassIir2GetSizeThunk() { return static_cast<int>(sizeof(BandPassIir2)); }

static PlugInDescRunTime g_BandPassIir2Desc = {
    "BandPassIir2",
    reinterpret_cast<void *>(&BandPassIir2GetSizeThunk),                                       // @0x82B96A38 (console ICF fold; host unfolded)
    reinterpret_cast<void *>(&BandPassIir2::CreateInstance), // @0x82BA2398
    0,
    reinterpret_cast<void *>(&BandPassIir2ProcessThunk),        // @0x82B9D778
    0, 0, 0, 0,
    0,
    0x42493230u,       // 'BI20'
    4, 0, 2, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B96A40 -- return &off_82F8C3D0.
// -------------------------------------------------------------------------------------
char **BandPassIir2::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_BandPassIir2Desc);
}

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B96A50
// omegaLo/omegaHi = pre-warped low/high band edges. Q' = clamp(omegaLo/omegaHi, <= 20).
// With s = sin(omegaLo), c = cos(omegaLo), alpha = s/(2*Q') and k = 1/(alpha + 1) the
// asm stores (block at +0x98, a1[38..42] -> mfA1,mfA2,mfB0,mfB1,mfB2):
//   mfA1 = -2*c * k
//   mfA2 = (1 - alpha) * k
//   mfB0 = k * alpha
//   mfB1 = 0
//   mfB2 = -(k * alpha)
// -------------------------------------------------------------------------------------
void BandPassIir2::CalculateFilterCoefficients(f64 omegaLo, f64 omegaHi)
{
    f64 qp = (omegaLo / omegaHi);
    if (qp > 20.0)
        qp = 20.0;

    const f32 s = static_cast<f32>(sin(omegaLo)); // fp30/fp31 (frsp'd path)
    const f64 alpha = s / (qp * 2.0);
    const f64 k = 1.0 / (alpha + 1.0);
    const f64 c = cos(omegaLo);

    mCoeffs.mfB0 = static_cast<f32>(k * alpha);
    mCoeffs.mfB1 = 0.0f;
    mCoeffs.mfA2 = static_cast<f32>((1.0 - alpha) * k);
    mCoeffs.mfB2 = static_cast<f32>(-(k * alpha));
    mCoeffs.mfA1 = static_cast<f32>((c * -2.0) * k);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2398
// Initialize<BandPassIir2>(self, 0x28) clears the 6 state slots and bases attributes at
// self+0x28; then seeds both band edges to 1e6, the cached omegas to 1e6, re-bases the
// owning voice's decay accumulator (voice+0x28 == Voice::mfFadeStart) by (1000 - oldAttr), parks
// 1000. (Initialize<T> is the templated PlugIn helper from another TU; its observable
// effect is reproduced inline. FLAGGED.)
// -------------------------------------------------------------------------------------
BandPassIir2 *BandPassIir2::CreateInstance(BandPassIir2 *self)
{
    for (int i = 0; i < KI_IIR2_MAX_CHANNELS; ++i)
        Iir2::ClearBuffer(&self->mState[i]);

    self->mBase.mpAttributes = &self->mfFreqLo;       // Initialize<T>(self, 0x28)

    const f32 oldAttr = self->mBase.mDecaySamples;       // lfs 0x18

    self->mfFreqLo = 1000000.0f;                      // stfs flt_820068C0 -> 0x28
    self->mfLastOmegaLo = 1000000.0f;                 // stfs -> 0xAC
    self->mfFreqHi = 1000000.0f;                      // stfs -> 0x30
    self->mfLastOmegaHi = 1000000.0f;                 // stfs -> 0xB0

    // Fold this shape's decay-tail delta into the owning voice's accumulator:
    // voice+0x28 == Voice::mfFadeStart, BY NAME (2026-08-25 wave 4; was a raw
    // reinterpret over the old void* mpInput misreading).
    self->mBase.mpVoice->mfFadeStart = (1000.0f - oldAttr) + self->mBase.mpVoice->mfFadeStart;

    self->mBase.mDecaySamples = 1000.0f;                  // stfs flt_82009E10 -> 0x18
    return self;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9D778
// omegaLo/omegaHi = freq/sampleRate*2pi, each clamped to [lo, hi]. A passband-validity
// test (the band must not be degenerate around Nyquist) gates filtering. On a valid,
// changed band it recomputes coefficients and filters every channel, then ping-pongs;
// on an invalid band it clears the running state. Constants: 2pi=6.2831855,
// hi=3.1384511, lo=0.003141593, pi=3.1415927, pi/2=1.5707964.
// -------------------------------------------------------------------------------------
int BandPassIir2::Process(AudioProcessContext *ctx)
{
    const f32 KF_TWO_PI = 6.2831855f;
    const f32 KF_OMEGA_HI = 3.1384511f;
    const f32 KF_OMEGA_LO = 0.003141593f;
    const f32 KF_PI = 3.1415927f;
    const f32 KF_HALF_PI = 1.5707964f;

    const f32 invRate = 1.0f / ctx->mpFormat->mfSampleRate;
    f32 omegaLo = (mfFreqLo * invRate) * KF_TWO_PI;
    f32 omegaHi = (mfFreqHi * invRate) * KF_TWO_PI;

    if (omegaLo < KF_OMEGA_LO)
        omegaLo = KF_OMEGA_LO;
    if (omegaLo > KF_OMEGA_HI)
        omegaLo = KF_OMEGA_HI;
    if (omegaHi < KF_OMEGA_LO)
        omegaHi = KF_OMEGA_LO;

    const bool valid =
        (omegaLo > KF_HALF_PI || omegaHi <= ((KF_PI - omegaLo) * 2.0f)) &&
        (omegaLo <= KF_HALF_PI || omegaHi <= (omegaLo * 2.0f));

    if (valid)
    {
        if (omegaLo != mfLastOmegaLo || omegaHi != mfLastOmegaHi)
        {
            CalculateFilterCoefficients(omegaLo, omegaHi);
            mfLastOmegaLo = omegaLo;
            mfLastOmegaHi = omegaHi;
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
    else
    {
        // The re-test on the cached omegas mirrors the asm (it gates the clear on the
        // previously-valid band) before parking the new omegas.
        const f32 lastLo = mfLastOmegaLo;
        const bool lastValid =
            (lastLo > KF_HALF_PI || mfLastOmegaHi <= ((KF_PI - mfLastOmegaLo) * 2.0f)) &&
            (lastLo <= KF_HALF_PI || mfLastOmegaHi <= (mfLastOmegaLo * 2.0f));
        if (lastValid)
        {
            for (u32 ch = 0; ch < mBase.mbChannelCount; ++ch)
                Iir2::ClearBuffer(&mState[ch]);
        }
        mfLastOmegaLo = omegaLo;
        mfLastOmegaHi = omegaHi;
    }
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
