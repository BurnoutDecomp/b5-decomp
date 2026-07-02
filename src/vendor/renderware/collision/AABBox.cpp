//   b5-decomp/src/vendor/renderware/collision/AABBox.cpp
// ===========================================================================
#include "vendor/renderware/collision/AABBox.hpp"

#include <cmath>

// ===========================================================================
// rw::collision::AABBox -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   AABBox::Transform  @ 0x828B6788
//
// The X360 body is hand-vectorised VMX128. Following the committed
// rw::math::vpu precedent (matrix44_operation_platform_inline.h Mult) it is
// lowered to per-lane scalar float maths with the exact accumulation order of
// the vmaddfp chain; each lowered intrinsic is named at its use site.
// ===========================================================================

namespace rw
{
namespace collision
{

// flt_820F2708. Value pinned structurally by this function's dataflow (see the
// header note): result = K*(max+min) transformed +/- K*(max-min) spread must
// reproduce the NULL-transform copy under the identity matrix => K = 0.5f.
const f32 AABBox::KF_HALF = 0.5f;

namespace
{
    // vandc v, signmask -- clear the lane sign bit (vspltisw v0,-1 ; vslw v0,v0,v0
    // builds the 0x80000000 mask). Identical to fabsf for every bit pattern.
    inline f32 AbsF(f32 lfV) { return std::fabs(lfV); }
}

// ---------------------------------------------------------------------------
// AABBox::Transform @ 0x828B6788
//
// r5 == NULL branch (cmplwi cr6,r5,0 falls through): a plain 32-byte
// ld/std x4 copy of {mMin, mMax} into the return slot.
//
// Transform branch, per VMX lane k (all four lanes, W included):
//   centre     = (max + min) * 0.5                (vaddfp / vmulfp128 v13)
//   half       = (max - min) * 0.5                (vsubfp / vmulfp128 v12)
//   extent[k]  =              |yAxis[k]| * half.y (vmulfp128 v8 after vspltw)
//   extent[k]  = |xAxis[k]| * half.x + extent[k]  (vmaddfp v8,v4,v8,v6)
//   extent[k]  = |zAxis[k]| * half.z + extent[k]  (vmaddfp v0,v0,v8,v12)
//   centre'[k] =  xAxis[k] * centre.x + wAxis[k]  (vmaddfp v12,v11,v7,v6)
//   centre'[k] =  yAxis[k] * centre.y + centre'[k](vmaddfp v12,v10,v12,v5)
//   centre'[k] =  zAxis[k] * centre.z + centre'[k](vmaddfp v13,v9,v12,v13)
//   result.min = centre' - extent                 (vsubfp, stored first)
//   result.max = centre' + extent                 (vaddfp, stored second)
// The row |.| is the vandc sign-bit clear; the half/centre broadcasts are
// vspltw lane splats of KF_HALF-scaled sums.
// ---------------------------------------------------------------------------
AABBox AABBox::Transform(const math::vpu::Matrix44Affine* lpTransform) const
{
    AABBox lResult;

    if (lpTransform == 0)
    {
        lResult = *this;   // ld/std x4: verbatim 32-byte copy
        return lResult;
    }

    const f32* lafMin = mMin.mV.mafLane;
    const f32* lafMax = mMax.mV.mafLane;

    // Matrix rows: lvx128 [r5+0x00/0x10/0x20/0x30] = xAxis/yAxis/zAxis/wAxis.
    const f32* lafRowX = lpTransform->xAxis.mV.mafLane;
    const f32* lafRowY = lpTransform->yAxis.mV.mafLane;
    const f32* lafRowZ = lpTransform->zAxis.mV.mafLane;
    const f32* lafRowW = lpTransform->wAxis.mV.mafLane;

    // centre = (max+min)*0.5 (vaddfp+vmulfp128); half = (max-min)*0.5
    // (vsubfp+vmulfp128); KF_HALF splatted from flt_820F2708 via lvlx+vspltw.
    f32 lCentre[4];
    f32 lHalf[4];
    for (int i = 0; i < 4; ++i)
    {
        lCentre[i] = (lafMax[i] + lafMin[i]) * KF_HALF;
        lHalf[i]   = (lafMax[i] - lafMin[i]) * KF_HALF;
    }

    // vspltw broadcasts of the x/y/z lanes feeding the row combines.
    const f32 lfCx = lCentre[0];
    const f32 lfCy = lCentre[1];
    const f32 lfCz = lCentre[2];
    const f32 lfHx = lHalf[0];
    const f32 lfHy = lHalf[1];
    const f32 lfHz = lHalf[2];

    f32 lNewMin[4];
    f32 lNewMax[4];
    for (int i = 0; i < 4; ++i)
    {
        // |row| * half accumulation, in asm order (y, then x, then z).
        f32 lfExtent = AbsF(lafRowY[i]) * lfHy;           // vmulfp128 v8
        lfExtent = AbsF(lafRowX[i]) * lfHx + lfExtent;    // vmaddfp  v8
        lfExtent = AbsF(lafRowZ[i]) * lfHz + lfExtent;    // vmaddfp  v0

        // row * centre accumulation seeded with the translation row.
        f32 lfCentre = lafRowX[i] * lfCx + lafRowW[i];    // vmaddfp v12
        lfCentre = lafRowY[i] * lfCy + lfCentre;          // vmaddfp v12
        lfCentre = lafRowZ[i] * lfCz + lfCentre;          // vmaddfp v13

        lNewMin[i] = lfCentre - lfExtent;                 // vsubfp v12
        lNewMax[i] = lfCentre + lfExtent;                 // vaddfp v0
    }

    // stvx128 min row then max row into the return slot.
    for (int i = 0; i < 4; ++i)
    {
        lResult.mMin.mV.mafLane[i] = lNewMin[i];
        lResult.mMax.mV.mafLane[i] = lNewMax[i];
    }

    return lResult;
}

} // namespace collision
} // namespace rw
