#include "vendor/renderware/collision/GPInstance.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"

#include <cmath>    // fabs
#include <cstring>  // memcpy (bit-exact subnormal constant)

// ===========================================================================
// rw::collision::GPTriangle -- the triangle primitive's VolumeMethods
// callbacks, reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass).
//
//   GPTriangle::GetMaximumFeature  @ 0x82BBA158   (VolumeMethods +0xA4)
//   GPTriangle::GetInterval        @ 0x82BBA6A0   (VolumeMethods +0xA8)
//   GPTriangle::GetIntervals       @ 0x82BBA6E0   (VolumeMethods +0xAC)
//   TriangleNearestPointRegion     @ 0x82BBA748   (waveQ5 C1; see its banner)
//
// Canonical declarations rwccore.h:1229 (non-static const members on PS3);
// reconstructed as static plain-function callbacks matching the committed
// GPInstance::VolumeMethods typedefs per the X360 delta note in GPInstance.hpp.
//
// Instance layout (the same vertex aliases AABBoxBuilder::CreateFromTriangle
// and TriangleVolume use):
//   mPos            (+0x00)  Vertex0
//   mFaceNormals[0] (+0x10)  unit face normal N (mNumFaceNormals == 1)
//   mFaceNormals[1] (+0x20)  Vertex1
//   mFaceNormals[2] (+0x30)  Vertex2
//   mEdgeDirections[0..2] (+0x40/+0x50/+0x60)  the three unit edge directions
//   mDimensions     (+0x70)  the three edge lengths in lanes x/y/z
//
// The triangle's three boundary edges are (base, direction, length):
//   E0 = (Vertex0, edgeDir0, len.x)
//   E1 = (Vertex2, edgeDir1, len.y)
//   E2 = (Vertex1, edgeDir2, len.z)
// -- exactly the ordering the FACE case (block A) memcpys into edges[0..2] and
// the EDGE case selects from; taken as ground truth from the asm.
//
// VMX decode notes (asm authoritative):
//   * vmsum3fp128 = xyz dot fold broadcast to all lanes -> scalar Dot3 + Splat.
//   * the direction rides in VMX v1 (spilled to the stack near the top and
//     handed to Feature::BuildEdgePlanes by reference at the tail).
//   * the lvsl(0, 4*lane) / vspltw / vperm triples are lane broadcasts of
//     mDimensions element 0/1/2 (the per-edge lengths) -- NOT rodata permute
//     tables.
//   * vspltisw(-1) + vslw builds the 0x80000000 lane mask: vandc against it is
//     fabs, vxor against it is per-lane negate.
//   * every branch is a vcmpgtfp. CR6[0] (all-lanes-true) bit test, extracted
//     by mfocrf r11,2 + extrwi. r11,r11,1,24 (mfocrf mask 2 selects CR field 6;
//     bit 24 of the 32-bit CR word is CR6[0], the ALL-four-lanes-greater flag --
//     CR6[2] is the none-true flag). It is NOT a lane-0 result test. All the
//     operands are lane-broadcast anyway (vmsum3fp128 replicates the dot3 into
//     all four words, the epsilons are vspltw-broadcast, and vandc against a
//     broadcast sign mask preserves that), so all-lanes-true and lane-0-true
//     coincide and this lowers exactly to a scalar `>`.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    inline f32 Dot3(const rw::math::vpu::Vector3& a, const rw::math::vpu::Vector3& b)
    { return __vmsum3fp(a.mV,b.mV).x; }
    inline rw::math::vpu::Vector3 Sub(const rw::math::vpu::Vector3& a, const rw::math::vpu::Vector3& b)
    { return rw::math::vpu::Vector3(__vsubfp(a.mV,b.mV)); }
    inline rw::math::vpu::Vector3 MaddScalar(const rw::math::vpu::Vector3& a, f32 s, const rw::math::vpu::Vector3& b)
    { const __vector4 v = {s,s,s,s}; return rw::math::vpu::Vector3(__vmaddfp(a.mV,v,b.mV)); }
    inline f32 F32FromBits(u32 bits)
    { f32 value; std::memcpy(&value,&bits,sizeof(value)); return value; }

}

