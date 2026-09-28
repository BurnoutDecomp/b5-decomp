#include "vendor/renderware/collision/AALineClipper.hpp"

#include "vendor/renderware/collision/LineSegKernelMath.hpp"   // RefinedRecip / VmxMax / Fsel: the console's idioms

// ===========================================================================
// rw::collision::AALineClipper -- from BURNOUT_X360_ARTIST.XEX, lane by lane as the console computes it (2026-09-28,
// owner's list, lane L2 CAMCOLLIDE, stage (c): CylinderVolume::FatLineSegIntersect @0x82BAEB10 builds one for its
// torus arm; KdTreeLineQuery @0x828AEE80 calls Init).
//
//   AALineClipper::AALineClipper  @ 0x82BAE3C8
//   AALineClipper::Init           @ 0x828AED60
//
// It was a "semantic" reconstruction with GUESSED .rdata words: flt_820F2708, the scale of the segment's delta, was
// taken as a 1e-6 "pad epsilon" -- the image holds 0x3F000000 (0.5) -- and the reciprocal was an exact divide with an
// invented zero guard. Now every step is the console's instruction, rounded as the console rounds it
// (scratch/CRASHPARITY_0922/ROUNDING_RULE.md): vmulfp128 / vsubfp / vaddfp separately (rule 4); vrefp at its
// correctly rounded value, its two Newton steps (vnmsubfp / vmaddfp) once each (rules 5 / 3: linemath::RefinedRecip);
// vmaxfp keeps a NaN and orders -0 below +0 (linemath::VmxMax). FLAG (rule 6): the VMX denormal flush is not
// modelled.
// ===========================================================================

