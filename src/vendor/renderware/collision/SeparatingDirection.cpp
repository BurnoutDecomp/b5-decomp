// Native ARTIST separating-axis family. Geometry values use the genuine
// Vector3/VecFloat ABI. VMX expressions retain their emitted rounding order;
// raw-estimate normalizations intentionally remain raw estimates.
// RimToRim's seven inputs are references (r3-r9), with output in r10.
// The radius class spelling is a machine-ABI-compatible reconstruction from
// native scalar-splat uses; no original DecFIGS rim declaration is available.
// The shared rim guard row is FLT_MIN after the attested CRT initializer.

#include "vendor/renderware/collision/GPInstance.hpp"

namespace rw
{
namespace collision
{

using rw::math::vpu::Vector3;
using rw::math::vpu::VecFloat;

namespace
{
    inline __vector4 Broadcast(f32 value)
    {
        const __vector4 row = {value, value, value, value};
        return row;
    }
    inline f32 Dot3(const Vector3& a, const Vector3& b)
    { return __vmsum3fp(a.mV, b.mV).x; }
    inline Vector3 SepSub(const Vector3& a, const Vector3& b)
    { return Vector3(__vsubfp(a.mV, b.mV)); }
    inline Vector3 SepAdd(const Vector3& a, const Vector3& b)
    { return Vector3(__vaddfp(a.mV, b.mV)); }
    inline Vector3 Scale(const Vector3& a, f32 b)
    { return Vector3(__vmulfp(a.mV, Broadcast(b))); }
    inline Vector3 MaddScalar(const Vector3& a, f32 b, const Vector3& c)
    { return Vector3(__vmaddfp(a.mV, Broadcast(b), c.mV)); }
    inline Vector3 SepNegate(const Vector3& a)
    {
        const __vector4 all = __vspltisw(-1);
        return Vector3(__vxor(a.mV, __vslw(all, all)));
    }
    inline Vector3 SepCross(const Vector3& a, const Vector3& b)
    {
        return Vector3(__vpermwi(__vnmsubfp(__vpermwi(a.mV, 0x63), b.mV,
            __vmulfp(a.mV, __vpermwi(b.mV, 0x63))), 0x63));
    }
    inline __vector4 ReciprocalRefined(__vector4 x)
    {
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        __vector4 e = __vrefp(x);
        e = __vmaddfp(e, __vnmsubfp(e, x, one), e);
        return __vmaddfp(e, __vnmsubfp(e, x, one), e);
    }
}

static const f32 KF_SEP_DIR_EPSILON = 1.1920929e-07f;

namespace
{
    inline Vector3 SATSub(const Vector3& a, const Vector3& b)
    { return Vector3(__vsubfp(a.mV,b.mV)); }
    inline f32 SATDot(const Vector3& a, const Vector3& b)
    { return __vmsum3fp(a.mV,b.mV).x; }
    inline Vector3 SATCross(const Vector3& a, const Vector3& b)
    { return Vector3(__vpermwi(__vnmsubfp(__vpermwi(a.mV,0x63),b.mV,__vmulfp(a.mV,__vpermwi(b.mV,0x63))),0x63)); }
    inline __vector4 SATInvLength(__vector4 x)
    {
        const __vector4 one=__vcfsx(__vspltisw(1),0), half=__vcfsx(__vspltisw(1),1);
        __vector4 e=__vrsqrtefp(x);
        __vector4 c=__vnmsubfp(x,__vmulfp(e,e),one);
        e=__vmaddfp(__vmulfp(e,half),c,e);
        c=__vnmsubfp(x,__vmulfp(e,e),one);
        return __vmaddfp(__vmulfp(e,half),c,e);
    }
    inline Vector3 SATNormalize(const Vector3& a)
    { return Vector3(__vmulfp(a.mV,SATInvLength(__vmsum3fp(a.mV,a.mV)))); }
    inline f32 SATMagnitude(const Vector3& a)
    {
        const __vector4 x=__vmsum3fp(a.mV,a.mV), zero=__vspltisw(0);
        return __vsel(__vmulfp(x,SATInvLength(x)),zero,__vcmpeqfp(zero,x)).x;
    }
}

// ARTIST 0x82BB4038
Vector3* FindBestSepDir_FindCandidates(Vector3* lapCandidates,
                                    const GPInstance& arGP1, const GPInstance& arGP2)
{
    Vector3* lpOut = lapCandidates;

    switch (arGP1.mNumFaceNormals)
    {
        case 3: *lpOut++ = arGP1.mFaceNormals[2];
        case 2: *lpOut++ = arGP1.mFaceNormals[1];
        case 1: *lpOut++ = arGP1.mFaceNormals[0];
        default: break;
    }
    switch (arGP2.mNumFaceNormals)
    {
        case 3: *lpOut++ = arGP2.mFaceNormals[2];
        case 2: *lpOut++ = arGP2.mFaceNormals[1];
        case 1: *lpOut++ = arGP2.mFaceNormals[0];
        default: break;
    }

    for (s32 li1 = 0; li1 < static_cast<s32>(arGP1.mNumEdgeDirections); ++li1)
    {
        for (s32 li2 = 0; li2 < static_cast<s32>(arGP2.mNumEdgeDirections); ++li2)
        {

            const Vector3 lvCross = SATCross(arGP1.mEdgeDirections[li1],
                                       arGP2.mEdgeDirections[li2]);

            *lpOut = lvCross;
            const f32 lfLenSq = SATDot(lvCross, lvCross);
            if (lfLenSq > KF_SEP_DIR_EPSILON)
            {

                *lpOut = SATNormalize(lvCross);
                ++lpOut;
            }
        }
    }

    if (arGP1.mNumEdgeDirections == 1 && arGP2.mNumEdgeDirections == 1)
    {
        const Vector3& lrEdge1 = arGP1.mEdgeDirections[0];
        const Vector3& lrEdge2 = arGP2.mEdgeDirections[0];

        const Vector3 lvCross = SATCross(lrEdge1, lrEdge2);

        const f32 lfCrossLen = SATMagnitude(lvCross);
        if (lfCrossLen > KF_SEP_DIR_EPSILON)
        {
            const Vector3 lvPerp1 = SATCross(lrEdge1, lvCross);
            const Vector3 lvPerp2 = SATCross(lrEdge2, lvCross);

            *lpOut = lvPerp1;
            if (SATMagnitude(lvPerp1) > KF_SEP_DIR_EPSILON)
            {

                *lpOut = SATNormalize(lvPerp1);
                ++lpOut;
            }

            *lpOut = lvPerp2;
            if (SATMagnitude(lvPerp2) > KF_SEP_DIR_EPSILON)
            {
                *lpOut = SATNormalize(lvPerp2);
                ++lpOut;
            }
        }
    }

    if (lpOut != lapCandidates)
    {
        return lpOut;
    }

    if (arGP1.mNumEdgeDirections == 1 && arGP2.mNumEdgeDirections == 1)
    {
        const Vector3& lrEdge  = arGP1.mEdgeDirections[0];
        const Vector3  lvDelta = SATSub(arGP1.mPos, arGP2.mPos);
        const Vector3  lvPerp  = SATCross(SATCross(lvDelta, lrEdge), lrEdge);

        *lpOut = lvPerp;
        if (SATMagnitude(lvPerp) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = SATNormalize(lvPerp);
            ++lpOut;
        }
    }

    if (arGP1.mNumEdgeDirections + arGP2.mNumEdgeDirections == 1)
    {
        const Vector3& lrEdge = (arGP1.mNumEdgeDirections != 0)
                                 ? arGP1.mEdgeDirections[0]
                                 : arGP2.mEdgeDirections[0];
        const Vector3 lvDelta = SATSub(arGP1.mPos, arGP2.mPos);
        const Vector3 lvPerp  = SATCross(SATCross(lvDelta, lrEdge), lrEdge);

        *lpOut = lvPerp;
        if (SATMagnitude(lvPerp) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = SATNormalize(lvPerp);
            ++lpOut;
        }
    }

    if (lpOut != lapCandidates)
    {
        return lpOut;
    }

    {
        const Vector3 lvDelta = SATSub(arGP1.mPos, arGP2.mPos);

        *lpOut = lvDelta;
        if (SATMagnitude(lvDelta) > KF_SEP_DIR_EPSILON)
        {
            *lpOut = SATNormalize(lvDelta);
            ++lpOut;
        }
    }

    return (lpOut != lapCandidates) ? lpOut : 0;
}

// ARTIST 0x82BB4660
VecFloat FindBestSeparatingDirection(Vector3& arBestSepDir,
                                const GPInstance& arGP1, const GPInstance& arGP2)
{
    Vector3 candidates[15];
    Interval intervals1[15], intervals2[15];
    Vector3* end = FindBestSepDir_FindCandidates(candidates, arGP1, arGP2);
    if (!end)
    {
        candidates[0] = Vector3(0.0f, 0.0f, 1.0f);
        end = &candidates[1];
    }
    const s32 count = static_cast<s32>(end - candidates);
    (arGP1.*(arGP1.mMethods.mGetIntervals))(candidates, static_cast<u32>(count), intervals1);
    (arGP2.*(arGP2.mMethods.mGetIntervals))(candidates, static_cast<u32>(count), intervals2);
    f32 bestDistance = 0.0f;
    const Vector3* best = 0;
    bool bestFlip = false;
    for (s32 i = 0; i < count; ++i)
    {
        const f32 flipped = __vsubfp(intervals1[i].min.mV, intervals2[i].max.mV).x;
        const f32 forward = __vsubfp(intervals2[i].min.mV, intervals1[i].max.mV).x;

        const bool flip = flipped > forward;
        const f32 distance = flip ? flipped : forward;
        if (!best || distance > bestDistance)
        { best = &candidates[i]; bestDistance = distance; bestFlip = flip; }
    }
    if (bestFlip)
    {
        const __vector4 mask = __vspltisw(-1);
        arBestSepDir.mV = __vxor(best->mV, __vslw(mask, mask));
    }
    else arBestSepDir = *best;
    const __vector4 result = {bestDistance, bestDistance, bestDistance, bestDistance};
    return VecFloat(result);
}

namespace
{