// flt_82004FDC: the face-alignment gate -- |dot3(dir, N)| above this makes the
// whole face the maximum feature (value from the export's pseudocode literal).
static const f32 KF_FACE_ALIGN_THRESHOLD = 0.94999999f;

// flt_82180A90: the edge-perpendicularity gate -- an edge direction whose
// |dot3(dir, edgeDir)| is below this counts as "perpendicular to the query"
// and promotes the winning vertex to that edge (pseudocode literal).
static const f32 KF_EDGE_PERP_THRESHOLD = 0.050000001f;

// ===========================================================================
// rw::collision::GPTriangle::GetMaximumFeature @ 0x82BBA158
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Phase 1 tests the query direction against the face normal: when it is nearly
// parallel (|dot| > 0.95) the whole triangle FACE (all three edges) is the
// feature, wound one way or the other by (sign(faceDot) == abCcw). Otherwise
// the best-projecting VERTEX is found and, if one of its two incident edges is
// nearly perpendicular to the query (and more so than the other), that EDGE is
// promoted to the feature. Every path ends by building the boundary-edge prism
// planes with the query direction as the extrusion axis.
// ===========================================================================
namespace
{
    void SetTriangleEdge(Feature& feature, u32 index, __vector4 base, __vector4 direction, __vector4 length)
    {
        feature.edges[index].base.mV = base;
        feature.edges[index].dir.mV = direction;
        feature.edges[index].length.mV = length;
    }
}

void GPTriangle::GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const
{
    const __vector4 v0 = mPos.mV;
    const __vector4 n = mFaceNormals[0].mV;
    const __vector4 v1 = mFaceNormals[1].mV;
    const __vector4 v2 = mFaceNormals[2].mV;
    const __vector4 e0 = mEdgeDirections[0].mV;
    const __vector4 e1 = mEdgeDirections[1].mV;
    const __vector4 e2 = mEdgeDirections[2].mV;
    const __vector4 len = mDimensions.mV;
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const f32 face = __vmsum3fp(arDir.mV, n).x;
    // fcmpu/ble branches on not-greater, including unordered.
    if (std::fabs(face) > KF_FACE_ALIGN_THRESHOLD)
    {
        if (static_cast<RwBool>(face < 0.0f) == abCcw)
        {
            SetTriangleEdge(arFeature, 0, v0, e0, __vspltw(len, 0));
            SetTriangleEdge(arFeature, 1, v2, e1, __vspltw(len, 1));
            SetTriangleEdge(arFeature, 2, v1, e2, __vspltw(len, 2));
            arFeature.region = 8;
        }
        else
        {
            SetTriangleEdge(arFeature, 0, v0, __vxor(e2, sign), __vspltw(len, 2));
            SetTriangleEdge(arFeature, 1, v1, __vxor(e1, sign), __vspltw(len, 1));
            SetTriangleEdge(arFeature, 2, v2, __vxor(e0, sign), __vspltw(len, 0));
            arFeature.region = 0;
        }
        arFeature.numedges = 3;
        arFeature.ownNormal.mV = n;
    }
    else
    {
        const f32 d0 = __vmsum3fp(arDir.mV, v0).x;
        const f32 d1 = __vmsum3fp(arDir.mV, v1).x;
        const f32 d2 = __vmsum3fp(arDir.mV, v2).x;
        const f32 a0 = __vandc(__vmsum3fp(arDir.mV, e0), sign).x;
        const f32 a1 = __vandc(__vmsum3fp(arDir.mV, e1), sign).x;
        const f32 a2 = __vandc(__vmsum3fp(arDir.mV, e2), sign).x;
        int edge = -1;
        __vector4 point;
        if (d0 > d1 && d0 > d2)
        {
            point = v0;
            if (KF_EDGE_PERP_THRESHOLD > a0 && a2 > a0) edge = 0;
            else if (KF_EDGE_PERP_THRESHOLD > a2) edge = 2;
        }
        else if (d1 > d2)
        {
            point = v1;
            if (KF_EDGE_PERP_THRESHOLD > a1 && a2 > a1) edge = 1;
            else if (KF_EDGE_PERP_THRESHOLD > a2) edge = 2;
        }
        else
        {
            point = v2;
            if (KF_EDGE_PERP_THRESHOLD > a0 && a1 > a0) edge = 0;
            else if (KF_EDGE_PERP_THRESHOLD > a1) edge = 1;
        }
        if (edge == 0) SetTriangleEdge(arFeature, 0, v0, e0, __vspltw(len, 0));
        else if (edge == 1) SetTriangleEdge(arFeature, 0, v2, e1, __vspltw(len, 1));
        else if (edge == 2) SetTriangleEdge(arFeature, 0, v1, e2, __vspltw(len, 2));
        if (edge < 0) { arFeature.numedges = 0; arFeature.pt.mV = point; }
        else arFeature.numedges = 1;
    }
    arFeature.BuildEdgePlanes(abCcw, arDir);
}

