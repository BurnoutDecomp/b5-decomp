#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // fabs

// ===========================================================================
// rw::collision::GPBox -- the box primitive's VolumeMethods callbacks,
// reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   GPBox::GetMaximumFeature  @ 0x82BA8918   (VolumeMethods +0xA4)
//   GPBox::GetInterval        @ 0x82BA8650   (VolumeMethods +0xA8)
//   GPBox::GetIntervals       @ 0x82BA9238   (VolumeMethods +0xAC)
//
// DecFIGS and the actual X360 entry agree on non-static const members.
// GetMaximumFeature receives this in r3, winding in r4, direction by value
// in v1 and Feature& in r5. The inherited native Vector3 rows carry their
// genuine mV values; mDimensions contains the half-extents.
//
// GPBox::GetBBox exists in the binary at another address outside this TU and
// is not homed here.
//
// VMX arithmetic is retained through the real XDK intrinsic type/operations.
// The wall cross products and their two fused reciprocal-root refinements
// preserve the original rounding boundaries and all four lanes.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    inline __vector4 SplatValue(f32 value)
    {
        return __vspltw(__lvlx(&value, 0), 0);
    }

    // ARTIST's lvsl/vspltw/vperm dynamic dimension-lane selection.
    inline __vector4 SplatLane(__vector4 value, s32 lane)
    {
        const __vector4 control = __vspltw(__lvsl(0, 4 * lane), 0);
        return __vperm(value, value, control);
    }

    inline __vector4 FlipSigns(__vector4 value)
    {
        const __vector4 ones = __vspltisw(-1);
        return __vxor(value, __vslw(ones, ones));
    }

    // ARTIST82BA8B40..4C / 8BD8..8BE8: the rounded product precedes
    // the fused subtraction, and the final permutation includes lane W.
    inline __vector4 CrossNative(__vector4 a, __vector4 b)
    {
        const __vector4 ayzx = __vpermwi(a, 0x63);
        const __vector4 byzx = __vpermwi(b, 0x63);
        const __vector4 product = __vmulfp(a, byzx);
        return __vpermwi(__vnmsubfp(ayzx, b, product), 0x63);
    }

    // ARTIST82BA8B9C..8BC0 and8CBC..8CE0. Preserve both ordered
    // fused corrections; a scalar sqrt or one refinement changes bits.
    inline __vector4 NormaliseWall(__vector4 wall)
    {
        const __vector4 square = __vmsum3fp(wall, wall);
        if (square.x > 1.1920929e-07f)
        {
            const __vector4 value = __vspltw(square, 0);
            const __vector4 one = __vcfsx(__vspltisw(1), 0);
            const __vector4 half = __vcfsx(__vspltisw(1), 1);
            __vector4 estimate = __vrsqrtefp(value);
            const __vector4 squaredEstimate = __vmulfp(estimate, estimate);
            const __vector4 halfEstimate = __vmulfp(estimate, half);
            const __vector4 correction = __vnmsubfp(value, squaredEstimate, one);
            estimate = __vmaddfp(halfEstimate, correction, estimate);
            const __vector4 squaredRefined = __vmulfp(estimate, estimate);
            const __vector4 halfRefined = __vmulfp(estimate, half);
            const __vector4 refinedCorrection = __vnmsubfp(value, squaredRefined, one);
            estimate = __vmaddfp(halfRefined, refinedCorrection, estimate);
            wall = __vmulfp(wall, estimate);
        }
        return wall;
    }
}

// flt_82004744: the |dot| classification gate of the axis loop (value from
// the export's pseudocode literal).
static const f32 KF_FACE_DOT_THRESHOLD = 0.2f;

// ARTIST image data 0x82F91794..0x82F917EB: ordered corner
// parameters, corner indices and other-axis pairs. These values are read
// directly from the retail image, including their tie-breaking order.

static const f32 KAF_CORNER_PARAMS[4][2] =
{
    {  1.0f,  1.0f },
    {  1.0f, -1.0f },
    { -1.0f, -1.0f },
    { -1.0f,  1.0f }
};

