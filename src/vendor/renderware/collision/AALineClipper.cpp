#include "vendor/renderware/collision/AALineClipper.hpp"

#include <cmath>

// ===========================================================================
// rw::collision::AALineClipper -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   AALineClipper::AALineClipper  @ 0x82BAE3C8
//   AALineClipper::Init           @ 0x828AED60
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following the project's
// FeatureEdge / Triangle4 precedent this is a SEMANTIC reconstruction lowered to
// portable scalar float maths, preserving the store ORDER and offsets. See the
// header for the layout and the flagged inferred .rdata constants.
// ===========================================================================

namespace rw
{
namespace collision
{

// .rdata constants reached only by reference in the asm (no value in the
// export). FLAGGED as inferred:
//   * flt_820F2708 -- the per-axis robustness pad epsilon: the segment delta is
//     scaled by it before being folded into the clip padding.
//   * flt_820AD47C -- the absolute-extent epsilon: max(|boxMin|,|boxMax|,
//     |start|) is scaled by it to floor the pad against catastrophic
//     cancellation on near-degenerate axes.
//   * flt_82001C98 / flt_820037C8 -- the fsel sign-select pair: per lane, when
//     the scaled delta is >= 0 the asm selects flt_82001C98, else flt_820037C8.
//     These encode the +/- clip direction; modelled as +1 / -1.
const f32 AALineClipper::KF_PAD_EPSILON = 1.0e-6f;   // matches the ctor's 0.000001
const f32 AALineClipper::KF_ABS_EPSILON = 1.0e-6f;
const f32 AALineClipper::KF_FSEL_POS    = 1.0f;
const f32 AALineClipper::KF_FSEL_NEG    = -1.0f;

namespace
{
    inline f32 AbsF(f32 lfV) { return std::fabs(lfV); }