namespace rw
{
namespace collision
{

// The .rdata words (tools/re/x360rd.py).
const f32 AALineClipper::KF_HALF_DELTA  = 0.5f;          // flt_820F2708 == 0x3F000000
const f32 AALineClipper::KF_ABS_EPSILON = 1.0e-6f;       // flt_820AD47C == 0x358637BD
const f32 AALineClipper::KF_FSEL_POS    = 1.0f;          // flt_82001C98 == 0x3F800000
const f32 AALineClipper::KF_FSEL_NEG    = -1.0f;         // flt_820037C8 == 0xBF800000

namespace
{
    // vandc x, (vspltisw -1 ; vslw) == x & 0x7FFFFFFF: the sign bit cleared, a NaN kept.
    inline f32 AbsBits(f32 afX)
    {
        return std::fabs(afX);
    }
}

// ---------------------------------------------------------------------------
// AALineClipper::Init @ 0x828AED60 -- r3 = this, r4 = the box (min +0x00, max +0x10), v1 = start, v2 = end,
// v3 = the seed. Per lane:
//   delta     = end - start                                  vsubfp v13, v2, v1          @0x828AED6C
//   scaled    = delta * 0.5                                  vmulfp128 v13, v13, v10     @0x828AEDBC
//   extent    = max(max(|min|, |max|), |start|)              vmaxfp v12, v12, v11 / vmaxfp v13, v12, v8
//   floor     = extent * 1e-6                                vmulfp128 v13, v13, v9      @0x828AEE10
//   pad       = max(|scaled|, floor)                         vmaxfp v13, v0, v13         @0x828AEE34
//   grow      = pad - |scaled|                               vsubfp v12, v13, v0         @0x828AEE38
//   sign      = scaled >= 0 ? 1 : -1 for x / y / z (fsel via the stack, a NaN takes -1), 0 for w (stw r7 = 0
//               @0x828AEE24)
//   signed    = sign * grow                                  vmulfp128 v0, v10, v12      @0x828AEE3C
//   +0x30     = seed + grow                                  vaddfp v12, v3, v12         @0x828AEE40
//   +0x00     = start - signed (clipMin)                     vsubfp v13, v1, v0          @0x828AEE44
//   +0x10     = (end + signed) - clipMin (the span)          vaddfp / vsubfp             @0x828AEE48 / 0x828AEE50
//   +0x20     = RefinedRecip(span): vrefp + two Newton steps (a span of 0 gives NaN: 1 - inf * 0)
// Returns this.
// ---------------------------------------------------------------------------
AALineClipper* AALineClipper::Init(const Vec4& rStart, const Vec4& rEnd,
                                   const Vec4& rDir, const Aabb* lpBox)
{
    const f32 lStart[4]  = { rStart.x, rStart.y, rStart.z, rStart.w };
    const f32 lEnd[4]    = { rEnd.x,   rEnd.y,   rEnd.z,   rEnd.w   };
    const f32 lSeed[4]   = { rDir.x,   rDir.y,   rDir.z,   rDir.w   };
    const f32 lBoxMin[4] = { lpBox->mMin.x, lpBox->mMin.y, lpBox->mMin.z, lpBox->mMin.w };
    const f32 lBoxMax[4] = { lpBox->mMax.x, lpBox->mMax.y, lpBox->mMax.z, lpBox->mMax.w };

    f32 lClipMin[4];
    f32 lSpan[4];
    f32 lRecipSpan[4];
    f32 lPadExtent[4];

    for (int i = 0; i < 4; ++i)
    {
        const f32 lfDelta     = lEnd[i] - lStart[i];
        const f32 lfScaled    = lfDelta * KF_HALF_DELTA;
        const f32 lfAbsScaled = AbsBits(lfScaled);
        const f32 lfExtent    = linemath::VmxMax(linemath::VmxMax(AbsBits(lBoxMin[i]), AbsBits(lBoxMax[i])),
                                                 AbsBits(lStart[i]));
        const f32 lfFloor     = lfExtent * KF_ABS_EPSILON;
        const f32 lfPad       = linemath::VmxMax(lfAbsScaled, lfFloor);
        const f32 lfGrow      = lfPad - lfAbsScaled;
        const f32 lfSign      = (i < 3) ? linemath::Fsel(lfScaled, KF_FSEL_POS, KF_FSEL_NEG) : 0.0f;
        const f32 lfSigned    = lfSign * lfGrow;

        lPadExtent[i] = lSeed[i] + lfGrow;                              // +0x30
        lClipMin[i]   = lStart[i] - lfSigned;                           // +0x00
        lSpan[i]      = (lEnd[i] + lfSigned) - lClipMin[i];             // +0x10
        lRecipSpan[i] = linemath::RefinedRecip(lSpan[i]);               // +0x20
    }

    mClipMin   = { lClipMin[0],   lClipMin[1],   lClipMin[2],   lClipMin[3]   };
    mSpan      = { lSpan[0],      lSpan[1],      lSpan[2],      lSpan[3]      };
    mRecipSpan = { lRecipSpan[0], lRecipSpan[1], lRecipSpan[2], lRecipSpan[3] };
    mPadExtent = { lPadExtent[0], lPadExtent[1], lPadExtent[2], lPadExtent[3] };

    return this;
}

// ---------------------------------------------------------------------------
// AALineClipper::AALineClipper @ 0x82BAE3C8 -- r3 = this, r4 = the box, v1 = start, v2 = end. The constructor builds
// Init's seed itself: seed = max(|start|, |end|) * 1e-6 per lane (vandc v0, v1 / vandc v12, v2 ; vmaxfp v0, v0, v12
// @0x82BAE408 ; vmulfp128 v3, v0, v13 @0x82BAE40C with v13 = splat flt_820AD47C), then `bl Init` @0x82BAE410.
// ---------------------------------------------------------------------------
AALineClipper::AALineClipper(const Vec4& rStart, const Vec4& rEnd,
                             const Aabb* lpBox)
{
    Vec4 lSeed;
    lSeed.x = linemath::VmxMax(AbsBits(rStart.x), AbsBits(rEnd.x)) * KF_ABS_EPSILON;
    lSeed.y = linemath::VmxMax(AbsBits(rStart.y), AbsBits(rEnd.y)) * KF_ABS_EPSILON;
    lSeed.z = linemath::VmxMax(AbsBits(rStart.z), AbsBits(rEnd.z)) * KF_ABS_EPSILON;
    lSeed.w = linemath::VmxMax(AbsBits(rStart.w), AbsBits(rEnd.w)) * KF_ABS_EPSILON;

    Init(rStart, rEnd, lSeed, lpBox);
}

} // namespace collision
} // namespace rw
