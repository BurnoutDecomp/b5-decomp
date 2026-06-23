// =====================================================================================
// rw::audio::core::HighPassIir2 -- 2nd-order high-pass biquad shape.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist. Bodies are
// store-for-store translations of:
//   GetPlugInDescRunTime        @0x82B978B0
//   CalculateFilterCoefficients @0x82B978C0
//   Process                     @0x82B9E0A0
//   `scalar deleting destructor' @0x82BA1830
// Layout in Iir2Filters.h (identical data layout to LowPassIir2). The shared biquad
// kernel/state lives in rw::audio::core::Iir2.
// =====================================================================================

#include "rw/audio/core/Iir2Filters.h"

#include <cmath>

namespace rw
{
namespace audio
{
namespace core
{

// off_82F8CFA8 -- run-time plug-in descriptor (undecoded rodata; accessor returns its
// address). FLAGGED: descriptor payload is undecoded rodata.
static char *g_HighPassIir2Desc = 0; // off_82F8CFA8

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B978B0 -- return &off_82F8CFA8.
// -------------------------------------------------------------------------------------
char **HighPassIir2::GetPlugInDescRunTime()
{
    return &g_HighPassIir2Desc;
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