    inline f32 ComputeBoxAxisSeparation(const Vector3& axis,
                                        const Vector3& p1, const Vector3& p2,
                                        const Vector3* proj1, const Vector3* proj2, f32& sign)
    {
        const __vector4 all=__vspltisw(-1), mask=__vslw(all,all);
        const __vector4 d1=__vmsum3fp(axis.mV,p1.mV), d2=__vmsum3fp(axis.mV,p2.mV);
        const __vector4 a0=__vandc(__vmsum3fp(proj1[0].mV,axis.mV),mask), a1=__vandc(__vmsum3fp(proj1[1].mV,axis.mV),mask), a2=__vandc(__vmsum3fp(proj1[2].mV,axis.mV),mask);
        const __vector4 b0=__vandc(__vmsum3fp(proj2[0].mV,axis.mV),mask), b1=__vandc(__vmsum3fp(proj2[1].mV,axis.mV),mask), b2=__vandc(__vmsum3fp(proj2[2].mV,axis.mV),mask);
        const __vector4 r1=__vaddfp(__vaddfp(a0,a1),a2), r2=__vaddfp(__vaddfp(b0,b1),b2);
        const __vector4 s1=__vsubfp(__vsubfp(d1,r1),__vaddfp(d2,r2));
        const __vector4 s2=__vsubfp(__vsubfp(d2,r2),__vaddfp(d1,r1));
        const __vector4 gt=__vcmpgtfp(s1,s2);
        const __vector4 plus=__vcfsx(__vspltisw(1),0);
        sign=__vsel(plus,__vxor(plus,mask),gt).x;
        return __vsel(s2,s1,gt).x;
    }
}

// ARTIST 0x82BB47F0
VecFloat FindBestSeparatingDirectionBoxBox(Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{

    Vector3 laProj1[3];
    Vector3 laProj2[3];
    laProj1[0] = Vector3(__vmulfp(arGP1.mFaceNormals[0].mV,__vspltw(arGP1.mDimensions.mV,0)));
    laProj1[1] = Vector3(__vmulfp(arGP1.mFaceNormals[1].mV,__vspltw(arGP1.mDimensions.mV,1)));
    laProj1[2] = Vector3(__vmulfp(arGP1.mFaceNormals[2].mV,__vspltw(arGP1.mDimensions.mV,2)));
    laProj2[0] = Vector3(__vmulfp(arGP2.mFaceNormals[0].mV,__vspltw(arGP2.mDimensions.mV,0)));
    laProj2[1] = Vector3(__vmulfp(arGP2.mFaceNormals[1].mV,__vspltw(arGP2.mDimensions.mV,1)));
    laProj2[2] = Vector3(__vmulfp(arGP2.mFaceNormals[2].mV,__vspltw(arGP2.mDimensions.mV,2)));

    Vector3 laCandidates[16];
    laCandidates[0] = arGP1.mFaceNormals[2];
    laCandidates[1] = arGP1.mFaceNormals[1];
    laCandidates[2] = arGP1.mFaceNormals[0];
    laCandidates[3] = arGP2.mFaceNormals[2];
    laCandidates[4] = arGP2.mFaceNormals[1];
    laCandidates[5] = arGP2.mFaceNormals[0];
    for (s32 li = 0; li < 3; ++li)
    {
        for (s32 lj = 0; lj < 3; ++lj)
        {
            const Vector3 lvCross = SATCross(arGP1.mEdgeDirections[li],
                                       arGP2.mEdgeDirections[lj]);

            laCandidates[6 + 3 * li + lj] =
                Vector3(__vmulfp(lvCross.mV, __vrsqrtefp(__vmsum3fp(lvCross.mV, lvCross.mV))));
        }
    }
    laCandidates[15] = laCandidates[14];

    f32  lfBestSign = 0.0f;
    f32  lfBestSep  = ComputeBoxAxisSeparation(laCandidates[0], arGP1.mPos, arGP2.mPos,
                                               laProj1, laProj2, lfBestSign);
    Vector3 lvBestAxis = laCandidates[0];

    for (s32 lu = 0; lu < 16; ++lu)
    {
        f32 lfSign = 0.0f;
        const f32 lfSep = ComputeBoxAxisSeparation(laCandidates[lu], arGP1.mPos, arGP2.mPos,
                                                   laProj1, laProj2, lfSign);
        if (lfSep > lfBestSep)
        {
            lfBestSep  = lfSep;
            lfBestSign = lfSign;
            lvBestAxis = laCandidates[lu];
        }
    }

    const __vector4 sign = {lfBestSign,lfBestSign,lfBestSign,lfBestSign};
    arBestSepDir.mV = __vmulfp(lvBestAxis.mV, sign);
    const __vector4 result = {lfBestSep,lfBestSep,lfBestSep,lfBestSep};
    return VecFloat(result);
}

// ARTIST 0x82BAA1A0
VecFloat FindBestSeparatingDirectionBoxTri(Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{
    const VecFloat separation = FindBestSeparatingDirectionTriBox(arBestSepDir, arGP2, arGP1);
    const __vector4 all = __vspltisw(-1);
    arBestSepDir.mV = __vxor(arBestSepDir.mV, __vslw(all, all));
    return separation;
}

// ARTIST 0x82BAA1F0
VecFloat FindBestSeparatingDirectionSphSph(Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{
    const __vector4 delta = __vsubfp(arGP2.mPos.mV, arGP1.mPos.mV);
    const __vector4 square = __vmsum3fp(delta, delta);
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    __vector4 estimate = __vrsqrtefp(square);
    __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
    estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
    correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
    estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
    const __vector4 magnitude = __vsel(__vmulfp(square, estimate), zero, __vcmpeqfp(zero, square));
    arBestSepDir.mV = __vmulfp(delta, estimate);
    return VecFloat(magnitude);
}

extern u32 FindBestSepDirWithCylinder_FindCandidates(
    Vector3* lapCandidates, const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

extern VecFloat FindBestSepDirWithCylinder_Evaluate(
    Vector3& arBestSepDir, Vector3* lapCandidates, u32 aiCandidateResult,
    const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

// ARTIST 0x82BB76E8
VecFloat FindBestSepDirWithCylinder(Vector3& arBestSepDir,
                               const GPCylinder& arGPCylinder, const GPInstance& arGPOther)
{

    Vector3 laCandidates[17];

    const u32 liCandidateResult =
        FindBestSepDirWithCylinder_FindCandidates(laCandidates, arGPCylinder, arGPOther);

    return FindBestSepDirWithCylinder_Evaluate(arBestSepDir, laCandidates,
                                               liCandidateResult, arGPCylinder, arGPOther);
}

// ARTIST 0x82BAA250
VecFloat FindBestSeparatingDirCylVol(Vector3& arBestSepDir,
                                const GPInstance& arGPCylinder, const GPInstance& arGPOther)
{
    return FindBestSepDirWithCylinder(arBestSepDir,
                                      static_cast<const GPCylinder&>(arGPCylinder),
                                      arGPOther);
}

// ARTIST 0x82BAA258
VecFloat FindBestSeparatingDirVolCyl(Vector3& arBestSepDir,
                                const GPInstance& arGPOther, const GPInstance& arGPCylinder)
{
    const VecFloat lfSeparation =
        FindBestSepDirWithCylinder(arBestSepDir,
                                   static_cast<const GPCylinder&>(arGPCylinder),
                                   arGPOther);

    arBestSepDir = SepNegate(arBestSepDir);

    return lfSeparation;
}

static const f32 KF_RIM_FLT_MIN = 1.1754944e-38f;

static const f32 KF_RIM_DEGENERATE_EPSILON = KF_RIM_FLT_MIN;

static const f32 KF_RIM_RADIUS_TOLERANCE_SCALE = 1.0e-7f;

// ARTIST 0x82BB53F8
Vector3 RimToEdge(Vector3 aRimCentre, Vector3 aRimAxis, VecFloat aRimRadius,
                  Vector3 aEdgeP1, Vector3 aEdgeP2)
{

    __vector4 v0, v1, v2, v3, v4, v5, v6, v7;
    __vector4 v8, v9, v10, v11, v12, v13, v20, v21;
    __vector4 v22, v23, v24, v25, v26, v27, v28, v29;
    __vector4 v30, v31;
    unsigned int cr;
    bool allGreater = false;
    u32 remaining = 0;
    v1 = aRimCentre.mV; v2 = aRimAxis.mV; v3 = aRimRadius.mV;
    v4 = aEdgeP1.mV; v5 = aEdgeP2.mV;
    v11 = __vsubfp(v4, v1);
    v13 = __vspltisw(1);
    v9 = __vsubfp(v5, v1);
    v31 = __vsubfp(v5, v4);
    v30 = v4;
    v0 = __vcfsx(v13, 0);
    v12 = __vcfsx(v13, 1);
    v29 = Broadcast(KF_RIM_DEGENERATE_EPSILON);
    v13 = __vmsum3fp(v11, v2);
    v7 = __vmsum3fp(v9, v2);
    v13 = __vmulfp(v2, v13);
    v10 = __vsubfp(v11, v13);
    v11 = __vmsum3fp(v10, v10);
    v13 = __vrsqrtefp(v11);
    v8 = v11;
    v6 = v11;
    v11 = __vcmpgefp(v11, v29);
    v28 = __vmulfp(v13, v13);
    v26 = __vmulfp(v13, v13);
    v11 = __vnor(v11, v11);
    v27 = __vmulfp(v13, v12);
    v25 = __vmulfp(v13, v12);
    v8 = __vnmsubfp(v8, v28, v0);
    v6 = __vnmsubfp(v6, v26, v0);
    v8 = __vmaddfp(v27, v8, v13);
    v6 = __vmaddfp(v25, v6, v13);
    v13 = __vmulfp(v10, v8);
    v10 = v13;
    v13 = __vmulfp(v13, v6);
    v13 = __vsel(v13, v10, v11);
    v8 = __vmaddfp(v13, v3, v1);
    v13 = __vmulfp(v2, v7);
    v11 = __vsubfp(v8, v4);
    v9 = __vsubfp(v9, v13);
    v11 = __vmsum3fp(v11, v31);
    v10 = __vmsum3fp(v9, v9);
    v13 = __vrsqrtefp(v10);
    v7 = v10;
    v6 = v10;
    v10 = __vcmpgefp(v10, v29);
    v28 = __vmulfp(v13, v13);
    v26 = __vmulfp(v13, v13);
    v10 = __vnor(v10, v10);
    v27 = __vmulfp(v13, v12);
    v25 = __vmulfp(v13, v12);
    v7 = __vnmsubfp(v7, v28, v0);
    v6 = __vnmsubfp(v6, v26, v0);
    v7 = __vmaddfp(v27, v7, v13);
    v6 = __vmaddfp(v25, v6, v13);
    v13 = __vmulfp(v9, v7);
    v9 = v13;
    v13 = __vmulfp(v13, v6);
    v13 = __vsel(v13, v9, v10);
    v9 = __vmaddfp(v13, v3, v1);
    v13 = __vsubfp(v9, v5);
    v10 = __vmsum3fp(v13, v31);
    v13 = __vcmpgtfpR(v10, v11, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5518;
    v13 = v8;
    v7 = v11;
    v8 = v9;
    v11 = v10;
    v30 = v5;
    v5 = v4;
    v9 = v13;
    v10 = v7;
L82BB5518:
    v6 = v0;
    v27 = __vspltisw(0);
    remaining = 10;
L82BB5524:
    v13 = __vsubfp(v11, v10);
    --remaining;
    v7 = __vaddfp(v6, v0);
    v28 = __vmulfp(v6, v12);
    v6 = __vrefp(v13);
    v4 = v13;
    v25 = v13;
    v13 = __vrefp(v7);
    v26 = v7;
    v24 = v7;
    v7 = __vnmsubfp(v6, v4, v0);
    v4 = __vnmsubfp(v13, v26, v0);
    v7 = __vmaddfp(v6, v7, v6);
    v13 = __vmaddfp(v13, v4, v13);
    v6 = __vnmsubfp(v7, v25, v0);
    v4 = __vnmsubfp(v13, v24, v0);
    v7 = __vmaddfp(v7, v6, v7);
    v13 = __vmaddfp(v13, v4, v13);
    v7 = __vmaddfp(v7, v11, v28);
    v13 = __vmulfp(v13, v7);
    v7 = __vsubfp(v0, v13);
    v7 = __vmulfp(v30, v7);
    v7 = __vmaddfp(v5, v13, v7);
    v13 = __vsubfp(v7, v1);
    v6 = __vmsum3fp(v13, v2);
    v6 = __vmulfp(v2, v6);
    v4 = __vsubfp(v13, v6);
    v6 = __vmsum3fp(v4, v4);
    v13 = __vrsqrtefp(v6);
    v26 = v6;
    v25 = v6;
    v6 = __vcmpgefp(v6, v29);
    v23 = __vmulfp(v13, v13);
    v21 = __vmulfp(v13, v13);
    v6 = __vnor(v6, v6);
    v22 = __vmulfp(v13, v12);
    v20 = __vmulfp(v13, v12);
    v26 = __vnmsubfp(v26, v23, v0);
    v25 = __vnmsubfp(v25, v21, v0);
    v26 = __vmaddfp(v22, v26, v13);
    v25 = __vmaddfp(v20, v25, v13);
    v13 = __vmulfp(v4, v26);
    v4 = v13;
    v13 = __vmulfp(v13, v25);
    v13 = __vsel(v13, v4, v6);
    v6 = __vmaddfp(v13, v3, v1);
    v13 = __vsubfp(v6, v7);
    v4 = __vmsum3fp(v13, v31);
    v13 = __vcmpgefp(v4, v27);
    v13 = __vnor(v13, v13);
    v9 = __vsel(v9, v6, v13);
    v8 = __vsel(v6, v8, v13);
    v5 = __vsel(v5, v7, v13);
    v10 = __vsel(v10, v4, v13);
    v30 = __vsel(v7, v30, v13);
    v6 = v28;
    v11 = __vsel(v4, v11, v13);
    if (remaining) goto L82BB5524;
    v13 = __vspltisw(-1);
    v13 = __vslw(v13, v13);
    v13 = __vxor(v10, v13);
    v13 = __vcmpgtfpR(v11, v13, &cr);
    allGreater = (cr & 0x80) != 0;
    v10 = __vpermwi(v2, 0x63);
    v6 = __vpermwi(v31, 0x63);
    if (!allGreater) goto L82BB564C;
    v13 = __vsubfp(v9, v1);
    v11 = __vsubfp(v5, v9);
    v9 = __vmulfp(v13, v10);
    v13 = __vpermwi(v13, 0x63);
    v13 = __vnmsubfp(v13, v2, v9);
    goto L82BB5660;
L82BB564C:
    v13 = __vsubfp(v8, v1);
    v11 = __vsubfp(v30, v8);
    v8 = __vmulfp(v13, v10);
    v13 = __vpermwi(v13, 0x63);
    v13 = __vnmsubfp(v13, v2, v8);
L82BB5660:
    v10 = __vmsum3fp(v11, v11);
    v9 = __vpermwi(v13, 0x63);
    v13 = __vrsqrtefp(v10);
    v5 = v10;
    v8 = __vcmpgefp(v10, v29);
    v10 = __vmulfp(v9, v6);
    v9 = __vpermwi(v9, 0x63);
    v7 = __vmulfp(v13, v12);
    v8 = __vnor(v8, v8);
    v10 = __vnmsubfp(v9, v31, v10);
    v9 = __vmulfp(v13, v13);
    v10 = __vpermwi(v10, 0x63);
    v6 = __vnmsubfp(v5, v9, v0);
    v9 = __vmsum3fp(v10, v10);
    v13 = __vmaddfp(v7, v6, v13);
    v7 = v9;
    v11 = __vmulfp(v11, v13);
    v13 = __vrsqrtefp(v9);
    v9 = __vmulfp(v13, v13);
    v12 = __vmulfp(v13, v12);
    v0 = __vnmsubfp(v7, v9, v0);
    v0 = __vmaddfp(v12, v0, v13);
    v0 = __vmulfp(v10, v0);
    v1 = __vsel(v11, v0, v8);
    return Vector3(v1);
}

// ARTIST 0x82BB56C8
RwBool RimToRim(const Vector3& aRimCentre1, const Vector3& aRimAxis1, const VecFloat& aRimRadius1,
                const Vector3& aRimCentre2, const Vector3& aRimAxis2, const VecFloat& aRimRadius2,
                const Vector3& aSepDir, Vector3* lpCandidate)
{

    __vector4 v0, v1, v2, v3, v4, v5, v6, v7;
    __vector4 v8, v9, v10, v11, v12, v13, v19, v20;
    __vector4 v21, v22, v23, v24, v25, v26, v27, v28;
    __vector4 v29, v30, v31;
    unsigned int cr;
    bool allGreater = false;
    u32 iteration = 0;
    f32 scalarFraction = 0.0f;
    v7 = aRimRadius1.mV;
    v27 = aRimRadius2.mV;
    v13 = __vminfp(v7, v27);
    v0 = Broadcast(KF_RIM_RADIUS_TOLERANCE_SCALE);
    v0 = __vspltw(v0, 0);
    v23 = __vmulfp(v13, v0);
    v12 = Broadcast(KF_RIM_FLT_MIN);
    v12 = __vspltw(v12, 0);
    v0 = __vcmpgtfpR(v12, v23, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5748;
L82BB5740:
    return 0;
L82BB5748:
    v10 = aRimAxis1.mV;
    v9 = __vmulfp(v27, v27);
    v6 = __vpermwi(v10, 0x63);
    v0 = aSepDir.mV;
    v4 = __vpermwi(v0, 0x63);
    v11 = aRimAxis2.mV;
    v31 = __vpermwi(v11, 0x63);
    v8 = aRimCentre1.mV;
    v30 = __vpermwi(v0, 0x63);
    v13 = aRimCentre2.mV;
    v6 = __vmulfp(v0, v6);
    v3 = __vpermwi(v11, 0x63);
    v2 = __vpermwi(v11, 0x63);
    v31 = __vmulfp(v0, v31);
    v12 = __vnmsubfp(v4, v10, v6);
    v6 = __vnmsubfp(v30, v11, v31);
    v12 = __vpermwi(v12, 0x63);
    v6 = __vpermwi(v6, 0x63);
    v12 = __vmulfp(v12, v7);
    v4 = __vmaddfp(v6, v27, v13);
    v30 = __vsubfp(v8, v12);
    v12 = __vsubfp(v30, v13);
    v6 = __vmulfp(v12, v3);
    v12 = __vpermwi(v12, 0x63);
    v12 = __vnmsubfp(v12, v11, v6);
    v12 = __vpermwi(v12, 0x63);
    v6 = v12;
    v12 = __vpermwi(v12, 0x63);
    v12 = __vmulfp(v11, v12);
    v12 = __vnmsubfp(v2, v6, v12);
    v12 = __vpermwi(v12, 0x63);
    v12 = __vmsum3fp(v12, v12);
    v12 = __vcmpgtfpR(v9, v12, &cr);
    allGreater = (cr & 0x80) != 0;
    if (allGreater) goto L82BB5740;
    v2 = __vsubfp(v4, v8);
    v6 = __vpermwi(v10, 0x63);
    v5 = __vpermwi(v10, 0x63);
    v9 = __vmulfp(v7, v7);
    v6 = __vmulfp(v2, v6);
    v3 = __vpermwi(v2, 0x63);
    v12 = __vnmsubfp(v3, v10, v6);
    v12 = __vpermwi(v12, 0x63);
    v6 = v12;
    v12 = __vpermwi(v12, 0x63);
    v12 = __vmulfp(v10, v12);
    v12 = __vnmsubfp(v5, v6, v12);
    v12 = __vpermwi(v12, 0x63);
    v12 = __vmsum3fp(v12, v12);
    v12 = __vcmpgtfpR(v9, v12, &cr);
    allGreater = (cr & 0x80) != 0;
    if (allGreater) goto L82BB5740;
    v12 = __vsubfp(v8, v13);
    v24 = __vspltisw(-1);
    v9 = __vslw(v24, v24);
    v0 = __vmsum3fp(v12, v0);
    v0 = __vandc(v0, v9);
    v0 = __vcmpgtfpR(v23, v0, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB59F0;
    v6 = __vmsum3fp(v2, v10);
    v0 = __vspltisw(1);
    v12 = __vcfsx(v0, 0);
    v9 = __vcfsx(v0, 1);
    v5 = Broadcast(KF_RIM_DEGENERATE_EPSILON);
    v0 = __vmulfp(v10, v6);
    v6 = __vsubfp(v2, v0);
    v10 = __vmsum3fp(v6, v6);
    v0 = __vrsqrtefp(v10);
    v4 = v10;
    v3 = v10;
    v10 = __vcmpgefp(v10, v5);
    v2 = __vmulfp(v0, v0);
    v31 = __vmulfp(v0, v0);
    v10 = __vnor(v10, v10);
    v1 = __vmulfp(v0, v9);
    v30 = __vmulfp(v0, v9);
    v4 = __vnmsubfp(v4, v2, v12);
    v3 = __vnmsubfp(v3, v31, v12);
    v4 = __vmaddfp(v1, v4, v0);
    v3 = __vmaddfp(v30, v3, v0);
    v0 = __vmulfp(v6, v4);
    v6 = v0;
    v0 = __vmulfp(v0, v3);
    v0 = __vsel(v0, v6, v10);
    v8 = __vmaddfp(v0, v7, v8);
    v0 = __vsubfp(v8, v13);
    v10 = __vmsum3fp(v0, v11);
    v11 = __vmulfp(v11, v10);
    v10 = __vsubfp(v0, v11);
    v0 = Broadcast(KF_RIM_FLT_MIN);
    v7 = __vspltw(v0, 0);
    v11 = __vmsum3fp(v10, v10);
    v0 = __vrsqrtefp(v11);
    v6 = v11;
    v4 = v11;
    v11 = __vcmpgefp(v11, v5);
    v5 = __vmulfp(v0, v0);
    v2 = __vmulfp(v0, v0);
    v11 = __vnor(v11, v11);
    v3 = __vmulfp(v0, v9);
    v1 = __vmulfp(v0, v9);
    v6 = __vnmsubfp(v6, v5, v12);
    v5 = __vnmsubfp(v4, v2, v12);
    v6 = __vmaddfp(v3, v6, v0);
    v5 = __vmaddfp(v1, v5, v0);
    v0 = __vmulfp(v10, v6);
    v10 = v0;
    v0 = __vmulfp(v0, v5);
    v0 = __vsel(v0, v10, v11);
    v0 = __vmaddfp(v0, v27, v13);
    v0 = __vsubfp(v0, v8);
    v13 = __vmsum3fp(v0, v0);
    lpCandidate->mV = v0;
    v11 = __vcmpgtfpR(v13, v7, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5990;
    v11 = __vrsqrtefp(v13);
    v10 = v13;
    v8 = v13;
    v13 = __vmulfp(v11, v11);
    v7 = __vmulfp(v11, v9);
    v13 = __vnmsubfp(v10, v13, v12);
    v13 = __vmaddfp(v7, v13, v11);
    v11 = __vmulfp(v13, v13);
    v10 = __vmulfp(v13, v9);
    v12 = __vnmsubfp(v8, v11, v12);
    v13 = __vmaddfp(v10, v12, v13);
    v0 = __vmulfp(v0, v13);
    goto L82BB610C;
L82BB5990:
    v0 = aRimAxis2.mV;
    v13 = aRimAxis1.mV;
    v13 = __vaddfp(v13, v0);
    v0 = aSepDir.mV;
    v11 = v0;
    v0 = __vpermwi(v0, 0x63);
    v0 = __vmulfp(v13, v0);
    v13 = __vpermwi(v13, 0x63);
    v0 = __vnmsubfp(v13, v11, v0);
    v13 = __vpermwi(v0, 0x63);
    v0 = __vmsum3fp(v13, v13);
    v11 = v0;
    v10 = v0;
    v0 = __vrsqrtefp(v0);
    v8 = __vmulfp(v0, v0);
    v7 = __vmulfp(v0, v9);
    v11 = __vnmsubfp(v11, v8, v12);
    v0 = __vmaddfp(v7, v11, v0);
    v11 = __vmulfp(v0, v0);
    v9 = __vmulfp(v0, v9);
    v12 = __vnmsubfp(v10, v11, v12);
    v0 = __vmaddfp(v9, v12, v0);
    v0 = __vmulfp(v13, v0);
    goto L82BB610C;
L82BB59F0:
    v3 = __vaddfp(v7, v27);
    v12 = __vmsum3fp(v12, v12);
    v31 = v8;
    v0 = __vmulfp(v3, v3);
    v0 = __vcmpgtfpR(v0, v12, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5B70;
    v0 = Broadcast(KF_RIM_FLT_MIN);
    v0 = __vspltw(v0, 0);
    v0 = __vcmpgtfpR(v12, v0, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5B70;
    v9 = __vrsqrtefp(v12);
    v5 = __vspltisw(1);
    v6 = __vspltisw(0);
    v0 = __vcfsx(v5, 0);
    v5 = __vcfsx(v5, 1);
    v22 = __vcmpeqfp(v6, v12);
    v26 = __vmulfp(v9, v9);
    v28 = Broadcast(0.5f);
    v28 = __vspltw(v28, 0);
    v1 = __vnmsubfp(v12, v26, v0);
    v26 = __vmulfp(v9, v5);
    v9 = __vmaddfp(v26, v1, v9);
    v1 = __vmulfp(v9, v9);
    v26 = __vmulfp(v9, v5);
    v1 = __vnmsubfp(v12, v1, v0);
    v9 = __vmaddfp(v26, v1, v9);
    v12 = __vmulfp(v12, v9);
    v12 = __vsel(v12, v6, v22);
    v9 = __vaddfp(v12, v3);
    v26 = v12;
    v20 = v12;
    v9 = __vmulfp(v9, v28);
    v3 = __vsubfp(v9, v12);
    v1 = __vsubfp(v9, v7);
    v29 = __vsubfp(v9, v27);
    v9 = __vmulfp(v9, v3);
    v9 = __vmulfp(v9, v1);
    v1 = __vrefp(v12);
    v9 = __vmulfp(v9, v29);
    v29 = v6;
    v3 = __vrsqrtefp(v9);
    v21 = __vcmpeqfp(v6, v9);
    v6 = __vrefp(v7);
    v28 = __vmulfp(v3, v3);
    v25 = __vmulfp(v3, v5);
    v12 = __vnmsubfp(v9, v28, v0);
    v28 = __vnmsubfp(v6, v7, v0);
    v12 = __vmaddfp(v25, v12, v3);
    v3 = Broadcast(2.0f);
    v19 = __vspltw(v3, 0);
    v3 = __vnmsubfp(v1, v26, v0);
    v5 = __vmulfp(v12, v5);
    v3 = __vmaddfp(v1, v3, v1);
    v1 = __vmulfp(v12, v12);
    v1 = __vnmsubfp(v9, v1, v0);
    v5 = __vmaddfp(v5, v1, v12);
    v12 = __vmaddfp(v6, v28, v6);
    v6 = __vnmsubfp(v3, v20, v0);
    v9 = __vmulfp(v9, v5);
    v0 = __vnmsubfp(v12, v7, v0);
    v6 = __vmaddfp(v3, v6, v3);
    v9 = __vsel(v9, v29, v21);
    v0 = __vmaddfp(v12, v0, v12);
    v9 = __vmulfp(v9, v19);
    v9 = __vmulfp(v6, v9);
    v6 = __vsubfp(v30, v8);
    v0 = __vmulfp(v0, v9);
    v31 = __vmaddfp(v6, v0, v31);
L82BB5B70:
    v9 = __vsubfp(v31, v13);
    v5 = __vmsum3fp(v2, v10);
    v12 = __vspltisw(1);
    v21 = __vpermwi(v11, 0x63);
    v0 = __vcfsx(v12, 0);
    v12 = __vcfsx(v12, 1);
    v6 = Broadcast(KF_RIM_DEGENERATE_EPSILON);
    v3 = __vmsum3fp(v9, v11);
    v5 = __vmulfp(v10, v5);
    v1 = __vmulfp(v11, v3);
    v3 = __vsubfp(v2, v5);
    v2 = __vsubfp(v9, v1);
    v5 = __vmsum3fp(v2, v2);
    v9 = __vrsqrtefp(v5);
    v1 = v5;
    v31 = v5;
    v5 = __vcmpgefp(v5, v6);
    v30 = __vmulfp(v9, v9);
    v28 = __vmulfp(v9, v9);
    v5 = __vnor(v5, v5);
    v29 = __vmulfp(v9, v12);
    v26 = __vmulfp(v9, v12);
    v1 = __vnmsubfp(v1, v30, v0);
    v31 = __vnmsubfp(v31, v28, v0);
    v1 = __vmaddfp(v29, v1, v9);
    v31 = __vmaddfp(v26, v31, v9);
    v9 = __vmulfp(v2, v1);
    v2 = v9;
    v9 = __vmulfp(v9, v31);
    v9 = __vsel(v9, v2, v5);
    v5 = __vmsum3fp(v3, v3);
    v2 = __vmaddfp(v9, v27, v13);
    v1 = __vsubfp(v2, v8);
    v31 = v5;
    v9 = __vrsqrtefp(v5);
    v29 = v5;
    v5 = __vcmpgefp(v5, v6);
    v20 = __vsubfp(v2, v13);
    v30 = __vmsum3fp(v1, v10);
    v28 = __vmulfp(v9, v9);
    v26 = __vmulfp(v9, v12);
    v25 = __vmulfp(v9, v9);
    v22 = __vmulfp(v9, v12);
    v31 = __vnmsubfp(v31, v28, v0);
    v29 = __vnmsubfp(v29, v25, v0);
    v30 = __vmulfp(v10, v30);
    v25 = __vsubfp(v4, v13);
    v31 = __vmaddfp(v26, v31, v9);
    v29 = __vmaddfp(v22, v29, v9);
    v22 = __vpermwi(v11, 0x63);
    v1 = __vsubfp(v1, v30);
    v30 = __vnor(v5, v5);
    v3 = __vmulfp(v3, v31);
    v5 = __vmsum3fp(v1, v1);
    v28 = v3;
    v3 = __vmulfp(v3, v29);
    v9 = __vrsqrtefp(v5);
    v31 = v5;
    v3 = __vsel(v3, v28, v30);
    v30 = __vmaddfp(v3, v7, v8);
    v3 = __vmulfp(v9, v9);
    v29 = __vmulfp(v9, v12);
    v28 = __vmulfp(v9, v12);
    v3 = __vnmsubfp(v5, v3, v0);
    v5 = __vcmpgefp(v5, v6);
    v3 = __vmaddfp(v29, v3, v9);
    v29 = __vmulfp(v9, v9);
    v5 = __vnor(v5, v5);
    v3 = __vmulfp(v1, v3);
    v31 = __vnmsubfp(v31, v29, v0);
    v1 = __vsubfp(v30, v13);
    v9 = __vmaddfp(v28, v31, v9);
    v9 = __vmulfp(v3, v9);
    v9 = __vsel(v9, v3, v5);
    v5 = __vpermwi(v1, 0x63);
    v5 = __vmulfp(v11, v5);
    v26 = __vmaddfp(v9, v7, v8);
    v9 = __vnmsubfp(v22, v1, v5);
    v9 = __vpermwi(v9, 0x63);
    v1 = __vmsum3fp(v25, v9);
    v9 = __vsubfp(v26, v13);
    v5 = v9;
    v9 = __vpermwi(v9, 0x63);
    v9 = __vmulfp(v11, v9);
    v9 = __vnmsubfp(v21, v5, v9);
    v9 = __vpermwi(v9, 0x63);
    v5 = __vmsum3fp(v20, v9);
    v9 = Broadcast(0.0f);
    v9 = __vspltw(v9, 0);
    v3 = __vmulfp(v1, v5);
    v9 = __vcmpgtfpR(v3, v9, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5DD0;
    v9 = __vaddfp(v13, v13);
    v4 = __vsubfp(v9, v4);
    v9 = __vsubfp(v4, v8);
    v3 = __vmsum3fp(v9, v10);
    v3 = __vmulfp(v10, v3);
    v1 = __vsubfp(v9, v3);
    v3 = __vmsum3fp(v1, v1);
    v9 = __vrsqrtefp(v3);
    v31 = v3;
    v30 = v3;
    v3 = __vcmpgefp(v3, v6);
    v29 = __vmulfp(v9, v9);
    v25 = __vmulfp(v9, v9);
    v3 = __vnor(v3, v3);
    v28 = __vmulfp(v9, v12);
    v22 = __vmulfp(v9, v12);
    v31 = __vnmsubfp(v31, v29, v0);
    v30 = __vnmsubfp(v30, v25, v0);
    v31 = __vmaddfp(v28, v31, v9);
    v30 = __vmaddfp(v22, v30, v9);
    v9 = __vmulfp(v1, v31);
    v31 = __vsubfp(v4, v13);
    v1 = v9;
    v9 = __vmulfp(v9, v30);
    v9 = __vsel(v9, v1, v3);
    v1 = __vpermwi(v11, 0x63);
    v30 = __vmaddfp(v9, v7, v8);
    v9 = __vsubfp(v30, v13);
    v3 = __vpermwi(v9, 0x63);
    v3 = __vmulfp(v11, v3);
    v9 = __vnmsubfp(v1, v9, v3);
    v9 = __vpermwi(v9, 0x63);
    v1 = __vmsum3fp(v31, v9);
    v9 = Broadcast(0.0f);
    v9 = __vspltw(v9, 0);
    v3 = __vmulfp(v1, v5);
    v9 = __vcmpgtfpR(v3, v9, &cr);
    allGreater = (cr & 0x80) != 0;
    if (allGreater) goto L82BB5740;
L82BB5DD0:
    v9 = __vcmpgtfpR(v1, v5, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5E08;
    v9 = v26;
    v3 = v2;
    v31 = v5;
    v26 = v30;
    v2 = v4;
    v5 = v1;
    v30 = v9;
    v4 = v3;
    v1 = v31;
L82BB5E08:
    iteration = 0;
    v9 = Broadcast(1.0f);
    v25 = __vspltw(v9, 0);
L82BB5E24:
    v9 = __vslw(v24, v24);
    v9 = __vxor(v1, v9);
    v9 = __vcmpgtfpR(v5, v9, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB5E74;
    v9 = __vslw(v24, v24);
    v9 = __vxor(v23, v9);
    v9 = __vcmpgtfpR(v1, v9, &cr);
    allGreater = (cr & 0x80) != 0;
    if (allGreater) goto L82BB5E64;
    if (iteration != 10) goto L82BB5E8C;
L82BB5E64:
    v13 = __vsubfp(v4, v13);
    v8 = __vsubfp(v30, v8);
    v9 = __vsubfp(v4, v30);
    goto L82BB607C;
L82BB5E74:
    v9 = __vcmpgtfpR(v23, v5, &cr);
    allGreater = (cr & 0x80) != 0;
    if (allGreater) goto L82BB6070;
    if (iteration == 10) goto L82BB6070;
L82BB5E8C:
    v9 = __vsubfp(v5, v1);
    v3 = Broadcast(1.0f);
    v28 = v9;
    v3 = __vspltw(v3, 0);
    v29 = v9;
    v9 = __vrefp(v9);
    v3 = __vaddfp(v25, v3);
    v31 = Broadcast(0.5f);
    v31 = __vspltw(v31, 0);
    v31 = __vmulfp(v25, v31);
    v29 = __vnmsubfp(v9, v29, v0);
    v22 = v3;
    v21 = v3;
    v3 = __vrefp(v3);
    v9 = __vmaddfp(v9, v29, v9);
    v29 = __vnmsubfp(v3, v22, v0);
    v3 = __vmaddfp(v3, v29, v3);
    v29 = __vnmsubfp(v9, v28, v0);
    v9 = __vmaddfp(v9, v29, v9);
    v9 = __vmaddfp(v9, v5, v31);
    v31 = __vnmsubfp(v3, v21, v0);
    v3 = __vmaddfp(v3, v31, v3);
    v9 = __vmulfp(v3, v9);
    scalarFraction = v9.x;
    scalarFraction = 1.0f - scalarFraction;
    v3 = Broadcast(scalarFraction);
    v3 = __vspltw(v3, 0);
    v3 = __vmulfp(v2, v3);
    v9 = __vmaddfp(v4, v9, v3);
    v3 = __vsubfp(v9, v13);
    v9 = __vmsum3fp(v3, v3);
    v29 = v9;
    v31 = v9;
    v9 = __vrsqrtefp(v9);
    v28 = __vmulfp(v9, v9);
    v22 = __vmulfp(v9, v12);
    v31 = __vnmsubfp(v31, v28, v0);
    v9 = __vmaddfp(v22, v31, v9);
    v31 = __vmulfp(v9, v9);
    v28 = __vmulfp(v9, v12);
    v31 = __vnmsubfp(v29, v31, v0);
    v9 = __vmaddfp(v28, v31, v9);
    v9 = __vmulfp(v3, v9);
    v29 = __vmaddfp(v9, v27, v13);
    v9 = __vsubfp(v29, v8);
    v3 = __vmsum3fp(v9, v10);
    v3 = __vmulfp(v10, v3);
    v31 = __vsubfp(v9, v3);
    v3 = __vmsum3fp(v31, v31);
    v9 = __vrsqrtefp(v3);
    v22 = __vmulfp(v9, v9);
    v21 = __vmulfp(v9, v12);
    v20 = __vmulfp(v9, v12);
    v28 = __vnmsubfp(v3, v22, v0);
    v22 = __vmulfp(v9, v9);
    v28 = __vmaddfp(v21, v28, v9);
    v31 = __vmulfp(v31, v28);
    v28 = __vcmpgefp(v3, v6);
    v3 = __vnmsubfp(v3, v22, v0);
    v21 = v31;
    v9 = __vmaddfp(v20, v3, v9);
    v22 = __vpermwi(v11, 0x63);
    v20 = __vsubfp(v29, v13);
    v3 = Broadcast(0.0f);
    v9 = __vmulfp(v31, v9);
    v31 = __vspltw(v3, 0);
    v3 = __vnor(v28, v28);
    v9 = __vsel(v9, v21, v3);
    v3 = __vmaddfp(v9, v7, v8);
    v9 = __vsubfp(v3, v13);
    v28 = v9;
    v9 = __vpermwi(v9, 0x63);
    v9 = __vmulfp(v11, v9);
    v9 = __vnmsubfp(v22, v28, v9);
    v9 = __vpermwi(v9, 0x63);
    v9 = __vmsum3fp(v20, v9);
    v31 = __vcmpgtfpR(v31, v9, &cr);
    allGreater = (cr & 0x80) != 0;
    if (!allGreater) goto L82BB6038;
    v4 = v29;
    v30 = v3;
    v1 = v9;
    goto L82BB6044;
L82BB6038:
    v2 = v29;
    v26 = v3;
    v5 = v9;
L82BB6044:
    ++iteration;
    v9 = Broadcast(0.5f);
    v9 = __vspltw(v9, 0);
    v25 = __vmulfp(v25, v9);
    goto L82BB5E24;
L82BB6070:
    v13 = __vsubfp(v2, v13);
    v8 = __vsubfp(v26, v8);
    v9 = __vsubfp(v2, v26);
L82BB607C:
    v7 = v11;
    v11 = __vpermwi(v11, 0x63);
    v5 = v10;
    v10 = __vpermwi(v10, 0x63);
    v11 = __vmulfp(v13, v11);
    v13 = __vpermwi(v13, 0x63);
    v4 = __vmulfp(v8, v10);
    v10 = __vmsum3fp(v9, v9);
    v8 = __vpermwi(v8, 0x63);
    v13 = __vnmsubfp(v13, v7, v11);
    v8 = __vnmsubfp(v8, v5, v4);
    v7 = __vcmpgefp(v10, v6);
    v11 = __vpermwi(v13, 0x63);
    v13 = __vrsqrtefp(v10);
    v8 = __vpermwi(v8, 0x63);
    v6 = v11;
    v11 = __vpermwi(v11, 0x63);
    v5 = __vpermwi(v8, 0x63);
    v7 = __vnor(v7, v7);
    v11 = __vmulfp(v8, v11);
    v11 = __vnmsubfp(v5, v6, v11);
    v8 = __vpermwi(v11, 0x63);
    v11 = __vmsum3fp(v8, v8);
    v6 = v11;
    v11 = __vrsqrtefp(v11);
    v5 = __vmulfp(v11, v11);
    v4 = __vmulfp(v11, v12);
    v12 = __vmulfp(v13, v12);
    v6 = __vnmsubfp(v6, v5, v0);
    v11 = __vmaddfp(v4, v6, v11);
    v11 = __vmulfp(v8, v11);
    v8 = __vmulfp(v13, v13);
    v0 = __vnmsubfp(v10, v8, v0);
    v0 = __vmaddfp(v12, v0, v13);
    v0 = __vmulfp(v9, v0);
    v0 = __vsel(v0, v11, v7);
L82BB610C:
    lpCandidate->mV = v0;
    return 1;
}

// ARTIST 0x82BB6118
void AddRimToEdgeCandidate(const Vector3& arRimCentre, const Vector3& arRimAxis,
                           const VecFloat& arRimRadius, const Vector3& arEdgePoint,
                           const Vector3& arEdgeDirection,
                           f32 afEdgeOffset, f32 afEdgeHalfLength,
                           Vector3* lapCandidateDirs, u32* lpuNumCandidateDirs)
{

    const f32 lfOffsetPlusHalf  = afEdgeOffset + afEdgeHalfLength;
    const f32 lfOffsetMinusHalf = afEdgeOffset - afEdgeHalfLength;

    const Vector3 lvEdgePointA = MaddScalar(arEdgeDirection, lfOffsetPlusHalf, arEdgePoint);
    const Vector3 lvEdgePointB = MaddScalar(arEdgeDirection, lfOffsetMinusHalf, arEdgePoint);

    const Vector3 lvCandidateDir =
        RimToEdge(arRimCentre, arRimAxis, arRimRadius, lvEdgePointA, lvEdgePointB);

    lapCandidateDirs[*lpuNumCandidateDirs] = lvCandidateDir;
    ++(*lpuNumCandidateDirs);
}

static const f32 KF_RIM_PARALLEL_EPSILON = 1.1754944e-38f;

// ARTIST 0x82BB61C0
void AddRimToRimCandidates(const GPInstance* lpGP1, const GPInstance* lpGP2,
                           Vector3* lapCandidateDirs, u32* lpuNumCandidateDirs)
{

    const Vector3 lvCrossDir = SepCross(lpGP1->mEdgeDirections[0], lpGP2->mEdgeDirections[0]);

    const f32 lfCrossLenSq = Dot3(lvCrossDir, lvCrossDir);

    if (!(lfCrossLenSq > KF_RIM_PARALLEL_EPSILON))
    {
        return;
    }

    const Vector3 lvSepDir = Vector3(__vmulfp(lvCrossDir.mV, SATInvLength(Broadcast(lfCrossLenSq))));

    const f32 lfHalfExtent1 = lpGP1->mDimensions.mV.x;
    const f32 lfHalfExtent2 = lpGP2->mDimensions.mV.x;

    const VecFloat lvRadius1(__vspltw(lpGP1->mDimensions.mV, 1));
    const VecFloat lvRadius2(__vspltw(lpGP2->mDimensions.mV, 1));

    const Vector3 lvAxisOffset1 = Scale(lpGP1->mEdgeDirections[0], lfHalfExtent1);
    const Vector3 lvAxisOffset2 = Scale(lpGP2->mEdgeDirections[0], lfHalfExtent2);

    if (RimToRim(SepAdd(lpGP1->mPos, lvAxisOffset1), lpGP1->mEdgeDirections[0], lvRadius1,
                 SepAdd(lpGP2->mPos, lvAxisOffset2), lpGP2->mEdgeDirections[0], lvRadius2,
                 lvSepDir, &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    if (RimToRim(SepSub(lpGP1->mPos, lvAxisOffset1), SepNegate(lpGP1->mEdgeDirections[0]), lvRadius1,
                 SepAdd(lpGP2->mPos, lvAxisOffset2), lpGP2->mEdgeDirections[0], lvRadius2,
                 SepNegate(lvSepDir), &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    if (RimToRim(SepAdd(lpGP1->mPos, lvAxisOffset1), lpGP1->mEdgeDirections[0], lvRadius1,
                 SepSub(lpGP2->mPos, lvAxisOffset2), SepNegate(lpGP2->mEdgeDirections[0]), lvRadius2,
                 SepNegate(lvSepDir), &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }

    if (RimToRim(SepSub(lpGP1->mPos, lvAxisOffset1), SepNegate(lpGP1->mEdgeDirections[0]), lvRadius1,
                 SepSub(lpGP2->mPos, lvAxisOffset2), SepNegate(lpGP2->mEdgeDirections[0]), lvRadius2,
                 lvSepDir, &lapCandidateDirs[*lpuNumCandidateDirs]))
    {
        ++(*lpuNumCandidateDirs);
    }
}

namespace
{

    inline f32 ComputeTriBoxAxisSeparation(const Vector3& axis,
        const Vector3& p0, const Vector3& p1, const Vector3& p2,
        const Vector3& boxPosition, const Vector3* boxProjections, f32& sign)
    {
        const __vector4 all = __vspltisw(-1), mask = __vslw(all, all);
        const __vector4 d0 = __vmsum3fp(p0.mV, axis.mV);
        const __vector4 d1 = __vmsum3fp(p1.mV, axis.mV);
        const __vector4 d2 = __vmsum3fp(p2.mV, axis.mV);
        const __vector4 low = __vminfp(d0, __vminfp(d1, d2));
        const __vector4 high = __vmaxfp(d0, __vmaxfp(d1, d2));
        const __vector4 center = __vmsum3fp(axis.mV, boxPosition.mV);
        const __vector4 a = __vandc(__vmsum3fp(boxProjections[0].mV, axis.mV), mask);
        const __vector4 b = __vandc(__vmsum3fp(boxProjections[1].mV, axis.mV), mask);
        const __vector4 c = __vandc(__vmsum3fp(boxProjections[2].mV, axis.mV), mask);
        const __vector4 radius = __vaddfp(__vaddfp(a, b), c);
        const __vector4 reverse = __vsubfp(low, __vaddfp(center, radius));
        const __vector4 forward = __vsubfp(__vsubfp(center, radius), high);
        const __vector4 gt = __vcmpgtfp(reverse, forward), plus = __vcfsx(__vspltisw(1), 0);
        sign = __vsel(plus, __vxor(plus, mask), gt).x;
        return __vsel(forward, reverse, gt).x;
    }
}

// ARTIST 0x82BB4CA8
VecFloat FindBestSeparatingDirectionTriBox(Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2)
{

    const Vector3& lrVert0     = arGP1.mPos;
    const Vector3& lrVert1     = arGP1.mFaceNormals[1];
    const Vector3& lrVert2     = arGP1.mFaceNormals[2];
    const Vector3& lrTriNormal = arGP1.mFaceNormals[0];

    const Vector3& lrBoxPos = arGP2.mPos;
    Vector3 laBoxProj[3];
    laBoxProj[0] = Scale(arGP2.mFaceNormals[0], arGP2.mDimensions.mV.x);
    laBoxProj[1] = Scale(arGP2.mFaceNormals[1], arGP2.mDimensions.mV.y);
    laBoxProj[2] = Scale(arGP2.mFaceNormals[2], arGP2.mDimensions.mV.z);

    Vector3 laCandidates[13];
    laCandidates[0] = lrTriNormal;
    laCandidates[1] = arGP2.mFaceNormals[2];
    laCandidates[2] = arGP2.mFaceNormals[1];
    laCandidates[3] = arGP2.mFaceNormals[0];
    for (s32 li = 0; li < 3; ++li)
    {
        for (s32 lj = 0; lj < 3; ++lj)
        {

            const Vector3 lvCross = SepCross(arGP1.mEdgeDirections[li],
                                       arGP2.mEdgeDirections[lj]);

            laCandidates[4 + 3 * li + lj] =
                Vector3(__vmulfp(lvCross.mV, __vrsqrtefp(__vmsum3fp(lvCross.mV, lvCross.mV))));
        }
    }

    f32  lfBestSign = 0.0f;
    f32  lfBestSep  = ComputeTriBoxAxisSeparation(laCandidates[0], lrVert0, lrVert1, lrVert2,
                                                  lrBoxPos, laBoxProj, lfBestSign);
    Vector3 lvBestAxis = laCandidates[0];

    for (s32 lu = 1; lu < 13; ++lu)
    {
        f32 lfSign = 0.0f;
        const f32 lfSep = ComputeTriBoxAxisSeparation(laCandidates[lu], lrVert0, lrVert1, lrVert2,
                                                      lrBoxPos, laBoxProj, lfSign);
        if (lfSep > lfBestSep)
        {
            lfBestSep  = lfSep;
            lfBestSign = lfSign;
            lvBestAxis = laCandidates[lu];
        }
    }

    arBestSepDir = Scale(lvBestAxis, lfBestSign);
    return VecFloat(Broadcast(lfBestSep));
}

}
}

namespace rw
{
namespace collision
{

namespace
{
    inline VecFloat SplatLane(const Vector3& row, u32 lane)
    {
        if (lane == 0) return VecFloat(__vspltw(row.mV, 0));
        if (lane == 1) return VecFloat(__vspltw(row.mV, 1));
        return VecFloat(__vspltw(row.mV, 2));
    }
    inline Vector3 NormalizeRefined(const Vector3& row, f32 square)
    { return Vector3(__vmulfp(row.mV, SATInvLength(Broadcast(square)))); }
    inline VecFloat SignedHalfExtent(const Vector3& dimensions, u32 lane, f32 dot)
    {
        const VecFloat half = SplatLane(dimensions, lane);
        const __vector4 all = __vspltisw(-1);
        return VecFloat(__vsel(half.mV, __vxor(half.mV, __vslw(all, all)),
            __vcmpgtfp(__vspltisw(0), Broadcast(dot))));
    }
}

static const u32 KU_CYLINDER_SAT_MAX_CANDIDATES = 16u;

// ARTIST 0x82BB75B0
VecFloat FindBestSepDirWithCylinder_Evaluate(
    Vector3& bestDirection, Vector3* candidates, u32 count,
    const GPCylinder& cylinder, const GPInstance& other)
{
    Interval cylinderIntervals[KU_CYLINDER_SAT_MAX_CANDIDATES];
    Interval otherIntervals[KU_CYLINDER_SAT_MAX_CANDIDATES];
    cylinder.GetIntervals(candidates, count, cylinderIntervals);
    (other.*(other.mMethods.mGetIntervals))(candidates, count, otherIntervals);
    __vector4 bestDistance = __vspltisw(0);
    const Vector3* best = 0;
    bool bestFlip = false;
    for (u32 i = 0; i != count; ++i)
    {
        __vector4 distance = __vsubfp(otherIntervals[i].min.mV, cylinderIntervals[i].max.mV);
        const __vector4 reverse = __vsubfp(cylinderIntervals[i].min.mV, otherIntervals[i].max.mV);
        unsigned int cr;
        __vcmpgtfpR(reverse, distance, &cr);
        const bool flip = (cr & 0x80) != 0;
        if (flip) distance = reverse;
        __vcmpgtfpR(distance, bestDistance, &cr);
        if (!best || (cr & 0x80) != 0)
        { best = &candidates[i]; bestDistance = distance; bestFlip = flip; }
    }
    bestDirection = bestFlip ? SepNegate(*best) : *best;
    return VecFloat(bestDistance);
}

static const f32 KF_AXIS_EDGE_PARALLEL_EPSILON = 1.1754944e-38f;

// ARTIST 0x82BB50E8
static void AddAxisToEdgeCandidate(const Vector3& arAxisPoint, const Vector3& arAxisDirection,
                                   const Vector3& arEdgePoint, const Vector3& arEdgeDirection,
                                   f32 afEdgeOffset, f32 afEdgeHalfLength,
                                   Vector3* lapCandidateDirs, u32* lpuNumCandidateDirs)
{

    Vector3& lrSlot = lapCandidateDirs[*lpuNumCandidateDirs];
    ++(*lpuNumCandidateDirs);

    const Vector3 lvCross      = SepCross(arAxisDirection, arEdgeDirection);
    const f32  lfCrossLenSq = Dot3(lvCross, lvCross);

    if (KF_AXIS_EDGE_PARALLEL_EPSILON > lfCrossLenSq)
    {

        const Vector3 lvGap       = SepSub(arAxisPoint, arEdgePoint);
        const Vector3 lvPerp      = SepCross(SepCross(arAxisDirection, lvGap), arAxisDirection);
        const f32  lfPerpLenSq = Dot3(lvPerp, lvPerp);

        lrSlot = (KF_AXIS_EDGE_PARALLEL_EPSILON > lfPerpLenSq)
                     ? arAxisDirection
                     : NormalizeRefined(lvPerp, lfPerpLenSq);
        return;
    }

    const Vector3 lvPlaneNormal = SepCross(arAxisDirection, lvCross);
    const f32  lfParameter   = __vmulfp(__vmsum3fp(lvPlaneNormal.mV, SepSub(arAxisPoint, arEdgePoint).mV),
                                ReciprocalRefined(__vmsum3fp(lvPlaneNormal.mV, arEdgeDirection.mV))).x;
    const f32  lfOffsetFromMid = lfParameter - afEdgeOffset;

    f32 lfClamped;
    if (lfOffsetFromMid > afEdgeHalfLength)
    {
        lfClamped = afEdgeOffset + afEdgeHalfLength;
    }
    else if (-afEdgeHalfLength > lfOffsetFromMid)
    {
        lfClamped = afEdgeOffset - afEdgeHalfLength;
    }
    else
    {

        lrSlot = NormalizeRefined(lvCross, lfCrossLenSq);
        return;
    }

    const Vector3 lvEndPoint = MaddScalar(arEdgeDirection, lfClamped, arEdgePoint);
    const Vector3 lvRadial   = SepCross(arAxisDirection, SepCross(lvEndPoint, arAxisDirection));
    lrSlot = NormalizeRefined(lvRadial, Dot3(lvRadial, lvRadial));
}

static const f32 KF_CYLINDER_SAT_HALF = 0.5f;

// ARTIST 0x82BB64C0
u32 FindBestSepDirWithCylinder_FindCandidates(
    Vector3* lapCandidates, const GPCylinder& arGPCylinder, const GPInstance& arGPOther)
{
    const GPInstance& lrCylinder = arGPCylinder;
    const GPInstance& lrOther    = arGPOther;

    lapCandidates[0] = lrCylinder.mEdgeDirections[0];
    u32 luNumDirs    = 1u;

    for (u32 luI = 0u; luI < static_cast<u32>(lrOther.mNumFaceNormals); ++luI)
    {
        lapCandidates[1u + luI] = lrOther.mFaceNormals[luI];
        ++luNumDirs;
    }

    if (lrOther.mVolumeType == GPInstance::SPHERE)
    {
        const Vector3 lvAxis  = lrCylinder.mEdgeDirections[0];
        const Vector3 lvDelta = SepSub(lrOther.mPos, lrCylinder.mPos);

        Vector3 lvCross      = SepCross(lvAxis, lvDelta);
        f32  lfCrossLenSq = Dot3(lvCross, lvCross);

        if (KF_AXIS_EDGE_PARALLEL_EPSILON > lfCrossLenSq)
        {

            const __vector4 basisI = {1.0f, 0.0f, 0.0f, 0.0f};
            const __vector4 basisJ = {0.0f, 1.0f, 0.0f, 0.0f};
            const Vector3 lvSeed((lvAxis.mV.x > KF_CYLINDER_SAT_HALF) ? basisJ : basisI);
            lvCross      = SepCross(lvAxis, lvSeed);
            lfCrossLenSq = Dot3(lvCross, lvCross);
        }

        lapCandidates[luNumDirs] =
            SepCross(lvAxis, NormalizeRefined(lvCross, lfCrossLenSq));
        ++luNumDirs;
    }

    if (lrOther.mVolumeType == GPInstance::CYLINDER)
    {
        const Vector3 lvOtherAxis   = lrOther.mEdgeDirections[0];
        const VecFloat lvOtherRadius = SplatLane(lrOther.mDimensions, 1u);
        const Vector3 lvOtherOffset = Scale(lvOtherAxis, lrOther.mDimensions.mV.x);

        AddRimToEdgeCandidate(MaddScalar(lvOtherAxis, lrOther.mDimensions.mV.x, lrOther.mPos), lvOtherAxis, lvOtherRadius,
                              lrCylinder.mPos, lrCylinder.mEdgeDirections[0],
                              0.0f, lrCylinder.mDimensions.mV.x,
                              lapCandidates, &luNumDirs);

        AddRimToEdgeCandidate(SepSub(lrOther.mPos, lvOtherOffset), lvOtherAxis, lvOtherRadius,
                              lrCylinder.mPos, lrCylinder.mEdgeDirections[0],
                              0.0f, lrCylinder.mDimensions.mV.x,
                              lapCandidates, &luNumDirs);

        AddRimToRimCandidates(&lrCylinder, &lrOther, lapCandidates, &luNumDirs);
    }

    const Vector3 lvCylAxis    = lrCylinder.mEdgeDirections[0];
    const VecFloat lvRimRadius  = SplatLane(lrCylinder.mDimensions, 1u);
    const Vector3 lvAxisOffset = Scale(lvCylAxis, lrCylinder.mDimensions.mV.x);
    const Vector3 lvRimPlus    = MaddScalar(lvCylAxis, lrCylinder.mDimensions.mV.x, lrCylinder.mPos);
    const Vector3 lvRimMinus   = SepSub(lrCylinder.mPos, lvAxisOffset);

    if (lrOther.mNumEdgeDirections == 1)
    {
        const Vector3 lvOtherAxis = lrOther.mEdgeDirections[0];
        const f32  lfOtherHalf = lrOther.mDimensions.mV.x;

        AddRimToEdgeCandidate(lvRimPlus, lvCylAxis, lvRimRadius,
                              lrOther.mPos, lvOtherAxis, 0.0f, lfOtherHalf,
                              lapCandidates, &luNumDirs);

        AddRimToEdgeCandidate(lvRimMinus, SepNegate(lvCylAxis), lvRimRadius,
                              lrOther.mPos, lvOtherAxis, 0.0f, lfOtherHalf,
                              lapCandidates, &luNumDirs);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis,
                               lrOther.mPos, lvOtherAxis, 0.0f, lfOtherHalf,
                               lapCandidates, &luNumDirs);
        return luNumDirs;
    }

    if (lrOther.mVolumeType == GPInstance::BOX)
    {
        const Vector3& lrAxis0 = lrOther.mEdgeDirections[0];
        const Vector3& lrAxis1 = lrOther.mEdgeDirections[1];
        const Vector3& lrAxis2 = lrOther.mEdgeDirections[2];
        const Vector3& lrDims  = lrOther.mDimensions;

        for (u32 luEnd = 0u; luEnd < 2u; ++luEnd)
        {
            const Vector3 lvRimCentre = (luEnd == 0u) ? lvRimPlus : lvRimMinus;
            const Vector3 lvDelta     = SepSub(lvRimCentre, lrOther.mPos);

            const VecFloat lvSH0 = SignedHalfExtent(lrDims, 0u, Dot3(lvDelta, lrAxis0));
            const VecFloat lvSH1 = SignedHalfExtent(lrDims, 1u, Dot3(lvDelta, lrAxis1));
            const VecFloat lvSH2 = SignedHalfExtent(lrDims, 2u, Dot3(lvDelta, lrAxis2));

            Vector3 lvVertex = MaddScalar(lrAxis0, lvSH0.mV.x, Scale(lrAxis1, lvSH1.mV.x));
            lvVertex = MaddScalar(lrAxis2, lvSH2.mV.x, lvVertex);
            lvVertex = SepAdd(lvVertex, lrOther.mPos);

            AddRimToEdgeCandidate(lvRimCentre, lvCylAxis, lvRimRadius,
                                  lvVertex, lrAxis0, -lvSH0.mV.x, lrDims.mV.x,
                                  lapCandidates, &luNumDirs);
            AddRimToEdgeCandidate(lvRimCentre, lvCylAxis, lvRimRadius,
                                  lvVertex, lrAxis1, -lvSH1.mV.x, lrDims.mV.y,
                                  lapCandidates, &luNumDirs);
            AddRimToEdgeCandidate(lvRimCentre, lvCylAxis, lvRimRadius,
                                  lvVertex, lrAxis2, -lvSH2.mV.x, lrDims.mV.z,
                                  lapCandidates, &luNumDirs);
        }

        const VecFloat lvSA0 = SignedHalfExtent(lrDims, 0u, Dot3(lvCylAxis, lrAxis0));
        const VecFloat lvSA1 = SignedHalfExtent(lrDims, 1u, Dot3(lvCylAxis, lrAxis1));
        const VecFloat lvSA2 = SignedHalfExtent(lrDims, 2u, Dot3(lvCylAxis, lrAxis2));

        Vector3 lvCorner = MaddScalar(lrAxis0, -lvSA0.mV.x, Scale(lrAxis1, lvSA1.mV.x));
        lvCorner = MaddScalar(lrAxis2, lvSA2.mV.x, lvCorner);
        lvCorner = SepAdd(lvCorner, lrOther.mPos);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis1,
                               -lvSA1.mV.x, lrDims.mV.y, lapCandidates, &luNumDirs);
        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis2,
                               -lvSA2.mV.x, lrDims.mV.z, lapCandidates, &luNumDirs);

        lvCorner = SepSub(Scale(lrAxis0, lvSA0.mV.x), Scale(lrAxis1, lvSA1.mV.x));
        lvCorner = MaddScalar(lrAxis2, lvSA2.mV.x, lvCorner);
        lvCorner = SepAdd(lvCorner, lrOther.mPos);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis2,
                               -lvSA2.mV.x, lrDims.mV.z, lapCandidates, &luNumDirs);
        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis0,
                               -lvSA0.mV.x, lrDims.mV.x, lapCandidates, &luNumDirs);

        lvCorner = MaddScalar(lrAxis0, lvSA0.mV.x, Scale(lrAxis1, lvSA1.mV.x));
        lvCorner = SepSub(lvCorner, Scale(lrAxis2, lvSA2.mV.x));
        lvCorner = SepAdd(lvCorner, lrOther.mPos);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis0,
                               -lvSA0.mV.x, lrDims.mV.x, lapCandidates, &luNumDirs);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lvCorner, lrAxis1,
                               -lvSA1.mV.x, lrDims.mV.y, lapCandidates, &luNumDirs);
        return luNumDirs;
    }

    if (lrOther.mVolumeType == GPInstance::TRIANGLE)
    {

        const Vector3& lrVert0 = lrOther.mPos;
        const Vector3& lrVert1 = lrOther.mFaceNormals[1];
        const Vector3& lrVert2 = lrOther.mFaceNormals[2];

        lapCandidates[luNumDirs++] = RimToEdge(lvRimPlus, lvCylAxis, lvRimRadius, lrVert0, lrVert1);
        lapCandidates[luNumDirs++] = RimToEdge(lvRimPlus, lvCylAxis, lvRimRadius, lrVert1, lrVert2);
        lapCandidates[luNumDirs++] = RimToEdge(lvRimPlus, lvCylAxis, lvRimRadius, lrVert2, lrVert0);

        lapCandidates[luNumDirs++] = RimToEdge(lvRimMinus, lvCylAxis, lvRimRadius, lrVert0, lrVert1);
        lapCandidates[luNumDirs++] = RimToEdge(lvRimMinus, lvCylAxis, lvRimRadius, lrVert1, lrVert2);
        lapCandidates[luNumDirs++] = RimToEdge(lvRimMinus, lvCylAxis, lvRimRadius, lrVert2, lrVert0);

        const f32 lfHalfEdge0 = lrOther.mDimensions.mV.x * KF_CYLINDER_SAT_HALF;
        const f32 lfHalfEdge1 = lrOther.mDimensions.mV.y * KF_CYLINDER_SAT_HALF;
        const f32 lfHalfEdge2 = lrOther.mDimensions.mV.z * KF_CYLINDER_SAT_HALF;

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lrVert0,
                               lrOther.mEdgeDirections[0], lfHalfEdge0, lfHalfEdge0,
                               lapCandidates, &luNumDirs);
        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lrVert1,
                               lrOther.mEdgeDirections[1], lfHalfEdge1, lfHalfEdge1,
                               lapCandidates, &luNumDirs);

        AddAxisToEdgeCandidate(lrCylinder.mPos, lvCylAxis, lrVert2,
                               lrOther.mEdgeDirections[2], lfHalfEdge2, lfHalfEdge2,
                               lapCandidates, &luNumDirs);
        return luNumDirs;
    }

    return luNumDirs;
}

}
}
