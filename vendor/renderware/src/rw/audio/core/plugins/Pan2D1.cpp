// =====================================================================================
// rw::audio::core::Pan2D1 -- the "Pan2D1" spatial (2D) panner plug-in bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative for every
// store and branch. See plugins/Pan2D1.h for the byte-exact layout and the per-function X360
// addresses. All maths here is scalar (fadds/fmuls/fmadds/fsqrts/fdivs + cos/sin/atan2/floor);
// the rodata immediates are ordinary constants (pi, 1/2pi, 1/64, deg->rad), reproduced below.
//
// This file also carries Pan2D1::Speaker::Set (a distinct ledger TU) -- its body is unchanged.
// =====================================================================================

#include "rw/audio/core/plugins/Pan2D1.h"
#include "rw/audio/core/MixKernels.h" // CopyWithGain / MixWithGain / CopyWithGainRamp / MixWithGainRamp

#include <cmath>
#include <cstring> // memcpy (the X360 blkmov intrinsic)

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// Pan2D1::Speaker::Set @0x82B986F0
//
// r3=this, f1=theta. theta is preserved across the cos call, then reused as the argument to
// sin. Both results are narrowed to f32 (frsp) before storage.
// ---------------------------------------------------------------------------
void Pan2D1::Speaker::Set(f64 theta)
{
    x = static_cast<f32>(cos(theta)); // FLAG (rwaudio PDB reconcile): mfGainA -> x
    y = static_cast<f32>(sin(theta)); // FLAG (rwaudio PDB reconcile): mfGainB -> y
}

// ---- rodata immediates (well-known math constants; grounded by the asm's flt_* loads) ----
namespace
{
const f64 KF_NEG_DEG2RAD = -0.017453292; // flt_82006D74 (-pi/180: emitter angles are negated)
const f64 KF_INV_TWO_PI  = 0.15915494;   // flt_82001C90 (1 / 2pi)
const f64 KF_TWO_PI      = 6.2831855;    // flt_82001C94 (2pi)
const f64 KF_HALF_PI     = 1.5707964;    // flt_82047B38 (pi/2)
const f64 KF_PI          = 3.1415927;    // flt_8215A980 (pi)
const f64 KF_HALF        = 0.5;          // flt_82001DA0
const f64 KF_ONE         = 1.0;          // flt_82001C98
const f64 KF_ZERO        = 0.0;          // flt_82001CC0
const f64 KF_TWO         = 2.0;          // flt_82001D9C
const f64 KF_RAMP_STEP   = 0.015625;     // flt_820ADC00 (1 / GAIN_DECLICK_FRAME_SIZE == 1/64)
const s32 KI_FRAME       = 256;          // MIXER_FRAME_SIZE (r6/r7 == 0x100 at every kernel call)

// off_82F8F140 -- the registered run-time descriptor record (its +0x00 label is the string
// "Pan2D1"). off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls
// (shared with Gain / Limiter1 / ReverbModel1). These are opaque data symbols in the XEX;
// modelled as honest placeholders so the bodies link without fabricating their contents.
char       *g_Pan2D1Desc      = nullptr; // off_82F8F140 (the "Pan2D1" record)
void *const KBASE_PLUGIN_VTAB = nullptr; // off_820AA810

// Finish one EmitterConfig pan target: store the position, clamp its squared magnitude to
// 1.0 (renormalising the position when it exceeds the unit circle), then set the angle from
// the *stored* (f32-narrowed) coordinates. Reproduces the shared LABEL_23/LABEL_24 tail.
void FinishTarget(Pan2D1::PanTarget &t, f64 x, f64 y)
{
    const f64 mag = (y * y) + (x * x);
    t.x   = static_cast<f32>(x);
    t.y   = static_cast<f32>(y);
    t.mag = static_cast<f32>(mag);
    if (mag > KF_ONE)
    {
        t.mag = static_cast<f32>(KF_ONE);
        const f64 inv = KF_ONE / std::sqrt(mag);
        t.x = static_cast<f32>(x * inv);
        t.y = static_cast<f32>(y * inv);
    }
    t.angle = static_cast<f32>(std::atan2(static_cast<f64>(t.y), static_cast<f64>(t.x)));
}
} // namespace