// ===========================================================================
// rw::collision::GPTriangle::GetInterval @ 0x82BBA6A0
//
// Projection interval of the triangle onto a direction: the min/max of the
// three vertices' dot products with the query. The vertices are loaded from
// this+0x00 / this+0x20 / this+0x30 (Vertex0 / Vertex1 / Vertex2). Both rows
// are lane-broadcast VecFloats (min -> +0x00, max -> +0x10).
// ===========================================================================
void GPTriangle::GetInterval(math::vpu::Vector3 arDir, Interval& arInterval) const
{
    const __vector4 p0 = __vmsum3fp(arDir.mV, mPos.mV);
    const __vector4 p1 = __vmsum3fp(arDir.mV, mFaceNormals[1].mV);
    const __vector4 p2 = __vmsum3fp(arDir.mV, mFaceNormals[2].mV);
    arInterval.min.mV = __vminfp(__vminfp(p0, p1), p2);
    arInterval.max.mV = __vmaxfp(__vmaxfp(p0, p1), p2);
}

// ===========================================================================
// rw::collision::GPTriangle::GetIntervals @ 0x82BBA6E0
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals.
//
// Batched form of GetInterval: one projection interval per direction, dirs
// advancing 0x10 and intervals 0x30 (the console Interval array stride).
// ===========================================================================
void GPTriangle::GetIntervals(const math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    for (u32 i = 0; i < auNumDirs; ++i)
    {
        const __vector4 p0 = __vmsum3fp(lapDirs[i].mV, mPos.mV);
        const __vector4 p1 = __vmsum3fp(lapDirs[i].mV, mFaceNormals[1].mV);
        const __vector4 p2 = __vmsum3fp(lapDirs[i].mV, mFaceNormals[2].mV);
        lapIntervals[i].min.mV = __vminfp(__vminfp(p0, p1), p2);
        lapIntervals[i].max.mV = __vmaxfp(__vmaxfp(p0, p1), p2);
    }
}