    // fsel f, f, pos, neg  ==  (f >= 0) ? pos : neg   (PPC fsel semantics).
    inline f32 Fsel(f32 lfCond, f32 lfPos, f32 lfNeg)
    {
        return (lfCond >= 0.0f) ? lfPos : lfNeg;
    }
}

// ---------------------------------------------------------------------------
// AALineClipper::Init @ 0x828AED60
//
// Per axis (one VMX lane each), the asm computes:
//   delta        = end - start                         (vsubfp v13,v2,v1)
//   scaledDelta  = delta * KF_PAD_EPSILON              (vmulfp v13,v13,padeps)
//   absExtent    = max(|boxMin|, |boxMax|, |start|)    (vandc/vmaxfp)
//   floorPad     = absExtent * KF_ABS_EPSILON          (vmulfp v13,v13,abseps)
//   pad          = max(|scaledDelta|, floorPad)        (vmaxfp v13,v0,v13)
//   sign         = (scaledDelta >= 0) ? +1 : -1        (fsel sign-select)
//   signedPad    = sign * (pad - |scaledDelta|)        (vmulfp v0,v10,v12)
//   clipMin      = start - signedPad                   -> this+0x00
//   clipSpan     = (end + signedPad) - clipMin         -> this+0x10  (== mClipMax slot)
//   recipSpan    = 1 / clipSpan  (vrefp + 2 NR steps)  -> this+0x20
//   padExtent    = dir + (pad - |scaledDelta|)         -> this+0x30
// The w lane follows the same per-lane maths (the asm operates on all 4 lanes).
// ---------------------------------------------------------------------------
AALineClipper* AALineClipper::Init(const Vec4& rStart, const Vec4& rEnd,
                                   const Vec4& rDir, const Aabb* lpBox)
{
    const f32 lStart[4] = { rStart.x, rStart.y, rStart.z, rStart.w };
    const f32 lEnd[4]   = { rEnd.x,   rEnd.y,   rEnd.z,   rEnd.w   };
    const f32 lDir[4]   = { rDir.x,   rDir.y,   rDir.z,   rDir.w   };
    const f32 lBoxMin[4] = { lpBox->mMin.x, lpBox->mMin.y, lpBox->mMin.z, lpBox->mMin.w };
    const f32 lBoxMax[4] = { lpBox->mMax.x, lpBox->mMax.y, lpBox->mMax.z, lpBox->mMax.w };

    f32 lClipMin[4];
    f32 lClipSpan[4];
    f32 lRecipSpan[4];
    f32 lPadExtent[4];

    for (int i = 0; i < 4; ++i)
    {
        const f32 lfDelta       = lEnd[i] - lStart[i];
        const f32 lfScaledDelta = lfDelta * KF_PAD_EPSILON;

        const f32 lfAbsExtent = std::fmax(std::fmax(AbsF(lBoxMin[i]), AbsF(lBoxMax[i])),
                                          AbsF(lStart[i]));
        const f32 lfFloorPad  = lfAbsExtent * KF_ABS_EPSILON;
        const f32 lfPad       = std::fmax(AbsF(lfScaledDelta), lfFloorPad);

        const f32 lfSign      = Fsel(lfScaledDelta, KF_FSEL_POS, KF_FSEL_NEG);
        const f32 lfDeltaPad  = lfPad - AbsF(lfScaledDelta);
        const f32 lfSignedPad = lfSign * lfDeltaPad;

        lClipMin[i]   = lStart[i] - lfSignedPad;                       // +0x00
        lClipSpan[i]  = (lEnd[i] + lfSignedPad) - lClipMin[i];         // +0x10
        lRecipSpan[i] = (lClipSpan[i] != 0.0f) ? (1.0f / lClipSpan[i]) // +0x20 (vrefp + NR)
                                               : 0.0f;
        lPadExtent[i] = lDir[i] + lfDeltaPad;                          // +0x30
    }

    mClipMin   = { lClipMin[0],   lClipMin[1],   lClipMin[2],   lClipMin[3]   };
    mClipMax   = { lClipSpan[0],  lClipSpan[1],  lClipSpan[2],  lClipSpan[3]  };
    mRecipSpan = { lRecipSpan[0], lRecipSpan[1], lRecipSpan[2], lRecipSpan[3] };
    mPadExtent = { lPadExtent[0], lPadExtent[1], lPadExtent[2], lPadExtent[3] };

    return this;
}

// ---------------------------------------------------------------------------
// AALineClipper::AALineClipper @ 0x82BAE3C8
//
// The ctor SYNTHESISES Init's dir-seed (v3) rather than receiving it: the asm
// loads KF_ABS_EPSILON (flt_820AD47C) into a splatted vector, clears the sign
// bit of each endpoint (vandc -> per-lane fabs), takes the per-lane max of the
// two, and multiplies by the epsilon:
//     vandc v12,v2 ; vandc v0,v1 ; vmaxfp v0,v0,v12 ; vmulfp128 v3,v0,v13
//     seed.lane = max(|start.lane|, |end.lane|) * KF_ABS_EPSILON
// It then calls Init(this, start, end, seed, box). (Init genuinely takes v3 as a
// parameter -- e.g. KdTreeLineQuery @0x828AEE80 passes its own -- but the ctor
// builds it here.)
// ---------------------------------------------------------------------------
AALineClipper::AALineClipper(const Vec4& rStart, const Vec4& rEnd,
                             const Aabb* lpBox)
{
    Vec4 lSeed;
    lSeed.x = std::fmax(AbsF(rStart.x), AbsF(rEnd.x)) * KF_ABS_EPSILON;
    lSeed.y = std::fmax(AbsF(rStart.y), AbsF(rEnd.y)) * KF_ABS_EPSILON;
    lSeed.z = std::fmax(AbsF(rStart.z), AbsF(rEnd.z)) * KF_ABS_EPSILON;
    lSeed.w = std::fmax(AbsF(rStart.w), AbsF(rEnd.w)) * KF_ABS_EPSILON;

    Init(rStart, rEnd, lSeed, lpBox);
}

} // namespace collision
} // namespace rw