static const s32 KAI_CORNER_INDICES[4][2] =
{
    { 0, 0 },   // (+u, +v)
    { 0, 1 },   // (+u, -v)
    { 1, 1 },   // (-u, -v)
    { 1, 0 }    // (-u, +v)
};

static const s32 KAI_OTHER_AXES[3][2] =
{
    { 1, 2 },
    { 0, 2 },
    { 0, 1 }
};

// ===========================================================================
// rw::collision::GPBox::GetMaximumFeature @ 0x82BA8918
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Phase 1 classifies dir against the three face axes: |dot3(dir, fn[i])| <
// 0.2 counts the axis as "perpendicular". Two perpendicular axes -> a FACE
// feature along the aligned axis (4-edge ring); one -> the EDGE parallel to
// it (best of the 4 edge midpoints); zero/three -> the best VERTEX of 8.
// All cases end with region = 0.
// ===========================================================================
void GPBox::GetMaximumFeature(RwBool abCcw, math::vpu::Vector3 arDir,
                              Feature& arFeature) const
{
    s32 liPerpCount = 0;
    s32 liPerpAxis = 0;
    s32 liAlignedAxis = 0;
    for (s32 li = 0; li < 3; ++li)
    {
        const f32 lfDot = __vmsum3fp(arDir.mV, mFaceNormals[li].mV).x;
        // ARTIST82BA8980 is not-less, including an unordered comparison.
        if (!(std::fabs(lfDot) < KF_FACE_DOT_THRESHOLD))
            liAlignedAxis = li;
        else
        {
            ++liPerpCount;
            liPerpAxis = li;
        }
    }

    if (liPerpCount == 2)
    {
        const __vector4 axis = mFaceNormals[liAlignedAxis].mV;
        f32 lfSign = __vmsum3fp(axis, arDir.mV).x > 0.0f ? 1.0f : -1.0f;
        const __vector4 faceOffset = __vmulfp(SplatLane(mDimensions.mV, liAlignedAxis), SplatValue(lfSign));
        const __vector4 faceCentre = __vmaddfp(axis, faceOffset, mPos.mV);
        if (liAlignedAxis == 1)
            lfSign = -lfSign;

        const s32 liAxisU = KAI_OTHER_AXES[liAlignedAxis][0];
        const s32 liAxisV = KAI_OTHER_AXES[liAlignedAxis][1];
        const __vector4 fnU = mFaceNormals[liAxisU].mV;
        const __vector4 fnV = mFaceNormals[liAxisV].mV;
        const __vector4 dimU = SplatLane(mDimensions.mV, liAxisU);
        const __vector4 dimV = SplatLane(mDimensions.mV, liAxisV);
        __vector4 offsetU[2];
        offsetU[0] = __vmulfp(fnU, dimU);
        offsetU[1] = __vmulfp(fnU, FlipSigns(dimU));
        __vector4 offsetV[2];
        offsetV[0] = __vmulfp(fnV, dimV);
        offsetV[1] = __vmulfp(fnV, FlipSigns(dimV));

        __vector4 edgeDir[4];
        edgeDir[0] = FlipSigns(fnV);
        edgeDir[1] = FlipSigns(fnU);
        edgeDir[2] = fnV;
        edgeDir[3] = fnU;

        const __vector4 wallV = NormaliseWall(abCcw ? CrossNative(fnV, arDir.mV)
                                                   : CrossNative(arDir.mV, fnV));
        const __vector4 wallU = NormaliseWall(abCcw ? CrossNative(fnU, arDir.mV)
                                                   : CrossNative(arDir.mV, fnU));
        __vector4 edgePn[4];
        edgePn[0] = FlipSigns(wallV);
        edgePn[1] = FlipSigns(wallU);
        edgePn[2] = wallV;
        edgePn[3] = wallU;

        const __vector4 two = SplatValue(2.0f);
        const __vector4 lengthV = __vmulfp(dimV, two);
        const __vector4 lengthU = __vmulfp(dimU, two);
        const s32 liSignBit = lfSign < 0.0f ? 1 : 0;
        if (liSignBit == abCcw)
        {
            for (s32 lk = 0; lk < 4; ++lk)
            {
                FeatureEdge& edge = arFeature.edges[lk];
                const s32 liU = KAI_CORNER_INDICES[lk][0];
                const s32 liV = KAI_CORNER_INDICES[lk][1];
                edge.base.mV = __vaddfp(__vaddfp(faceCentre, offsetU[liU]), offsetV[liV]);
                edge.dir.mV = edgeDir[lk];
                edge.pn.mV = edgePn[lk];
                edge.length.mV = (lk & 1) ? lengthU : lengthV;
            }
        }
        else
        {
            for (s32 lk = 0; lk < 4; ++lk)
            {
                FeatureEdge& edge = arFeature.edges[3 - lk];
                const s32 liPair = (lk + 1) & 3;
                const s32 liRing = (lk + 2) & 3;
                const s32 liU = KAI_CORNER_INDICES[liPair][0];
                const s32 liV = KAI_CORNER_INDICES[liPair][1];
                edge.base.mV = __vaddfp(__vaddfp(faceCentre, offsetU[liU]), offsetV[liV]);
                edge.dir.mV = edgeDir[liRing];
                edge.pn.mV = edgePn[liRing];
                edge.length.mV = (lk & 1) ? lengthU : lengthV;
            }
        }

        arFeature.numedges = 4;
        arFeature.ownNormal.mV = __vmulfp(axis, SplatValue(-lfSign));
    }
    else if (liPerpCount == 1)
    {
        const s32 liAxisU = KAI_OTHER_AXES[liPerpAxis][0];
        const s32 liAxisV = KAI_OTHER_AXES[liPerpAxis][1];
        const __vector4 fnU = mFaceNormals[liAxisU].mV;
        const __vector4 fnV = mFaceNormals[liAxisV].mV;
        const __vector4 dimU = SplatLane(mDimensions.mV, liAxisU);
        const __vector4 dimV = SplatLane(mDimensions.mV, liAxisV);
        s32 liBest = -1;
        f32 lfBestScore = 0.0f;
        __vector4 bestPoint = __vspltisw(0);
        for (s32 lk = 0; lk < 4; ++lk)
        {
            const __vector4 du = __vmulfp(dimU, SplatValue(KAF_CORNER_PARAMS[lk][0]));
            const __vector4 dv = __vmulfp(dimV, SplatValue(KAF_CORNER_PARAMS[lk][1]));
            const __vector4 pointU = __vmaddfp(fnU, du, mPos.mV);
            const __vector4 point = __vmaddfp(fnV, dv, pointU);
            const f32 lfScore = __vmsum3fp(point, arDir.mV).x;
            if (liBest < 0 || lfScore > lfBestScore)
            {
                bestPoint = point;
                lfBestScore = lfScore;
                liBest = lk;
            }
        }

        const __vector4 axis = mFaceNormals[liPerpAxis].mV;
        const __vector4 half = SplatLane(mDimensions.mV, liPerpAxis);
        const math::vpu::Vector3 pointPlus(__vmaddfp(axis, half, bestPoint));
        const math::vpu::Vector3 pointMinus(__vsubfp(bestPoint, __vmulfp(axis, half)));
        const FeatureEdge edge(pointMinus, pointPlus);
        arFeature.edges[0] = edge;
        arFeature.numedges = 1;
    }
    else
    {
        s32 liBest = -1;
        f32 lfBestScore = 0.0f;
        __vector4 bestPoint = __vspltisw(0);
        const __vector4 dimX = __vspltw(mDimensions.mV, 0);
        const __vector4 dimY = __vspltw(mDimensions.mV, 1);
        const __vector4 dimZ = __vspltw(mDimensions.mV, 2);
        for (s32 lk = 0; lk < 8; ++lk)
        {
            const s32 liPair = lk & 3;
            const __vector4 dy = __vmulfp(dimY, SplatValue(KAF_CORNER_PARAMS[liPair][0]));
            const __vector4 dz = __vmulfp(dimZ, SplatValue(KAF_CORNER_PARAMS[liPair][1]));
            const __vector4 y = __vmulfp(mFaceNormals[1].mV, dy);
            const __vector4 z = __vmulfp(mFaceNormals[2].mV, dz);
            const __vector4 dx = __vmulfp(dimX, SplatValue(lk <= 3 ? -1.0f : 1.0f));
            const __vector4 pointX = __vmaddfp(mFaceNormals[0].mV, dx, mPos.mV);
            const __vector4 point = __vaddfp(__vaddfp(pointX, y), z);
            const f32 lfScore = __vmsum3fp(point, arDir.mV).x;
            if (liBest < 0 || lfScore > lfBestScore)
            {
                bestPoint = point;
                lfBestScore = lfScore;
                liBest = lk;
            }
        }
        arFeature.numedges = 0;
        arFeature.pt.mV = bestPoint;
    }
    arFeature.region = 0;
}