// ---------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B98748 -- return the address of the registered descriptor record
// (its label is the string "Pan2D1").
//   return &off_82F8F140;
// ---------------------------------------------------------------------------
char **Pan2D1::GetPlugInDescRunTime()
{
    return &g_Pan2D1Desc; // &off_82F8F140
}

// ---------------------------------------------------------------------------
// GetSize @0x82B982C8 -- the plug-in instance footprint.
//   li r3, 0x1B8
// ---------------------------------------------------------------------------
int Pan2D1::GetSize()
{
    return 440; // 0x1B8
}

// ---------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA1B00 -- reinstall the base PlugIn v-table, then
// conditionally free. (~Pan2D1 is trivial and folded away -- the asm performs no member
// teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// ---------------------------------------------------------------------------
void *Pan2D1::ScalarDeletingDestructor(Pan2D1 *self, char flags)
{
    self->mBase.mpVTable = KBASE_PLUGIN_VTAB; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// ---------------------------------------------------------------------------
// SpeakerConfig @0x82B98758 -- build the five speaker-direction unit vectors from the two
// half-angles, then the four inverse 2x2 matrices that map an arbitrary in-plane direction
// onto adjacent speaker pairs. (r3 == this; the pseudocode treats it as a float* `a1`.)
// ---------------------------------------------------------------------------
int Pan2D1::SpeakerConfig()
{
    mfSpeakerScale = static_cast<f32>(std::cos(static_cast<f64>(mfSpeakerAngle0)) * KF_TWO);

    mSpeakerDir[0].Set(mfSpeakerAngle0);          // Set(a1+79,  a1[31])
    mSpeakerDir[2].Set(-mfSpeakerAngle0);         // Set(a1+83, -a1[31])
    mSpeakerDir[3].Set(mfSpeakerAngle1);          // Set(a1+85,  a1[32])
    mSpeakerDir[4].Set(-mfSpeakerAngle1);         // Set(a1+87, -a1[32])
    mSpeakerDir[1].x = static_cast<f32>(KF_ONE);  // a1[81]
    mSpeakerDir[1].y = static_cast<f32>(KF_ZERO); // a1[82]

    // Matrix block 0 (mfPanMatrix[0..3]) -- inverse of [dir0, dir2].
    {
        const f64 inv = KF_ONE / ((mSpeakerDir[0].x * mSpeakerDir[2].y) -
                                  (mSpeakerDir[0].y * mSpeakerDir[2].x));
        mfPanMatrix[0] = static_cast<f32>(inv * mSpeakerDir[0].x);
        mfPanMatrix[3] = static_cast<f32>(inv * mSpeakerDir[2].y);
        mfPanMatrix[1] = static_cast<f32>(-(inv * mSpeakerDir[0].y));
        mfPanMatrix[2] = static_cast<f32>(-(inv * mSpeakerDir[2].x));
    }

    // Matrix block 3 (mfPanMatrix[12..15]) -- inverse of [dir3, dir0].
    {
        const f64 inv = KF_ONE / ((mSpeakerDir[3].x * mSpeakerDir[0].y) -
                                  (mSpeakerDir[3].y * mSpeakerDir[0].x));
        mfPanMatrix[12] = static_cast<f32>(inv * mSpeakerDir[3].x);
        mfPanMatrix[15] = static_cast<f32>(inv * mSpeakerDir[0].y);
        mfPanMatrix[13] = static_cast<f32>(-(inv * mSpeakerDir[3].y));
        mfPanMatrix[14] = static_cast<f32>(-(inv * mSpeakerDir[0].x));
    }

    // Matrix block 2 (mfPanMatrix[8..11]) -- inverse of [dir4, dir3].
    {
        const f64 inv = KF_ONE / ((mSpeakerDir[4].x * mSpeakerDir[3].y) -
                                  (mSpeakerDir[4].y * mSpeakerDir[3].x));
        mfPanMatrix[8]  = static_cast<f32>(inv * mSpeakerDir[4].x);
        mfPanMatrix[11] = static_cast<f32>(inv * mSpeakerDir[3].y);
        mfPanMatrix[9]  = static_cast<f32>(-(inv * mSpeakerDir[4].y));
        mfPanMatrix[10] = static_cast<f32>(-(inv * mSpeakerDir[3].x));
    }

    // Matrix block 1 (mfPanMatrix[4..7]) -- inverse of [dir2, dir4].
    {
        const f64 inv = KF_ONE / ((mSpeakerDir[2].x * mSpeakerDir[4].y) -
                                  (mSpeakerDir[2].y * mSpeakerDir[4].x));
        mfPanMatrix[4] = static_cast<f32>(inv * mSpeakerDir[2].x);
        mfPanMatrix[7] = static_cast<f32>(inv * mSpeakerDir[4].y);
        mfPanMatrix[5] = static_cast<f32>(-(inv * mSpeakerDir[2].y));
        mfPanMatrix[6] = static_cast<f32>(-(inv * mSpeakerDir[4].x));
    }

    return 0; // asm tail is the last Speaker::Set return (discarded by CreateInstance)
}

// ---------------------------------------------------------------------------
// EmitterConfig @0x82B98900 -- map the emitter azimuth/spread/radius/width attributes into a
// set of pan targets (one per virtual source), by mode (miNumSources 1/2/4/5). The negative
// deg->rad constant negates the angles. Each target is finished (clamp + atan2) by
// FinishTarget, except the mode-1 single source whose angle is taken directly from azimuth.
// ---------------------------------------------------------------------------
int Pan2D1::EmitterConfig()
{
    f64 azRad         = mfAzimuthDeg * KF_NEG_DEG2RAD; // v1
    const f64 radius  = mfRadius;                       // v2
    const f64 spread  = mfSpreadDeg * KF_NEG_DEG2RAD;   // v3
    const f64 width   = mfWidth;                         // v4
    const f64 baseX   = std::cos(azRad) * radius;        // v5
    const f64 baseY   = std::sin(azRad) * radius;        // v8 (= sin(azRad) * radius)

    switch (miNumSources)
    {
    case 1:
    {
        // Single source at the emitter position; magnitude clamp, angle straight from azimuth.
        PanTarget &t = mPanTarget[0];
        const f64 mag = (baseY * baseY) + (baseX * baseX);
        t.x   = static_cast<f32>(baseX);
        t.y   = static_cast<f32>(baseY);
        t.mag = static_cast<f32>(mag);
        if (mag > KF_ONE)
        {
            const f64 inv = KF_ONE / std::sqrt(mag);
            t.mag = static_cast<f32>(KF_ONE);
            t.x   = static_cast<f32>(inv * baseX);
            t.y   = static_cast<f32>(inv * baseY);
        }
        if (radius <= KF_ZERO)
            azRad += KF_PI;
        t.angle = static_cast<f32>(azRad);
        break;
    }
    case 2:
    {
        // A rotated pair straddling the emitter: sum and difference of a pi/2-offset vector.
        const f64 ang  = (spread + azRad) + KF_HALF_PI;
        const f64 offX = std::cos(ang) * width; // v57
        const f64 offY = std::sin(ang) * width; // v60 (= sin(ang) * width)
        FinishTarget(mPanTarget[0], offX + baseX, offY + baseY);
        FinishTarget(mPanTarget[1], baseX - offX, baseY - offY);
        break;
    }
    case 5:
    {
        // Mode 5 first lays down mPanTarget[1] from the centre spread vector, then falls
        // through to the mode-4 fan (which uses the {2,3,4} index set for miNumSources == 5).
        const f64 ang = spread + azRad;
        FinishTarget(mPanTarget[1], (std::cos(ang) * width) + baseX,
                                    (std::sin(ang) * width) + baseY);
    }
    // fallthrough
    case 4:
    {
        int i0, i1, i2;
        if (miNumSources == 4) { i0 = 1; i1 = 2; i2 = 3; }
        else                   { i0 = 2; i1 = 3; i2 = 4; }

        const f64 a = (mfSpeakerAngle0 + spread) + azRad;
        FinishTarget(mPanTarget[0], (std::cos(a) * width) + baseX,
                                    (std::sin(a) * width) + baseY);

        const f64 b = (spread + azRad) - mfSpeakerAngle0;
        FinishTarget(mPanTarget[i0], (std::cos(b) * width) + baseX,
                                     (std::sin(b) * width) + baseY);

        const f64 c = (mfSpeakerAngle1 + spread) + azRad;
        FinishTarget(mPanTarget[i1], (std::cos(c) * width) + baseX,
                                     (std::sin(c) * width) + baseY);

        const f64 d = (spread + azRad) - mfSpeakerAngle1;
        FinishTarget(mPanTarget[i2], (std::cos(d) * width) + baseX,
                                     (std::sin(d) * width) + baseY);
        break;
    }
    default:
        break;
    }

    return 0; // asm tail is the restfpr trampoline return (discarded by Process)
}

// ---------------------------------------------------------------------------
// ComputePerimeterTerm @0x82B98F78 -- for one virtual source `index`, wrap its target angle
// into [0, 2pi) relative to the speaker half-angles, pick the angular sector, and accumulate
// the source's contribution into that sector's two speaker gains (via the inverse pan matrix
// for the sector). (r3 == this, r4 == index.)
// ---------------------------------------------------------------------------
void Pan2D1::ComputePerimeterTerm(int index)
{
    const PanTarget &t = mPanTarget[index];
    const f64 wrapped  = (t.angle + mfSpeakerAngle0) * KF_INV_TWO_PI;
    const f64 theta    = ((wrapped - std::floor(wrapped)) * KF_TWO_PI) - mfSpeakerAngle0; // v6
    const f64 s        = std::sin(theta); // v7
    const f64 c        = std::cos(theta); // v8
    const f64 mag      = t.mag;           // *(v4 + 364)

    f32 *g = &mfGains[5 * index];

    if (theta >= mfSpeakerAngle0)
    {
        if (theta >= mfSpeakerAngle1)
        {
            if (theta >= (KF_TWO_PI - mfSpeakerAngle1))
            {
                // Sector using mfPanMatrix[4..7].
                const f64 p = (mfPanMatrix[5] * c) + (mfPanMatrix[4] * s); // v30
                const f64 q = (mfPanMatrix[7] * c) + (mfPanMatrix[6] * s); // v31
                const f64 k = mag / std::sqrt((q * q) + (p * p));          // v33
                g[4] = static_cast<f32>((p * k) + g[4]);
                g[2] = static_cast<f32>((q * k) + g[2]);
            }
            else
            {
                // Sector using mfPanMatrix[8..11].
                const f64 p = (mfPanMatrix[9] * c) + (mfPanMatrix[8] * s);   // v25
                const f64 q = (mfPanMatrix[11] * c) + (mfPanMatrix[10] * s); // v26
                const f64 k = mag / std::sqrt((q * q) + (p * p));           // v28
                g[3] = static_cast<f32>((p * k) + g[3]);
                g[4] = static_cast<f32>((q * k) + g[4]);
            }
        }
        else
        {
            // Sector using mfPanMatrix[12..15].
            const f64 p = (mfPanMatrix[13] * c) + (mfPanMatrix[12] * s); // v21
            const f64 q = (mfPanMatrix[15] * c) + (mfPanMatrix[14] * s); // v22
            const f64 k = mag / std::sqrt((q * q) + (p * p));           // v24
            g[0] = static_cast<f32>((p * k) + g[0]);
            g[3] = static_cast<f32>((q * k) + g[3]);
        }
    }
    else
    {
        // Interior sector using mfPanMatrix[0..3], with an optional mode-5 focus fold.
        f64 focus = KF_ZERO;                                       // v9
        f64 u     = (mfPanMatrix[1] * c) + (mfPanMatrix[0] * s);    // v10
        f64 v     = (mfPanMatrix[3] * c) + (mfPanMatrix[2] * s);    // v11
        if (miNumSpeakers == 5)
        {
            const f64 lesser = (u >= v) ? v : u;                   // v12
            const f64 fold   = mfFocus * lesser;                   // v13
            u    -= fold;
            v    -= fold;
            focus = mfSpeakerScale * fold;                          // v9
        }
        const f64 k = mag / std::sqrt((u * u) + ((focus * focus) + (v * v))); // v18/v19
        g[0] = static_cast<f32>((k * v) + g[0]);
        g[2] = static_cast<f32>((k * u) + g[2]);
        g[1] = static_cast<f32>((k * focus) + g[1]);
    }
}

// ---------------------------------------------------------------------------
// ComputeLevels @0x82B991D8 -- build each virtual source's 5-speaker gain vector. The stereo
// fast path (miNumSpeakers == 2) derives a constant-power L/R pair straight from the target's
// y coordinate; otherwise it sums the interior + perimeter terms and normalises the 5-vector.
// ---------------------------------------------------------------------------
int Pan2D1::ComputeLevels()
{
    const f64 level = mfLevel;         // v2
    const int count = miNumSources;    // v4
    mfCentreGain = mfCentreLevel;      // *(this+312) = *(this+88)

    if (miNumSpeakers == 2)
    {
        for (int k = 0; k < count; ++k)
        {
            f32 *g = &mfGains[5 * k];
            const f64 pan = (mPanTarget[k].y + KF_ONE) * KF_HALF; // v8
            g[0] = static_cast<f32>(pan);
            g[1] = static_cast<f32>(KF_ONE - pan);
            const f64 norm = (KF_ONE / std::sqrt(((KF_ONE - pan) * (KF_ONE - pan)) +
                                                 (pan * pan))) * mfNormGain * level; // v9
            g[0] = static_cast<f32>(norm * pan);
            g[1] = static_cast<f32>((KF_ONE - pan) * norm);
        }
    }
    else
    {
        for (int k = 0; k < count; ++k)
        {
            ComputeInteriorTerm(k);
            ComputePerimeterTerm(k);

            f32 *g = &mfGains[5 * k];
            const f64 g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3], g4 = g[4];
            const f64 inv   = KF_ONE / std::sqrt((g1 * g1) +
                                     ((g0 * g0) + ((g2 * g2) + ((g4 * g4) + (g3 * g3))))); // v17
            const f64 scale = (inv * mfNormGain) * level;                                   // v18
            g[0] = static_cast<f32>(g0 * scale);
            g[1] = static_cast<f32>(g1 * scale);
            g[2] = static_cast<f32>(g2 * scale);
            g[3] = static_cast<f32>(g3 * scale);
            g[4] = static_cast<f32>(g4 * scale);
        }
    }

    return 0; // asm tail is the last kernel/loop value (discarded by Process)
}

