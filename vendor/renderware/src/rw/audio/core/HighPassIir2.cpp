// =====================================================================================
// rw::audio::core::HighPassIir2 -- 2nd-order high-pass biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist. Bodies are
// store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B978B0
//   CreateInstance              @0x82BA2E40  (phase-E callback wave; dossier re-exported)
//   CalculateFilterCoefficients @0x82B978C0
//   Process                     @0x82B9E0A0
//   `scalar deleting destructor' @0x82BA1830
// Layout in Iir2Filters.h (identical data layout to LowPassIir2). The shared biquad
// kernel/state lives in rw::audio::core::Iir2.
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

// The dispatch thunk for the record's Process slot: the console callback takes the
// instance in r3; the PC body is a member, so this static forward IS the dispatched
// (instance, ctx) shape (the BandPassIir2 record precedent).
static int HighPassIir2ProcessThunk(HighPassIir2 *self, AudioProcessContext *ctx)
{
    return self->Process(ctx);
}

// The console record's GetSize slot is the ICF-folded Limiter1::GetSize @0x82B97DA8
// (the console footprints coincide at 0xA8); the HOST objects differ, so the fold is
// unfolded here -- the slot must size THIS type's host object (the stage-carve audit).
static int HighPassIir2GetSizeThunk() { return static_cast<int>(sizeof(HighPassIir2)); }

// off_82F8CFA8 -- the "HighPassIir2" runtime descriptor, REAL (descriptor-record wave;
// record dump progress/scratch_dossiers/plugindesc_layout_codex.md). Metadata FLAG'd
// null per the descriptor-wave convention (no committed consumer reads them).
static PlugInDescRunTime g_HighPassIir2Desc = {
    "HighPassIir2",
    reinterpret_cast<void *>(&HighPassIir2GetSizeThunk),      // @0x82B97DA8 (console ICF fold; host unfolded)
    reinterpret_cast<void *>(&HighPassIir2::CreateInstance),  // @0x82BA2E40
    0,
    reinterpret_cast<void *>(&HighPassIir2ProcessThunk),      // @0x82B9E0A0
    0, 0, 0, 0,
    0,
    0x48493230u,       // 'HI20'
    4, 0, 1, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B978B0 -- return &off_82F8CFA8.
// -------------------------------------------------------------------------------------
char **HighPassIir2::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_HighPassIir2Desc);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2E40
// Initialize<HighPassIir2>(self, 0x28) installs the vtable, points mpAttributes at
// self+0x28 and clears the 6 state slots (the templated PlugIn helper from another TU;
// its observable effect is reproduced inline, the family idiom -- the attribute-base
// store is ATTESTED by the raw Initialize<SndPlayer1> instantiation @0x82B9D368, whose
// body is exactly the vfptr install + the `self+0x28 -> +0x0C` store). Then this seeds
// the cutoff fields to ZERO (unlike LowPass's 1e6 -- high-pass starts wide open), folds
// (450 - oldDecay) into the owning voice's decay accumulator (the REBASE, not a blind
// add), and parks mDecaySamples at 450. Returns 1 (r3 set before the epilogue).
// -------------------------------------------------------------------------------------
int HighPassIir2::CreateInstance(HighPassIir2 *self)
{
    for (int i = 0; i < KI_IIR2_MAX_CHANNELS; ++i)
        Iir2::ClearBuffer(&self->mState[i]);

    // Initialize<T>'s attribute-base wiring: the 0x28 immediate passed in r4 IS the
    // attribute-table offset (base SetAttribute writes land on mfCutoffFreq).
    self->mBase.mpAttributes = &self->mfCutoffFreq;

    const f32 oldDecay = self->mBase.mDecaySamples;   // lfs 0x18 -> f12

    self->mfCutoffFreq = 0.0f;                        // stfs flt_82001CC0 -> 0x28
    self->mfLastCutoffOmega = 0.0f;                   // stfs -> 0xA4

    // Fold this shape's decay-tail delta into the owning voice's accumulator:
    // voice+0x28 == Voice::mfFadeStart; the console adds (450.0 - oldDecay), a rebase.
    self->mBase.mpVoice->mfFadeStart =
        (450.0f - oldDecay) + self->mBase.mpVoice->mfFadeStart;

    self->mBase.mDecaySamples = 450.0f;               // stfs flt_8203869C -> 0x18
    return 1;                                          // li r3,1 (set early, kept)
}

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B978C0
// s = sin(omega), c = cos(omega), k = 1/(1 + s/2). Stores (a1[36..40] ->
// mfA1,mfA2,mfB0,mfB1,mfB2):
//   mfA1 = -2*c * k
//   mfA2 = (1 - s/2) * k
//   mfB0 = (c + 1) / (2*(1 + s/2))
//   mfB2 = (c + 1) / (2*(1 + s/2))
//   mfB1 = -((c + 1) * k)
// -------------------------------------------------------------------------------------
void HighPassIir2::CalculateFilterCoefficients(f64 omega)
{
    const f32 s = static_cast<f32>(sin(omega)); // fp31 (frsp'd)
    const f64 c = cos(omega);

    const f64 half = s * 0.5;
    const f64 k = 1.0 / (half + 1.0);

    mCoeffs.mfA1 = static_cast<f32>((c * -2.0) * k);
    mCoeffs.mfA2 = static_cast<f32>((1.0 - half) * k);
    mCoeffs.mfB0 = static_cast<f32>((c + 1.0) / ((half + 1.0) * 2.0));
    mCoeffs.mfB2 = static_cast<f32>((c + 1.0) / ((half + 1.0) * 2.0));
    mCoeffs.mfB1 = static_cast<f32>(-((c + 1.0) * k));
}

// -------------------------------------------------------------------------------------
// Process @0x82B9E0A0
// Mirror of LowPass but with the band test inverted: high-pass passes above the low
// edge. Above omega_lo it filters and ping-pongs; below it clears the running state on
// the first crossing and parks omega.
// -------------------------------------------------------------------------------------
int HighPassIir2::Process(AudioProcessContext *ctx)
{
    const f32 KF_TWO_PI = 6.2831855f;
    const f32 KF_OMEGA_HI = 3.1384511f;
    const f32 KF_OMEGA_LO = 0.003141593f;

    f32 omega = (mfCutoffFreq / ctx->mpFormat->mfSampleRate) * KF_TWO_PI;

    if (omega > KF_OMEGA_LO)
    {
        if (omega > KF_OMEGA_HI)
            omega = KF_OMEGA_HI;

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
        AudioChannelBuffer *tmp = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = tmp;
    }
    else
    {
        if (mfLastCutoffOmega > KF_OMEGA_LO)
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