// ===========================================================================
// rw::collision::TriangleNearestPointRegion @ 0x82BBA748   (waveQ5 C1)
//
// SEPARATE-ENTRY vs PART-OF-GetIntervals -- decided from the asm, not the
// ledger. IDA reports 0x82BBA748 INSIDE the function chunk
// 0x82BBA6E0..0x82BBAA00 (`rw::collision::GPTriangle::GetIntervals`), which is
// why the earlier notes called it "a mid-function bl target / shared tail".
// It is neither: GetIntervals' own loop ENDS with `blr` at 0x82BBA744, so
// there is no fall-through, and 0x82BBA748 opens a fresh body with its own
// argument contract. IDA simply merged two adjacent functions into one chunk
// (AGENTS gotcha 6: a truncated/merged IDA symbol is not evidence about the
// code). The single call site is `bl loc_82BBA748` @ 0x82BBB10C inside
// rw::collision::FatTriangleLineSegIntersect -- i.e. the console's TU is one
// run holding both the GP triangle callbacks and the triangle line tests, and
// this file owns the first half of that run. So: a standalone function, homed
// here beside GetIntervals exactly where the binary places it.
//
// It is a LEAF: no prologue, no stwu -- it scratches the linkage area at
// r1+0x00..0x0F and the red zone below r1 for its dot-product spills.
//
// X360 register map (__fastcall + VMX128 vector args, all confirmed at the
// 0x82BBB0F4..0x82BBB108 call site):
//   r3 = out nearest point   r4 = out s   r5 = out t
//   v1 = query point   v2 = V0   v3 = V1   v4 = V2
//   return (r3) = the feature region code.
//
// ** THERE IS NO FOURTH OUT-PARAMETER. ** The committed
// LineSegIntersect.hpp:180 declares an extra `f32* lpfPlaneDistance` on the
// claim that the callee returns a signed point-to-plane distance in f3 as a
// "same-TU register side channel". This body writes f0/f4..f13 and never
// touches f3 -- so it cannot. What the caller reads at 0x82BBB1D4
// (`fcmpu cr6, f3, f31`) is its OWN f3, set way back at 0x82BBAE30/0x82BBAE44
// to |det| and kept live across the call (an LTCG same-TU register-allocation
// artefact). Since |det| >= 0, that caller-side branch never fires -- which is
// exactly what the reconstruction does today, because it leaves its
// `lfPlaneDist` at its 0.0f seed. Behaviour therefore matches; the DECLARATION
// is what is wrong. Reported to the conductor with the exact edit (that file
// is outside this cluster's ownership).
//
// Algorithm: the standard barycentric point-vs-triangle nearest-feature query
// (Ericson's ClosestPtPointTriangle in its s/t determinant form), with
//   ab = V1-V0, ac = V2-V0, ap = V0-P   (note the sign: A minus P)
//   a = ab.ab   b = ab.ac   c = ac.ac   d = ab.ap   e = ac.ap
//   det = c*a - b*b     sNum = e*b - d*c     tNum = d*b - e*a
// Stage 1 picks one of four groups (three edge fans + the interior), stage 2
// resolves that group to a vertex / edge parameter / interior barycentric.
// The nearest point is rebuilt as V0 + ab*s + ac*t and s/t are published.
//
// VMX/FPU lowering notes (asm authoritative):
//   * vmsum3fp128 = xyz dot fold broadcast to all lanes -> scalar Dot3; every
//     dot is spilled to the stack and re-read with `lfs`, i.e. consumed as a
//     scalar -- so the whole classifier is scalar FP, not lane-wise.
//   * vrefp + two vnmsubfp/vmaddfp Newton-Raphson steps -> exact 1/det.
//   * BRANCH POLARITY (AGENTS gotcha 4): `bge X` after `fcmpu a,b` is
//     `if (!(a < b)) goto X` (NaN takes it), `ble X` is `if (!(a > b))`,
//     `blt`/`bgt` are the plain `<` / `>`. Every test below is written in the
//     form that reproduces the console's NaN behaviour exactly.
//   * `fneg` then `> 0` is kept as written rather than folded to `< 0`.
//
// Region codes (the values FatTriangleLineSegIntersect switches on):
//   0 = vertex V0   1 = vertex V1   2 = vertex V2
//   3 = edge V0V1   4 = edge V0V2   5 = edge V1V2
//   6 = face interior
// ===========================================================================

