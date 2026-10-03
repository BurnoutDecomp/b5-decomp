#pragma once

// FLAG PC-platform leaf: SSE2 restores ARTIST's four-triangle vector execution.
// The existing PC exact sqrt/division lowering, operation order, face precedence,
// two-vertex face test, later-pair ties, and unconditional outputs are retained.
// No approximate reciprocal, fast-math mode, ISA above SSE2, or culling change.
// Original: 0x8283EF50 plus circle helper 0x82839AC0; check VMX128 raw operands.
#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"
#include <xmmintrin.h>
#include <emmintrin.h>

namespace CgsGeometric { namespace SweptSpherePC {
struct Float4 {
    __m128 v;
    Float4() : v(_mm_setzero_ps()) {}
    Float4(float x) : v(_mm_set1_ps(x)) {}
    Float4(__m128 x) : v(x) {}
};
struct Mask4 {
    __m128 v;
    Mask4(bool x=false) : v(_mm_castsi128_ps(_mm_set1_epi32(x ? -1 : 0))) {}
    Mask4(__m128 x) : v(x) {}
};
inline Float4 operator+(Float4 a, Float4 b) { return _mm_add_ps(a.v, b.v); }
inline Float4 operator-(Float4 a, Float4 b) { return _mm_sub_ps(a.v, b.v); }
inline Float4 operator-(Float4 a) { return _mm_xor_ps(a.v, _mm_set1_ps(-0.0f)); }
inline Float4 operator*(Float4 a, Float4 b) { return _mm_mul_ps(a.v, b.v); }
inline Float4 operator/(Float4 a, Float4 b) { return _mm_div_ps(a.v, b.v); }
inline Float4& operator*=(Float4& a, Float4 b) { a = a * b; return a; }
inline Mask4 operator<(Float4 a, Float4 b) { return _mm_cmplt_ps(a.v, b.v); }
inline Mask4 operator>(Float4 a, Float4 b) { return _mm_cmpgt_ps(a.v, b.v); }
inline Mask4 operator>=(Float4 a, Float4 b) { return _mm_cmpge_ps(a.v, b.v); }
inline Mask4 operator==(Float4 a, Float4 b) { return _mm_cmpeq_ps(a.v, b.v); }
inline Mask4 operator!=(Float4 a, Float4 b) { return _mm_cmpneq_ps(a.v, b.v); }
inline Mask4 operator!(Mask4 a) { return _mm_xor_ps(a.v, Mask4(true).v); }
inline Mask4 operator&&(Mask4 a, Mask4 b) { return _mm_and_ps(a.v, b.v); }
inline Mask4 operator||(Mask4 a, Mask4 b) { return _mm_or_ps(a.v, b.v); }
inline Float4 Select(Mask4 mask, Float4 yes, Float4 no)
{
    return _mm_or_ps(_mm_and_ps(mask.v, yes.v), _mm_andnot_ps(mask.v, no.v));
}
inline Float4 Sqrt(Float4 a) { return _mm_sqrt_ps(a.v); }
// MINPS/MAXPS choose the second operand for equality and unordered comparisons.
inline Float4 Min(Float4 a, Float4 b) { return _mm_min_ps(a.v, b.v); }
inline Float4 Max(Float4 a, Float4 b) { return _mm_max_ps(a.v, b.v); }
inline Float4 Load(const Vector4& a) { return _mm_loadu_ps(&a.x); }
struct Point { Float4 u, v; };
inline Point Select(Mask4 mask, Point yes, Point no)
{
    return { Select(mask, yes.u, no.u), Select(mask, yes.v, no.v) };
}
template<class Vector>
inline void Store(Float4 x, Float4 y, Float4 z, Float4 w, Vector* const* output)
{
    __m128 a = x.v, b = y.v, c = z.v, d = w.v;
    _MM_TRANSPOSE4_PS(a, b, c, d);
    _mm_storeu_ps(&output[0]->x, a);
    _mm_storeu_ps(&output[1]->x, b);
    _mm_storeu_ps(&output[2]->x, c);
    _mm_storeu_ps(&output[3]->x, d);
}

inline Mask4 Intersect2DCircleWithTriangle(Point lCentre, Float4 lfRadiusSq,
                                   Point lA, Point lB,
                                   Point lC,
                                   Point lDirAB, Point lDirBC,
                                   Point lDirCA,
                                   Float4 lfLenAB, Float4 lfLenBC, Float4 lfLenCA,
                                   Point& lrClosestPoint)
{

    const Float4 lfSideAB = (lDirAB.u * (lA.v - lCentre.v)) - (lDirAB.v * (lA.u - lCentre.u));
    const Float4 lfSideBC = (lDirBC.u * (lB.v - lCentre.v)) - (lDirBC.v * (lB.u - lCentre.u));
    const Float4 lfSideCA = (lDirCA.u * (lC.v - lCentre.v)) - (lDirCA.v * (lC.u - lCentre.u));
    const Mask4 lbInside = (lfSideBC >= 0.0f) && (lfSideAB >= 0.0f) && (lfSideCA >= 0.0f);

    const Point laQ[3] =
    {
        { lCentre.u - lA.u, lCentre.v - lA.v },
        { lCentre.u - lB.u, lCentre.v - lB.v },
        { lCentre.u - lC.u, lCentre.v - lC.v },
    };
    const Point laBase[3] = { lA, lB, lC };
    const Point laDir[3]  = { lDirAB, lDirBC, lDirCA };
    const Float4              lafLen[3] = { lfLenAB, lfLenBC, lfLenCA };

    Float4  lafProj[3];
    Mask4 labBehind[3];
    for (s32 liEdge = 0; liEdge < 3; ++liEdge)
    {
        lafProj[liEdge] = (laQ[liEdge].u * laDir[liEdge].u)
                        + (laQ[liEdge].v * laDir[liEdge].v);
        labBehind[liEdge] = (0.0f >= lafProj[liEdge]);
    }

    Float4  lfBest  = lfRadiusSq;
    Mask4 lbFound = false;
    lrClosestPoint = lCentre;

    // ARTIST helper tests A and B here, followed by AB, BC, CA.
    for (s32 liVertex = 0; liVertex < 2; ++liVertex)
    {
        const Float4 lfDistSq = (laQ[liVertex].u * laQ[liVertex].u)
                           + (laQ[liVertex].v * laQ[liVertex].v);
        const Mask4 take = (lfBest >= lfDistSq) && labBehind[liVertex];
        lfBest = Select(take, lfDistSq, lfBest);
        lrClosestPoint = Select(take, laBase[liVertex], lrClosestPoint);
        lbFound = lbFound || take;
    }

    for (s32 liEdge = 0; liEdge < 3; ++liEdge)
    {
        const Float4 lfPerpU = laQ[liEdge].u - (lafProj[liEdge] * laDir[liEdge].u);
        const Float4 lfPerpV = laQ[liEdge].v - (lafProj[liEdge] * laDir[liEdge].v);
        const Float4 lfDistSq = (lfPerpU * lfPerpU) + (lfPerpV * lfPerpV);

        const Mask4 lbOnSegment =
            !(labBehind[liEdge] || (lafProj[liEdge] >= lafLen[liEdge]));

        const Mask4 take = (lfBest >= lfDistSq) && lbOnSegment;
        lfBest = Select(take, lfDistSq, lfBest);
        lrClosestPoint.u = Select(take, (lafProj[liEdge] * laDir[liEdge].u) + laBase[liEdge].u, lrClosestPoint.u);
        lrClosestPoint.v = Select(take, (lafProj[liEdge] * laDir[liEdge].v) + laBase[liEdge].v, lrClosestPoint.v);
        lbFound = lbFound || take;
    }

    lrClosestPoint = Select(lbInside, lCentre, lrClosestPoint);
    return lbFound || lbInside;
}

inline Float4 SolveSweptCircleEquation(Float4 lfA, Float4 lfB, Float4 lfC, Float4& lrfDiscriminant)
{
    lrfDiscriminant = (lfB * lfB) - ((4.0f * lfA) * lfC);

    const Float4 lfInvTwoA = 1.0f / (2.0f * lfA);
    const Float4 lfRoot = Select(lrfDiscriminant == 0.0f, Float4(0.0f),
        lrfDiscriminant * (1.0f / Sqrt(Select(lrfDiscriminant == 0.0f, Float4(1.0f), lrfDiscriminant))));

    return Min((-lfB + lfRoot) * lfInvTwoA, (-lfB - lfRoot) * lfInvTwoA);
}

inline Triangle4::Mask4 Run(
    const SweptSphere& lSweptSphere,
    const Triangle4&   lTriangles,
    Vector3& lContactNormal0, Vector3& lTriangleNormal0,
    Vector3Plus& lSphereContactPoint0, Vector3Plus& lTriangleContactPoint0,
    Vector3& lContactNormal1, Vector3& lTriangleNormal1,
    Vector3Plus& lSphereContactPoint1, Vector3Plus& lTriangleContactPoint1,
    Vector3& lContactNormal2, Vector3& lTriangleNormal2,
    Vector3Plus& lSphereContactPoint2, Vector3Plus& lTriangleContactPoint2,
    Vector3& lContactNormal3, Vector3& lTriangleNormal3,
    Vector3Plus& lSphereContactPoint3, Vector3Plus& lTriangleContactPoint3)
{
    Vector3*     lapContactNormal[4]        = { &lContactNormal0, &lContactNormal1,
                                                &lContactNormal2, &lContactNormal3 };
    Vector3*     lapTriangleNormal[4]       = { &lTriangleNormal0, &lTriangleNormal1,
                                                &lTriangleNormal2, &lTriangleNormal3 };
    Vector3Plus* lapSphereContactPoint[4]   = { &lSphereContactPoint0, &lSphereContactPoint1,
                                                &lSphereContactPoint2, &lSphereContactPoint3 };
    Vector3Plus* lapTriangleContactPoint[4] = { &lTriangleContactPoint0, &lTriangleContactPoint1,
                                                &lTriangleContactPoint2, &lTriangleContactPoint3 };

    const Vector3Plus lPositionAndRadius  = lSweptSphere.GetPositionAndRadius();
    const Vector3Plus lDirectionAndLength = lSweptSphere.GetDirectionAndLength();

    const Float4 lfCx = lPositionAndRadius.x;
    const Float4 lfCy = lPositionAndRadius.y;
    const Float4 lfCz = lPositionAndRadius.z;
    const Float4 lfRadius = lPositionAndRadius.w;

    const Float4 lfDx = lDirectionAndLength.x * lDirectionAndLength.w;
    const Float4 lfDy = lDirectionAndLength.y * lDirectionAndLength.w;
    const Float4 lfDz = lDirectionAndLength.z * lDirectionAndLength.w;

    Triangle4::Mask4 lResult;
    lResult.SetZero();

    {
        const Float4 lfP0x = Load(lTriangles.mVertex0X);
        const Float4 lfP0y = Load(lTriangles.mVertex0Y);
        const Float4 lfP0z = Load(lTriangles.mVertex0Z);
        const Float4 lfP1x = Load(lTriangles.mVertex1X);
        const Float4 lfP1y = Load(lTriangles.mVertex1Y);
        const Float4 lfP1z = Load(lTriangles.mVertex1Z);
        const Float4 lfP2x = Load(lTriangles.mVertex2X);
        const Float4 lfP2y = Load(lTriangles.mVertex2Y);
        const Float4 lfP2z = Load(lTriangles.mVertex2Z);

        const Float4 lfE1x = lfP1x - lfP0x, lfE1y = lfP1y - lfP0y, lfE1z = lfP1z - lfP0z;
        const Float4 lfE2x = lfP2x - lfP1x, lfE2y = lfP2y - lfP1y, lfE2z = lfP2z - lfP1z;

        const Float4 lfCrx = (lfE1y * lfE2z) - (lfE1z * lfE2y);
        const Float4 lfCry = (lfE1z * lfE2x) - (lfE1x * lfE2z);
        const Float4 lfCrz = (lfE1x * lfE2y) - (lfE1y * lfE2x);
        const Float4 lfInvCr =
            1.0f / Sqrt((lfCrx * lfCrx) + (lfCry * lfCry) + (lfCrz * lfCrz));
        const Float4 lfNx = lfCrx * lfInvCr;
        const Float4 lfNy = lfCry * lfInvCr;
        const Float4 lfNz = lfCrz * lfInvCr;
        const Float4 lfPlaneD = (lfNx * lfP0x) + (lfNy * lfP0y) + (lfNz * lfP0z);

        const Float4 lfInvE1 =
            1.0f / Sqrt((lfE1x * lfE1x) + (lfE1y * lfE1y) + (lfE1z * lfE1z));
        const Float4 lfUx = lfE1x * lfInvE1;
        const Float4 lfUy = lfE1y * lfInvE1;
        const Float4 lfUz = lfE1z * lfInvE1;

        Float4 lfVx = (lfUy * lfNz) - (lfUz * lfNy);
        Float4 lfVy = (lfUz * lfNx) - (lfUx * lfNz);
        Float4 lfVz = (lfUx * lfNy) - (lfUy * lfNx);
        const Float4 lfInvV =
            1.0f / Sqrt((lfVx * lfVx) + (lfVy * lfVy) + (lfVz * lfVz));
        lfVx *= lfInvV;
        lfVy *= lfInvV;
        lfVz *= lfInvV;

        const Float4 lfDotDN = (lfDx * lfNx) + (lfDy * lfNy) + (lfDz * lfNz);
        const Float4 lfH0 = ((lfCx * lfNx) + (lfCy * lfNy) + (lfCz * lfNz)) - lfPlaneD;

        const Float4 lfInvNegDN = 1.0f / (-lfDotDN);
        const Float4 lfTa = (lfH0 - lfRadius) * lfInvNegDN;
        const Float4 lfTb = (lfH0 + lfRadius) * lfInvNegDN;

        const Mask4 lbParallel = (lfDotDN == 0.0f);
        const Float4 lfTExit  = Select(lbParallel, Float4(1.0f), Min(Max(lfTa, lfTb), 1.0f));
        const Float4 lfTEnter = Select(lbParallel, Float4(0.0f), Max(Min(lfTa, lfTb), 0.0f));

        const Float4 lfAbsH0 = Select(lfH0 < 0.0f, -lfH0, lfH0);
        const Mask4 lbRejectSlab =
              (lbParallel && (lfAbsH0 >= lfRadius))
           || (!lbParallel && ((0.0f >= lfTExit) || (lfTEnter >= 1.0f)));

        const Float4 lfW2x = lfP2x - lfP0x, lfW2y = lfP2y - lfP0y, lfW2z = lfP2z - lfP0z;
        const Float4 lfRelx = lfCx - lfP0x, lfRely = lfCy - lfP0y, lfRelz = lfCz - lfP0z;

        const Point lCentre2 = { (lfRelx * lfUx) + (lfRely * lfUy) + (lfRelz * lfUz),
                                            (lfRelx * lfVx) + (lfRely * lfVy) + (lfRelz * lfVz) };
        const Point lSweep2  = { (lfDx * lfUx) + (lfDy * lfUy) + (lfDz * lfUz),
                                            (lfDx * lfVx) + (lfDy * lfVy) + (lfDz * lfVz) };

        const Point laVertex[3] =
        {
            { 0.0f, 0.0f },
            { (lfE1x * lfUx) + (lfE1y * lfUy) + (lfE1z * lfUz),
              (lfE1x * lfVx) + (lfE1y * lfVy) + (lfE1z * lfVz) },
            { (lfW2x * lfUx) + (lfW2y * lfUy) + (lfW2z * lfUz),
              (lfW2x * lfVx) + (lfW2y * lfVy) + (lfW2z * lfVz) },
        };

        Point laDir[3];
        Float4 lafLen[3];
        for (s32 liEdge = 0; liEdge < 3; ++liEdge)
        {
            const Point& lrFrom = laVertex[liEdge];
            const Point& lrTo   = laVertex[(liEdge + 1) % 3];
            const Float4 lfEu = lrTo.u - lrFrom.u;
            const Float4 lfEv = lrTo.v - lrFrom.v;
            const Float4 lfLenSq = (lfEu * lfEu) + (lfEv * lfEv);
            const Float4 lfInvLen = 1.0f / Sqrt(lfLenSq);
            laDir[liEdge].u = lfEu * lfInvLen;
            laDir[liEdge].v = lfEv * lfInvLen;
            lafLen[liEdge]  = lfLenSq * lfInvLen;
        }

        const Float4 lfHEnter = (((lfCx + lfDx * lfTEnter) * lfNx)
                            + ((lfCy + lfDy * lfTEnter) * lfNy)
                            + ((lfCz + lfDz * lfTEnter) * lfNz)) - lfPlaneD;
        const Float4 lfCircleRadiusSq = (lfRadius * lfRadius) - (lfHEnter * lfHEnter);

        const Point lCircleCentre =
            { (lfTEnter * lSweep2.u) + lCentre2.u, (lfTEnter * lSweep2.v) + lCentre2.v };

        Point lFacePoint = { 0.0f, 0.0f };
        const Mask4 lbFaceHit =
            Intersect2DCircleWithTriangle(lCircleCentre, lfCircleRadiusSq,
                                          laVertex[0], laVertex[1], laVertex[2],
                                          laDir[0], laDir[1], laDir[2],
                                          lafLen[0], lafLen[1], lafLen[2],
                                          lFacePoint);

        const Float4 lfAFull = ((lSweep2.u * lSweep2.u) + (lSweep2.v * lSweep2.v))
                          + (lfDotDN * lfDotDN);
        const Float4 lfCOffset = (lfH0 * lfH0) - (lfRadius * lfRadius);
        const Float4 lfBOffset = (2.0f * lfH0) * lfDotDN;

        Mask4 labPairHit[3];
        Float4  lafPairTime[3];
        Point laPairPoint[3];

        for (s32 liPair = 0; liPair < 3; ++liPair)
        {
            const Float4 lfQu = lCentre2.u - laVertex[liPair].u;
            const Float4 lfQv = lCentre2.v - laVertex[liPair].v;

            Float4 lfDiscV = 0.0f;
            const Float4 lfVertexTime = SolveSweptCircleEquation(
                lfAFull,
                (((lfQu * lSweep2.u) + (lfQv * lSweep2.v)) * 2.0f) + lfBOffset,
                ((lfQu * lfQu) + (lfQv * lfQv)) + lfCOffset,
                lfDiscV);

            const Mask4 lbVertexHit = (lfAFull != 0.0f) && (lfDiscV >= 0.0f)
                                  && (lfVertexTime >= 0.0f) && (1.0f >= lfVertexTime);

            const Float4 lfQAlong = (lfQu * laDir[liPair].u) + (lfQv * laDir[liPair].v);
            const Float4 lfDAlong = (lSweep2.u * laDir[liPair].u) + (lSweep2.v * laDir[liPair].v);
            const Float4 lfQpu = lfQu - (lfQAlong * laDir[liPair].u);
            const Float4 lfQpv = lfQv - (lfQAlong * laDir[liPair].v);
            const Float4 lfDpu = lSweep2.u - (lfDAlong * laDir[liPair].u);
            const Float4 lfDpv = lSweep2.v - (lfDAlong * laDir[liPair].v);

            const Float4 lfEdgeA = ((lfDpu * lfDpu) + (lfDpv * lfDpv)) + (lfDotDN * lfDotDN);
            Float4 lfDiscE = 0.0f;
            const Float4 lfEdgeTime = SolveSweptCircleEquation(
                lfEdgeA,
                (((lfQpu * lfDpu) + (lfQpv * lfDpv)) * 2.0f) + lfBOffset,
                ((lfQpu * lfQpu) + (lfQpv * lfQpv)) + lfCOffset,
                lfDiscE);

            const Float4 lfAlong = (((lSweep2.u * lfEdgeTime) + lfQu) * laDir[liPair].u)
                              + (((lSweep2.v * lfEdgeTime) + lfQv) * laDir[liPair].v);

            const Mask4 lbEdgeHit = (lfEdgeA != 0.0f) && (lfDiscE >= 0.0f)
                                && (lfEdgeTime >= 0.0f) && (1.0f >= lfEdgeTime)
                                && (lfAlong >= 0.0f) && (lafLen[liPair] >= lfAlong);

            const Float4 lfVertexT = Select(lbVertexHit, lfVertexTime, Float4(2.0f));
            const Float4 lfEdgeT   = Select(lbEdgeHit, lfEdgeTime, Float4(2.0f));
            const Mask4 lbUseVertex = (lfEdgeT >= lfVertexT);

            labPairHit[liPair]  = lbVertexHit || lbEdgeHit;
            lafPairTime[liPair] = Select(labPairHit[liPair], Select(lbUseVertex, lfVertexT, lfEdgeT), Float4(2.0f));
            laPairPoint[liPair].u = Select(lbUseVertex, laVertex[liPair].u,
                (lfAlong * laDir[liPair].u) + laVertex[liPair].u);
            laPairPoint[liPair].v = Select(lbUseVertex, laVertex[liPair].v,
                (lfAlong * laDir[liPair].v) + laVertex[liPair].v);
        }

        // >= keeps the later pair on equal times, as in the VMX selects.
        Float4 lfSweptTime = lafPairTime[0];
        Point lSweptPoint = laPairPoint[0];
        for (s32 liPair = 1; liPair < 3; ++liPair)
        {
            const Mask4 take = lfSweptTime >= lafPairTime[liPair];
            lfSweptTime = Select(take, lafPairTime[liPair], lfSweptTime);
            lSweptPoint = Select(take, laPairPoint[liPair], lSweptPoint);
        }

        // A face contact wins regardless of an earlier swept feature time.
        const Float4 lfHitTime = Select(lbFaceHit, lfTEnter, lfSweptTime);
        const Point lHitPoint = Select(lbFaceHit, lFacePoint, lSweptPoint);

        const Float4 lfPx = ((lHitPoint.u * lfUx) + (lHitPoint.v * lfVx)) + lfP0x;
        const Float4 lfPy = ((lHitPoint.u * lfUy) + (lHitPoint.v * lfVy)) + lfP0y;
        const Float4 lfPz = ((lHitPoint.u * lfUz) + (lHitPoint.v * lfVz)) + lfP0z;

        const Float4 lfFx = lfPx - lfCx, lfFy = lfPy - lfCy, lfFz = lfPz - lfCz;
        const Float4 lfInvF =
            1.0f / Sqrt((lfFx * lfFx) + (lfFy * lfFy) + (lfFz * lfFz));
        const Float4 lfFaceNx = lfFx * lfInvF;
        const Float4 lfFaceNy = lfFy * lfInvF;
        const Float4 lfFaceNz = lfFz * lfInvF;

        const Float4 lfBackX = lfPx - (lfDx * lfHitTime);
        const Float4 lfBackY = lfPy - (lfDy * lfHitTime);
        const Float4 lfBackZ = lfPz - (lfDz * lfHitTime);
        const Float4 lfGx = lfBackX - lfCx, lfGy = lfBackY - lfCy, lfGz = lfBackZ - lfCz;
        const Float4 lfInvG =
            1.0f / Sqrt((lfGx * lfGx) + (lfGy * lfGy) + (lfGz * lfGz));
        const Float4 lfSweptNx = lfGx * lfInvG;
        const Float4 lfSweptNy = lfGy * lfInvG;
        const Float4 lfSweptNz = lfGz * lfInvG;

        const Float4 lfNormalX = Select(lbFaceHit, lfFaceNx, lfSweptNx);
        const Float4 lfNormalY = Select(lbFaceHit, lfFaceNy, lfSweptNy);
        const Float4 lfNormalZ = Select(lbFaceHit, lfFaceNz, lfSweptNz);

        // 0x8283FC58..0x8283FD20: group stores before the final valid-mask load.
        // Preserve all four components even for rejected/disabled triangles.
        Store(lfNormalX, lfNormalY, lfNormalZ, lfNormalZ, lapContactNormal);
        Store(lfNx, lfNy, lfNz, lfNz, lapTriangleNormal);
        Store(Select(lbFaceHit, lfFaceNx * lfRadius + lfCx, lfBackX),
              Select(lbFaceHit, lfFaceNy * lfRadius + lfCy, lfBackY),
              Select(lbFaceHit, lfFaceNz * lfRadius + lfCz, lfBackZ),
              lfHitTime, lapSphereContactPoint);
        Store(lfPx, lfPy, lfPz, lfHitTime, lapTriangleContactPoint);

        const Float4 lfNormalAgainstFace =
            (lfNormalX * lfNx) + (lfNormalY * lfNy) + (lfNormalZ * lfNz);

        const Mask4 lbAccept =
               !lbRejectSlab
            && (lbFaceHit || labPairHit[0] || labPairHit[1] || labPairHit[2])
            && (0.0f > lfNormalAgainstFace);

        _mm_storeu_ps(&lResult.x, _mm_and_ps(lbAccept.v, Load(lTriangles.mValidMasks).v));
    }

    return lResult;
}
} }
#endif