// ===========================================================================
// rw::collision::GPBox::GetInterval @ 0x82BA8650
//
// Projection interval of an oriented box onto a direction:
//   centre = dot3(mPos, dir)
//   radius = (|dims.x * dot3(fn0, dir)| + |dims.y * dot3(fn1, dir)|)
//          + |dims.z * dot3(fn2, dir)|              (vandc fabs; this fold order)
//   interval.min = centre - radius, interval.max = centre + radius
// ===========================================================================
void GPBox::GetInterval(math::vpu::Vector3 arDir, Interval& arInterval) const
{
    const __vector4 centre = __vmsum3fp(mPos.mV, arDir.mV);
    arInterval.max.mV = centre;
    arInterval.min.mV = centre;
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 x = __vandc(__vmulfp(__vspltw(mDimensions.mV, 0), __vmsum3fp(mFaceNormals[0].mV, arDir.mV)), sign);
    const __vector4 y = __vandc(__vmulfp(__vspltw(mDimensions.mV, 1), __vmsum3fp(mFaceNormals[1].mV, arDir.mV)), sign);
    const __vector4 z = __vandc(__vmulfp(__vspltw(mDimensions.mV, 2), __vmsum3fp(mFaceNormals[2].mV, arDir.mV)), sign);
    const __vector4 extent = __vaddfp(__vaddfp(x, y), z);
    arInterval.min.mV = __vsubfp(centre, extent);
    arInterval.max.mV = __vaddfp(centre, extent);
}