// ---------------------------------------------------------------------------
// PanOutput @0x82B995C8 -- apply the (hard) pan matrix to one frame: distribute source
// channel 0 across the speakers, mix the remaining source channels through the [source x
// speaker] gain matrix, then fold the 6th (centre/LFE) channel when the output is 5.1.
//   pDst == a2 (destination channel buffers), pSrc == a3 (source channel buffers).
// ---------------------------------------------------------------------------
int Pan2D1::PanOutput(AudioChannelBuffer *pDst, AudioChannelBuffer *pSrc)
{
    const int nSpeakers = miNumSpeakers; // v7
    int sel[5];
    if (nSpeakers == 4)
    {
        sel[0] = 0; sel[1] = 2; sel[2] = 3; sel[3] = 4;
    }
    else
    {
        for (int i = 0; i < 5; ++i)
            sel[i] = i;
    }

    f32 *pSrcBase = pSrc->mpSamples; // v9

    // Source channel 0 -> every speaker (overwriting copy).
    if (nSpeakers != 0)
    {
        for (unsigned c = 0; c < static_cast<unsigned>(miNumSpeakers); ++c)
            CopyWithGain(pDst->mpSamples + pDst->muStride * c, pSrcBase,
                         mfGains[sel[c]], KI_FRAME);
    }

    // Source channels 1..N -> every speaker (accumulating), via the gain matrix.
    if (miNumSources > 1)
    {
        for (unsigned row = 1; row < static_cast<unsigned>(miNumSources); ++row)
        {
            f32 *pSrcRow = pSrc->mpSamples + pSrc->muStride * row; // v16
            for (unsigned c = 0; c < static_cast<unsigned>(miNumSpeakers); ++c)
                MixWithGain(pDst->mpSamples + pDst->muStride * c, pSrcRow,
                            mfGains[5 * row + sel[c]], KI_FRAME);
        }
    }

    // Centre/LFE fold when the output layout has 6 channels.
    if (mBase.mbChannelCount == 6)
    {
        const f32 centre = mfCentreGain;          // v18
        f32 *pLfe = pDst->mpSamples + pDst->muStride * 5;
        if (mBase.mbFlag20 == 6)
        {
            CopyWithGain(pLfe, pSrc->mpSamples + pSrc->muStride * 5, centre, KI_FRAME);
        }
        else
        {
            CopyWithGain(pLfe, pSrc->mpSamples, centre, KI_FRAME);
            for (unsigned i = 1; i < static_cast<unsigned>(miNumSources); ++i)
                MixWithGain(pLfe, pSrc->mpSamples + pSrc->muStride * i, centre, KI_FRAME);
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// RampPanOutput @0x82B99340 -- the de-clicked variant of PanOutput. First derive the
// per-gain ramp step (new - old) / 64 for every gain in the matrix, then run the same
// distribute/mix/centre-fold structure through the ramping kernels (start = old gain,
// step = per-64-sample increment).
//   pDst == a2, pSrc == a3, pOldGains == a4 (the pre-recompute gain snapshot), fOldCentreGain == a5.
// ---------------------------------------------------------------------------
int Pan2D1::RampPanOutput(AudioChannelBuffer *pDst, AudioChannelBuffer *pSrc,
                          const f32 *pOldGains, f32 fOldCentreGain)
{
    f32 step[28];
    const int nSources = miNumSources; // v10
    if (nSources > 0)
    {
        const int total = 5 * nSources;
        for (int i = 0; i < total; ++i)
            step[i] = static_cast<f32>((mfGains[i] - pOldGains[i]) * KF_RAMP_STEP);
    }

    const int nSpeakers = miNumSpeakers;                                       // v14
    const f32 centreStep = static_cast<f32>((mfCentreGain - fOldCentreGain) * KF_RAMP_STEP); // v16

    int sel[5];
    if (nSpeakers == 4)
    {
        sel[0] = 0; sel[1] = 2; sel[2] = 3; sel[3] = 4;
    }
    else
    {
        for (int i = 0; i < 5; ++i)
            sel[i] = i;
    }

    f32 *pSrcBase = pSrc->mpSamples; // v18

    // Source channel 0 -> every speaker (ramping copy).
    if (nSpeakers != 0)
    {
        for (unsigned c = 0; c < static_cast<unsigned>(miNumSpeakers); ++c)
            CopyWithGainRamp(pDst->mpSamples + pDst->muStride * c, pSrcBase,
                             pOldGains[sel[c]], step[sel[c]], KI_FRAME);
    }

    // Source channels 1..N -> every speaker (ramping mix).
    if (miNumSources > 1)
    {
        for (unsigned row = 1; row < static_cast<unsigned>(miNumSources); ++row)
        {
            f32 *pSrcRow = pSrc->mpSamples + pSrc->muStride * row; // v27
            for (unsigned c = 0; c < static_cast<unsigned>(miNumSpeakers); ++c)
                MixWithGainRamp(pDst->mpSamples + pDst->muStride * c, pSrcRow,
                                pOldGains[5 * row + sel[c]], step[5 * row + sel[c]], KI_FRAME);
        }
    }

    // Centre/LFE fold (ramping) when the output layout has 6 channels.
    if (mBase.mbChannelCount == 6)
    {
        f32 *pLfe = pDst->mpSamples + pDst->muStride * 5;
        if (mBase.mbFlag20 == 6)
        {
            CopyWithGainRamp(pLfe, pSrc->mpSamples + pSrc->muStride * 5,
                             fOldCentreGain, centreStep, KI_FRAME);
        }
        else
        {
            CopyWithGainRamp(pLfe, pSrc->mpSamples, fOldCentreGain, centreStep, KI_FRAME);
            for (unsigned i = 1; i < static_cast<unsigned>(miNumSources); ++i)
                MixWithGainRamp(pLfe, pSrc->mpSamples + pSrc->muStride * i,
                                fOldCentreGain, centreStep, KI_FRAME);
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Process @0x82B997C8 -- per-frame entry. Compare the seven live emitter attributes against
// the cached snapshot: if unchanged, optionally recompute (when bImmediate) and hard-pan;
// if changed, snapshot the old gains, recompute, refresh the cache, and either ramp (smooth)
// or hard-pan (bImmediate). Finally ping-pong the context's src/dst buffer slots.
//   r3 == this, r4 == ctx, r5 == bImmediate flag.
// ---------------------------------------------------------------------------
int Pan2D1::Process(AudioProcessContext *ctx, bool bImmediate)
{
    AudioChannelBuffer **ppSrcSlot = &ctx->mpSrcBuffer; // v3 (ctx + 0x3000C)
    AudioChannelBuffer **ppDstSlot = &ctx->mpDstBuffer; // v4 (ctx + 0x30010)
    AudioChannelBuffer  *pSrc = *ppSrcSlot;             // v7
    AudioChannelBuffer  *pDst = *ppDstSlot;             // v9

    const bool same = (mfAzimuthDeg  == mfCached[0]) &&
                      (mfRadius      == mfCached[1]) &&
                      (mfWidth       == mfCached[2]) &&
                      (mfSpreadDeg   == mfCached[3]) &&
                      (mfFocus       == mfCached[4]) &&
                      (mfLevel       == mfCached[5]) &&
                      (mfCentreLevel == mfCached[6]);

    if (same)
    {
        if (bImmediate)
        {
            EmitterConfig();
            ComputeLevels();
        }
        PanOutput(pDst, pSrc);
    }
    else
    {
        const int nSources      = miNumSources;   // v18
        const f32 oldCentreGain = mfCentreGain;   // v19 (read before recompute)
        f32 oldGains[42];                          // v22[168]
        if (nSources > 0)
            std::memcpy(oldGains, mfGains, (20u * static_cast<u32>(nSources)) & ~3u);

        EmitterConfig();
        ComputeLevels();

        mfCached[0] = mfAzimuthDeg;
        mfCached[1] = mfRadius;
        mfCached[2] = mfWidth;
        mfCached[3] = mfSpreadDeg;
        mfCached[4] = mfFocus;
        mfCached[5] = mfLevel;
        mfCached[6] = mfCentreLevel;

        if (!bImmediate)
            RampPanOutput(pDst, pSrc, oldGains, oldCentreGain);
        else
            PanOutput(pDst, pSrc);
    }

    // Ping-pong the src/dst buffer slots for the next stage.
    AudioChannelBuffer *tmp = *ppSrcSlot;
    *ppSrcSlot = *ppDstSlot;
    *ppDstSlot = tmp;
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