// flt_82180A24: raw 0x00200000 == 2^-128 (subnormal) -- the degenerate-area
// threshold on the barycentric determinant. The same .rdata word the committed
// LineSegIntersect.cpp names KF_PARALLEL_EPSILON_BITS.
static const f32 KF_DEGENERATE_AREA_EPSILON = F32FromBits(0x00200000u);

s32 TriangleNearestPointRegion(rw::math::vpu::Vector3* lpNearest, f32* lpfParamS, f32* lpfParamT,
                               rw::math::vpu::Vector3 aPoint, rw::math::vpu::Vector3 aV0, rw::math::vpu::Vector3 aV1, rw::math::vpu::Vector3 aV2)
{
    const rw::math::vpu::Vector3 lvAB = Sub(aV1, aV0);      // vsubfp v0,  v3, v2
    const rw::math::vpu::Vector3 lvAC = Sub(aV2, aV0);      // vsubfp v13, v4, v2
    const rw::math::vpu::Vector3 lvAP = Sub(aV0, aPoint);   // vsubfp v12, v2, v1   (V0 - P)

    const f32 lfA = Dot3(lvAB, lvAB);     // f8   -> r1+0x00
    const f32 lfB = Dot3(lvAB, lvAC);     // f0   -> var_40
    const f32 lfC = Dot3(lvAC, lvAC);     // f13  -> var_30
    const f32 lfD = Dot3(lvAB, lvAP);     // f10  -> var_20
    const f32 lfE = Dot3(lvAC, lvAP);     // f11  -> var_10

    const f32 lfDet  = linemath::ScalarMadd(lfC, lfA, -(lfB * lfB));   // fmuls + fmsubs -> var_5C
    const f32 lfSNum = linemath::ScalarMadd(lfE, lfB, -(lfD * lfC));   // f7  (fmuls + fmsubs)
    const f32 lfTNum = linemath::ScalarMadd(lfD, lfB, -(lfE * lfA));   // f6  (fmuls + fmsubs)

    // |V1V2|^2 == a - 2b + c (fnmsubs against flt_82001D9C == 2.0f, then
    // fadds). The asm recomputes this in both places that need it; the value
    // is identical, so it is hoisted once.
    const f32 lfEdge12LenSq = linemath::ScalarNmsub(lfB, 2.0f, lfA) + lfC;

    // ---- stage 1: pick the group -------------------------------------------
    // 0 = the V0V1 edge fan, 1 = the V0V2 edge fan, 2 = the V1V2 edge fan,
    // 3 = the triangle interior.
    s32 liGroup;
    if (lfDet < KF_DEGENERATE_AREA_EPSILON)   // fcmpu det, eps / bge -> non-degenerate
    {
        // Degenerate (near-zero area): resolve against the LONGEST edge.
        if (lfA > lfC)                                     // ble -> the c branch
        {
            liGroup = (lfA > lfEdge12LenSq) ? 0 : 2;       // ble -> 2
        }
        else
        {
            liGroup = (lfC > lfEdge12LenSq) ? 1 : 2;       // ble -> 2
        }
    }
    else if (lfSNum + lfTNum > lfDet)         // fadds + fcmpu / ble -> the other arm
    {
        // Beyond the V1V2 edge: V1 / V2 / the V1V2 fan.
        if (lfSNum < 0.0f)                                 // bge -> the t test
        {
            liGroup = !((lfE + lfC) < (lfD + lfB)) ? 2 : 1;   // bge -> 2
        }
        else if (!(lfTNum < 0.0f))                         // bge -> 2
        {
            liGroup = 2;
        }
        else
        {
            liGroup = ((lfD + lfA) < (lfE + lfB)) ? 0 : 2;    // blt -> 0
        }
    }
    else
    {
        // Inside the V1V2 half-plane: the interior, or one of the two fans
        // hanging off V0.
        if (lfSNum < 0.0f)                                 // bge -> the t test
        {
            liGroup = (-lfE > 0.0f) ? 1 : 0;               // fneg + bgt -> 1
        }
        else if (!(lfTNum < 0.0f))                         // bge -> 3
        {
            liGroup = 3;
        }
        else
        {
            liGroup = (-lfD > 0.0f) ? 0 : 1;               // fneg + bgt -> 0
        }
    }

    // ---- stage 2: resolve the group to a feature ---------------------------
    f32 lfS;
    f32 lfT;
    s32 liRegion;

    if (liGroup == 0)
    {
        // The V0V1 edge fan (loc_82BBA98C).
        if (!(lfD < 0.0f))          // fcmpu d, 0 / blt -> continue
        {
            lfS      = 0.0f;
            lfT      = 0.0f;
            liRegion = 0;           // vertex V0
        }
        else if (!(-lfD < lfA))     // fneg + fcmpu / blt -> continue
        {
            lfS      = 1.0f;        // flt_82001C98
            lfT      = 0.0f;
            liRegion = 1;           // vertex V1
        }
        else
        {
            lfS      = -(lfD / lfA);   // fdivs + fneg
            lfT      = 0.0f;
            liRegion = 3;           // edge V0V1
        }
    }
    else if (liGroup == 1)
    {
        // The V0V2 edge fan (loc_82BBA958): s is 0 on every path here.
        lfS = 0.0f;
        if (!(lfE < 0.0f))          // fcmpu e, 0 / blt -> continue
        {
            lfT      = 0.0f;
            liRegion = 0;           // vertex V0
        }
        else if (!(-lfE < lfC))     // fneg + fcmpu / bge -> vertex V2
        {
            lfT      = 1.0f;
            liRegion = 2;           // vertex V2
        }
        else
        {
            lfT      = -(lfE / lfC);   // fdivs + fneg
            liRegion = 4;           // edge V0V2
        }
    }
    else if (liGroup == 2)
    {
        // The V1V2 edge fan (loc_82BBA8F4). The parameter runs from V2 (0)
        // toward V1 (1): num == dot(V2-V1, V2-P), denominator == |V1V2|^2.
        const f32 lfNum = lfE + lfC - lfB - lfD;   // fadds + fsubs + fsubs
        if (!(lfNum > 0.0f))        // fcmpu num, 0 / bgt -> continue
        {
            lfS      = 0.0f;
            lfT      = 1.0f;
            liRegion = 2;           // vertex V2
        }
        else if (lfNum < lfEdge12LenSq)   // fcmpu / blt -> the edge case
        {
            lfS      = lfNum / lfEdge12LenSq;   // fdivs
            lfT      = 1.0f - lfS;              // fsubs against flt_82001C98
            liRegion = 5;           // edge V1V2
        }
        else
        {
            lfS      = 1.0f;
            lfT      = 0.0f;
            liRegion = 1;           // vertex V1
        }
    }
    else
    {
        // Interior (loc_82BBA8A4): vrefp + two Newton-Raphson steps == 1/det.
        const f32 lfInvDet = linemath::RefinedRecip(lfDet);
        lfS      = lfInvDet * lfSNum;   // fmuls
        lfT      = lfInvDet * lfTNum;   // fmuls
        liRegion = 6;                   // face interior
    }

    // ---- publish (store order as on X360) ----------------------------------
    *lpfParamS = lfS;   // stfs f12, 0(r4)
    *lpfParamT = lfT;   // stfs f0,  0(r5)

    // vmaddfp v0, ab, splat(s), V0  then  vmaddfp v0, ac, splat(t), v0
    // (all four lanes; stvx128 v0, r0, r9 with r9 = the saved r3).
    *lpNearest = MaddScalar(lvAC, lfT, MaddScalar(lvAB, lfS, aV0));

    return liRegion;
}

void GPTriangle::GetBBox(AABBox&) const {} // ARTIST ICF 0x82AD5078

} // namespace collision
} // namespace rw