// ===========================================================================
// rw::collision::GPBox::GetIntervals @ 0x82BA9238
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals.
//
// Batched form of GetInterval: one projection interval per direction, with
// the three half-extent projection rows (face normal i * dimensions lane i)
// hoisted out of the loop (vspltw + vmulfp128 before the count test).
// ===========================================================================
void GPBox::GetIntervals(const math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    const __vector4 x = __vmulfp(mFaceNormals[0].mV, __vspltw(mDimensions.mV, 0));
    const __vector4 y = __vmulfp(mFaceNormals[1].mV, __vspltw(mDimensions.mV, 1));
    const __vector4 z = __vmulfp(mFaceNormals[2].mV, __vspltw(mDimensions.mV, 2));
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    for (u32 i = 0; i < auNumDirs; ++i)
    {
        const __vector4 direction = lapDirs[i].mV;
        const __vector4 projection = __vmsum3fp(direction, mPos.mV);
        const __vector4 px = __vandc(__vmsum3fp(x, direction), sign);
        const __vector4 py = __vandc(__vmsum3fp(y, direction), sign);
        const __vector4 pz = __vandc(__vmsum3fp(z, direction), sign);
        const __vector4 extent = __vaddfp(__vaddfp(px, py), pz);
        lapIntervals[i].min.mV = __vsubfp(projection, extent);
        lapIntervals[i].max.mV = __vaddfp(projection, extent);
    }
}

void GPBox::GetBBox(AABBox&) const {} // ARTIST ICF 0x82AD5078

} // namespace collision
} // namespace rw
