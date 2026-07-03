#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // sqrt, fabs

// ===========================================================================
// rw::collision::GPCylinder -- the cylinder primitive's VolumeMethods
// callbacks, reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass
// wave 2).
//
//   GPCylinder::GetMaximumFeature  @ 0x82BAE430   (VolumeMethods +0xA4)
//   GPCylinder::GetInterval        @ 0x82BAD7F8   (VolumeMethods +0xA8)
//   GPCylinder::GetIntervals       @ 0x82BAD938   (VolumeMethods +0xAC)
//
// Canonical declarations Feb-2007 rwccore.h:1264-1267 (non-static const
// members on PS3); reconstructed as static plain-function callbacks matching
// the committed GPInstance::VolumeMethods typedefs per the X360 delta note in
// GPInstance.hpp. Instance layout per the canonical GPCylinder::Initialize
// (rwccore.h:1390): mFaceNormals[0] = the axis (== mEdgeDirections[0]),
// mFaceNormals[1]/[2] = the perpendicular frame axes, mDimensions.x = the
// half-height, mDimensions.y = the radius.
//
// VMX lowering notes shared by all three bodies: vrsqrtefp + 2x Newton-
// Raphson -> exact 1/sqrt; vrefp + 2x Newton-Raphson -> exact 1/x;
// lenSq * rsqrt(lenSq) with vcmpeqfp/vsel -> zero-guarded length; the
// vpermwi128 0x63 / vmulfp128 / vnmsubfp / vpermwi128 idiom -> Cross;
// vrlimi128 vD, vB, 2, 0 -> insert vB's z lane into vD; vxor with the
// vspltisw(-1)/vslw sign mask -> per-lane negation. vmaddfp operand rule
// (multiplier LAST, addend THIRD) checked at every FMA.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; broadcast lane 0).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // stvx128 of a lane-0 vspltw broadcast: all four lanes carry the scalar.
    inline Vec4 Splat(f32 s)
    {
        Vec4 r;
        r.x = s;
        r.y = s;
        r.z = s;
        r.w = s;
        return r;
    }

    // vsubfp: per-lane a - b.
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    // vmulfp128 against a splat: per-lane a * s.
    inline Vec4 Scale(const Vec4& a, f32 s)
    {
        Vec4 r;
        r.x = a.x * s;
        r.y = a.y * s;
        r.z = a.z * s;
        r.w = a.w * s;
        return r;
    }

    // vmaddfp against a splat multiplier: per-lane a*s + b.
    inline Vec4 MaddScalar(const Vec4& a, f32 s, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x * s + b.x;
        r.y = a.y * s + b.y;
        r.z = a.z * s + b.z;
        r.w = a.w * s + b.w;
        return r;
    }

    // vspltisw(-1)/vslw sign mask + vxor: flip every lane's sign bit.
    inline Vec4 Negate(const Vec4& a)
    {
        Vec4 r;
        r.x = -a.x;
        r.y = -a.y;
        r.z = -a.z;
        r.w = -a.w;
        return r;
    }

    // Cross product a x b via the VMX two-permute idiom; the w lane cancels
    // to 0.
    inline Vec4 Cross(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = a.w * b.w - a.w * b.w;   // == 0 (carried for fidelity)
        return r;
    }

    // Lane-by-lane stack quad build (stfs/stw staging rows).
    inline Vec4 MakeVec4(f32 x, f32 y, f32 z, f32 w)
    {
        Vec4 r;
        r.x = x;
        r.y = y;
        r.z = z;
        r.w = w;
        return r;
    }
}

// flt_821803A0: |cos| threshold above which the direction counts as
// axis-aligned and the end cap is the maximum feature. Value attested by the
// export's pseudocode compare (`> 0.99980003`).
static const f32 KF_END_FACE_COS_THRESHOLD = 0.99980003f;

// flt_82005574: |cos| threshold below which the direction counts as
// perpendicular to the axis and the side line is the maximum feature. Value
// attested by the export's pseudocode compare (`<= 0.02`).
static const f32 KF_SIDE_EDGE_COS_THRESHOLD = 0.02f;

// flt_821802F8: the octagon diagonal-vertex coefficient, cos(45deg). Value
// attested by the export's pseudocode multiply (`* 0.70710677`).
static const f32 KF_OCTAGON_DIAGONAL = 0.70710677f;

// ===========================================================================
// rw::collision::GPCylinder::GetMaximumFeature @ 0x82BAE430
// Reached through mMethods.mGetMaximumFeature by the committed
// PrimitiveIntersect.cpp batch kernels (pass 2) and ComputeContactPoints.
//
// Structure (asm-attested):
//   0. Fold the direction into the cylinder's LOCAL frame with a vmrghw/
//      vmrglw 3x3 transpose + column FMA: localDir = { dot(fn[2], dir),
//      dot(fn[1], dir), dot(fn[0], dir), 0 } (fn[0] == the axis).
//   A. |localDir.z| > 0.99980003: END-CAP FACE -- an 8-vertex octagon at
//      z = +/-halfHeight, 8 FeatureEdge records, numedges = 8. The winding
//      flag is toggled instead of the loop normal when the cap faces -Z
//      (v121 stays {0,0,1,0} for the prism-wall crosses; only the STORED
//      ownNormal is sign-flipped).
//   C. |localDir.z| <= 0.02: SIDE EDGE -- the axis-parallel rim line nearest
//      the direction, one FeatureEdge, numedges = 1. Its pn row is NEVER
//      staged: the console memcpy carries 16 bytes of leftover stack into
//      edges[0].pn (left unwritten here; no consumer reads an edge-feature pn).
//   D. otherwise: RIM POINT -- radius * normalised XY of localDir with
//      z = +/-halfHeight, numedges = 0, feature.pt written.
//   T. Tail (all paths): rotate ownNormal / translate+rotate pt into world
//      space, then rotate/translate every edge record's dir/base/pn rows
//      (length untouched). On paths A and C feature.pt (+0x220) was never
//      written -- the console transforms whatever the caller's slot held;
//      preserved.
//
// rodata: flt_82001CC0 = 0.0f / flt_82001C98 = 1.0f (committed
// SeparatingDirection.cpp rodata note); the three thresholds above.
// ===========================================================================
void GPCylinder::GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                   const Vec4& arDir, Feature& arFeature)
{
    // Local-frame rows (lvx128 this+0x00/+0x30/+0x20/+0x10). Local X rides on
    // mFaceNormals[2], Y on mFaceNormals[1], Z on mFaceNormals[0] == the axis.
    const Vec4 lvPos   = lpThis->mPos;               // v119
    const Vec4 lvAxisX = lpThis->mFaceNormals[2];    // v124
    const Vec4 lvAxisY = lpThis->mFaceNormals[1];    // v123
    const Vec4 lvAxisZ = lpThis->mFaceNormals[0];    // v122 (cylinder axis)

    // vmrghw/vmrglw 3x3 transpose + column FMA fold: the direction in the
    // cylinder's local frame. The merged fourth column is all zeros, so the
    // w lane is exactly 0.
    Vec4 lvLocalDir;                                 // v127 -> var_230
    lvLocalDir.x = Dot3(lvAxisX, arDir);
    lvLocalDir.y = Dot3(lvAxisY, arDir);
    lvLocalDir.z = Dot3(lvAxisZ, arDir);
    lvLocalDir.w = 0.0f;

    const f32 lfLocalZ = lvLocalDir.z;               // lfs var_228 (f0)

    if (std::fabs(lfLocalZ) > KF_END_FACE_COS_THRESHOLD)
    {
        // ================= branch A: END-CAP FACE (8 edges) =================
        // Local cap normal, built lane-by-lane from the 0.0f/1.0f rodata
        // (var_220 quad = {0,0,1,0}) -- v121. The loop's prism-wall crosses
        // keep using the UN-flipped +Z copy even when the -Z cap is selected;
        // only the winding flag and the stored ownNormal flip.
        const Vec4 lvLocalNormal = MakeVec4(0.0f, 0.0f, 1.0f, 0.0f);

        arFeature.ownNormal = lvLocalNormal;   // stvx128 v121 -> +0x210

        f32    lfZ   = lpThis->mDimensions.x;  // vspltw dims lane 0: halfHeight
        RwBool lbCcw = abCcw;                  // r23
        if (lfLocalZ < 0.0f)                   // fcmpu/bge polarity preserved
        {
            lbCcw = (abCcw == 0);              // cntlzw + extrwi bit trick
            // vxor128 v121 ^ signmask stored (v121 itself NOT updated):
            // ownNormal = {-0,-0,-1,-0}.
            arFeature.ownNormal = Negate(lvLocalNormal);
            lfZ = -lfZ;                        // splat(halfHeight) ^ signmask
        }

        const f32 lfRadius = lpThis->mDimensions.y;   // vspltw dims lane 1
        const f32 lfDiag   = lfRadius * KF_OCTAGON_DIAGONAL;   // fmuls

        // The 8 octagon vertices in the cap plane z = lfZ, staged into the
        // ascending stack run var_160..var_F0 (slot order preserved).
        Vec4 laOctagon[8];
        laOctagon[0] = MakeVec4(0.0f,      lfRadius,  lfZ, 0.0f);   // var_160
        laOctagon[1] = MakeVec4(lfDiag,    lfDiag,    lfZ, 0.0f);   // var_150
        laOctagon[2] = MakeVec4(lfRadius,  0.0f,      lfZ, 0.0f);   // var_140
        laOctagon[3] = MakeVec4(lfDiag,   -lfDiag,    lfZ, 0.0f);   // var_130
        laOctagon[4] = MakeVec4(0.0f,     -lfRadius,  lfZ, 0.0f);   // var_120
        laOctagon[5] = MakeVec4(-lfDiag,  -lfDiag,    lfZ, 0.0f);   // var_110
        laOctagon[6] = MakeVec4(-lfRadius, 0.0f,      lfZ, 0.0f);   // var_100
        laOctagon[7] = MakeVec4(-lfDiag,   lfDiag,    lfZ, 0.0f);   // var_F0

        // 8 iterations (r29 = 0..7); the ccw test sits INSIDE the loop.
        for (u32 lu = 0; lu < 8; ++lu)
        {
            Vec4 lvBase;       // v13 -> staged base row (var_1A0)
            Vec4 lvEdgeVec;    // v12
            Vec4 lvCrossRaw;   // v10 -> unnormalised prism-wall direction
            if (lbCcw != 0)
            {
                // Descending r26 cursor: base = P[7-i]; the ring successor
                // comes from the (16 * r27) & 0x70 slot with r27 = -2 - i.
                lvBase     = laOctagon[7 - lu];
                lvEdgeVec  = Sub(laOctagon[(6u - lu) & 7], lvBase);   // vsubfp
                // e * perm(n) - perm(e) * n, permuted = cross(edge, +Z).
                lvCrossRaw = Cross(lvEdgeVec, lvLocalNormal);
            }
            else
            {
                // Ascending r25 cursor: base = P[i], successor P[(i+1)&7].
                lvBase     = laOctagon[lu];
                lvEdgeVec  = Sub(laOctagon[(lu + 1u) & 7], lvBase);
                // n * perm(e) - perm(n) * e, permuted = cross(+Z, edge).
                lvCrossRaw = Cross(lvLocalNormal, lvEdgeVec);
            }

            // vmsum3fp128; guarded length = lenSq * rsqrt(lenSq) (vrsqrtefp +
            // 2x Newton-Raphson), vsel'd to 0 when lenSq == 0.
            const f32 lfLenSq  = Dot3(lvEdgeVec, lvEdgeVec);
            const f32 lfLength = (lfLenSq == 0.0f)
                                     ? 0.0f
                                     : lfLenSq * (1.0f / std::sqrt(lfLenSq));

            // vrefp + two Newton-Raphson refines of 1/length feed BOTH the
            // unit edge direction and the prism normal (1/0 -> inf lanes,
            // exactly as vrefp(0) -- unreachable for this octagon).
            const f32 lfInvLength = 1.0f / lfLength;

            // memcpy(edges cursor, staging, 0x40): the four staged rows.
            FeatureEdge& lrEdge = arFeature.edges[lu];      // r28 += 0x40
            lrEdge.base   = lvBase;                          // var_1A0 +0x00
            lrEdge.dir    = Scale(lvEdgeVec, lfInvLength);   // var_190 +0x10
            lrEdge.pn     = Scale(lvCrossRaw, lfInvLength);  // var_180 +0x20
            lrEdge.length = Splat(lfLength);                 // var_170 +0x30
        }

        arFeature.numedges = 8;   // LABEL_15: stw r11(8), 0x230(r30)
    }
    else if (std::fabs(lfLocalZ) <= KF_SIDE_EDGE_COS_THRESHOLD)
    {
        // ================= branch C: SIDE EDGE (1 edge) =====================
        // Unguarded normalisation of the FULL local direction (vrsqrtefp +
        // 2x Newton-Raphson, no vsel on this pipeline).
        const f32  lfDirLenSq = Dot3(lvLocalDir, lvLocalDir);   // v12
        const Vec4 lvUnitDir  = Scale(lvLocalDir,
                                      1.0f / std::sqrt(lfDirLenSq));

        const f32 lfHalfHeight = lpThis->mDimensions.x;   // vspltw lane 0
        const f32 lfRadius     = lpThis->mDimensions.y;   // vspltw lane 1

        // Rim point at the +z cap: radius * unitDir with the z lane replaced
        // by +halfHeight (vrlimi128 v11, v9, 2, 0); the -z copy replaces it
        // with -halfHeight (vrlimi128 v12, v8, 2, 0).
        Vec4 lvTop = Scale(lvUnitDir, lfRadius);   // v11
        lvTop.z = lfHalfHeight;
        Vec4 lvBottom = lvTop;                     // vmr v12, v11
        lvBottom.z = -lfHalfHeight;

        const Vec4 lvUp   = Sub(lvTop, lvBottom);  // v12 = {0,0,+2h,0}
        const Vec4 lvDown = Sub(lvBottom, lvTop);  // v9  = {0,0,-2h,0}

        // Guarded length of the up vector -> the FeatureEdge length row
        // (vrsqrtefp + 2x NR; vcmpeqfp v4 / vsel against zero).
        const f32 lfLenSq  = Dot3(lvUp, lvUp);
        const f32 lfLength = (lfLenSq == 0.0f)
                                 ? 0.0f
                                 : lfLenSq * (1.0f / std::sqrt(lfLenSq));

        // The down vector is normalised through its OWN (unguarded) rsqrt
        // pipeline (vmsum3fp128 v10 + vrsqrtefp v12 + 2x NR).
        const f32 lfDownLenSq = Dot3(lvDown, lvDown);

        // memcpy(feature+0x10, staging, 0x40). The pn row (var_180) is never
        // staged on this path -- the console copies 16 bytes of leftover
        // stack into edges[0].pn; left unwritten here (indeterminate either
        // way, no consumer reads an edge-feature pn).
        FeatureEdge& lrEdge = arFeature.edges[0];
        lrEdge.base   = lvTop;                                        // var_1A0
        lrEdge.dir    = Scale(lvDown, 1.0f / std::sqrt(lfDownLenSq)); // var_190
        lrEdge.length = Splat(lfLength);                              // var_170

        // Post-memcpy: ownNormal = the untouched local direction (v127).
        arFeature.ownNormal = lvLocalDir;   // stvx128 v127 -> +0x210
        arFeature.numedges  = 1;            // LABEL_15: stw r11(1)
    }
    else
    {
        // ================= branch D: RIM POINT (0 edges) ====================
        // Zero the z lane of the local direction (stfs 0.0f into the staged
        // quad), then normalise the radial part -- unguarded rsqrt pipeline.
        Vec4 lvRadial = lvLocalDir;          // stvx128 v127 -> var_230
        lvRadial.z = 0.0f;                   // var_228 = flt_82001CC0
        const f32  lfXYLenSq = Dot3(lvRadial, lvRadial);   // vmsum3fp128
        const Vec4 lvUnitXY  = Scale(lvRadial, 1.0f / std::sqrt(lfXYLenSq));

        // point = radius * unitXY with z = +halfHeight inserted (vspltw dims
        // lane 1 multiply; vrlimi128 v0, splat(dims lane 0), 2, 0).
        Vec4 lvPoint = Scale(lvUnitXY, lpThis->mDimensions.y);
        lvPoint.z = lpThis->mDimensions.x;
        if (lfLocalZ < 0.0f)                 // fcmpu/bge polarity preserved
        {
            // splat lane 2 ^ signmask, re-inserted: z = -halfHeight.
            lvPoint.z = -lvPoint.z;
        }

        arFeature.numedges  = 0;             // stw r31, 0x230(r30)
        arFeature.ownNormal = lvLocalDir;    // stvx128 v127 (full, z included)
        arFeature.pt        = lvPoint;       // stvx128 v0 -> +0x220
    }

    // ===================== LABEL_16: local -> world =========================
    // ownNormal is rotated (no translation); pt is rotated AND translated by
    // mPos. On branches A and C feature.pt was never written -- the console
    // transforms whatever the caller's +0x220 slot held; preserved here.
    {
        const Vec4 lvLocalN  = arFeature.ownNormal;   // lvx128 [r24]
        const Vec4 lvLocalPt = arFeature.pt;          // lvx128 [r30+0x220]

        Vec4 lvWorldN = Scale(lvAxisX, lvLocalN.x);             // v124 * splat0
        lvWorldN = MaddScalar(lvAxisY, lvLocalN.y, lvWorldN);   // vmaddfp128
        lvWorldN = MaddScalar(lvAxisZ, lvLocalN.z, lvWorldN);
        arFeature.ownNormal = lvWorldN;                         // stvx128 [r24]

        Vec4 lvWorldPt = MaddScalar(lvAxisX, lvLocalPt.x, lvPos);   // v119 seed
        lvWorldPt = MaddScalar(lvAxisY, lvLocalPt.y, lvWorldPt);
        lvWorldPt = MaddScalar(lvAxisZ, lvLocalPt.z, lvWorldPt);
        arFeature.pt = lvWorldPt;                        // stvx128 [r30+0x220]
    }

    // cmpwi (signed) numedges > 0: rotate/translate every edge record.
    // The console reloads numedges from +0x230 each iteration (no writes to
    // it inside the loop, so the do-while collapses cleanly).
    if (arFeature.numedges > 0)
    {
        s32 li = 0;   // r31 (0 on entry to every branch)
        do
        {
            FeatureEdge& lrEdge = arFeature.edges[li];   // r11 = dir row cursor
            const Vec4 lvDir  = lrEdge.dir;    // lvx128 [r11]
            const Vec4 lvBase = lrEdge.base;   // lvx128 [r11-0x10]
            const Vec4 lvPn   = lrEdge.pn;     // lvx128 [r11+0x10]

            // dir: rotation only.
            Vec4 lvWorldDir = Scale(lvAxisX, lvDir.x);
            lvWorldDir = MaddScalar(lvAxisY, lvDir.y, lvWorldDir);
            lvWorldDir = MaddScalar(lvAxisZ, lvDir.z, lvWorldDir);
            lrEdge.dir = lvWorldDir;                     // stored first

            // base: rotation + mPos translation (v8 seeded from v119).
            Vec4 lvWorldBase = MaddScalar(lvAxisX, lvBase.x, lvPos);
            lvWorldBase = MaddScalar(lvAxisY, lvBase.y, lvWorldBase);
            lvWorldBase = MaddScalar(lvAxisZ, lvBase.z, lvWorldBase);

            // pn: rotation only.
            Vec4 lvWorldPn = Scale(lvAxisX, lvPn.x);
            lvWorldPn = MaddScalar(lvAxisY, lvPn.y, lvWorldPn);
            lvWorldPn = MaddScalar(lvAxisZ, lvPn.z, lvWorldPn);

            lrEdge.base = lvWorldBase;                   // stvx128 [r10]
            lrEdge.pn   = lvWorldPn;                     // stvx128 [r9]
            // length row untouched.

            ++li;   // addi r31, r31, 1; edge cursor r11 += 0x40
        }
        while (li < arFeature.numedges);   // lwz 0x230(r30) / cmpw / blt
    }
}

// ===========================================================================
// rw::collision::GPCylinder::GetInterval @ 0x82BAD7F8
//
//   centreDot = dot3(mPos, dir); axisDot = dot3(mEdgeDirections[0], dir);
//   sinSq = |1.0 - axisDot^2| (fnmsubs + fabs);
//   sin   = (sinSq == 0) ? 0 : sinSq * rsqrt(sinSq)  (guarded sqrt);
//   proj  = halfHeight * |axisDot| + radius * sin;
//   min = centreDot - |proj|; max = |proj| + centreDot (this operand order);
//   interval.min/max = lane-0 broadcasts (padding words NOT touched).
// Hex-Rays's "int result" return is the r3 pass-through artifact -- void.
// ===========================================================================
void GPCylinder::GetInterval(const GPInstance* lpThis,
                             const Vec4& arDir, Interval& arInterval)
{
    // vmsum3fp128 v9 = dot3(mPos, dir); v11 = dot3(mEdgeDirections[0], dir)
    // (the cylinder axis lives in mEdgeDirections[0], canonical Axis()).
    const f32 lfCentreDot = Dot3(lpThis->mPos, arDir);                // v9
    const f32 lfAxisDot   = Dot3(lpThis->mEdgeDirections[0], arDir);  // v11

    // fnmsubs f13 = -(axisDot*axisDot - 1.0f) = 1 - axisDot^2
    // (flt_82001C98 = 1.0f), then fabs on both the term and the dot.
    const f32 lfSinSq      = 1.0f - lfAxisDot * lfAxisDot;    // f13
    const f32 lfAbsAxisDot = std::fabs(lfAxisDot);            // -> var_40 quad
    const f32 lfAbsSinSq   = std::fabs(lfSinSq);              // -> var_50 quad

    // Guarded sqrt: vrsqrtefp + two Newton-Raphson refines (vcfsx 1.0/0.5
    // constants), x * rsqrt(x), forced to 0 by vcmpeqfp/vsel when x == 0.
    const f32 lfSin = (lfAbsSinSq == 0.0f)
                          ? 0.0f
                          : lfAbsSinSq * (1.0f / std::sqrt(lfAbsSinSq));

    // v0 = splat(radius) * sin; v0 = splat(halfHeight) * splat(|axisDot|) + v0
    // (vmaddfp v0, v7, v0, v5: multiplier v5 is the LAST-listed operand).
    const f32 lfProj = lpThis->mDimensions.x * lfAbsAxisDot
                     + lpThis->mDimensions.y * lfSin;

    // fsubs f13 = centreDot - |proj|; fadds f0 = |proj| + centreDot
    // (the console fabs's the projection twice, f13/f12; proj is already
    // non-negative for sane inputs -- preserved regardless).
    const f32 lfMin = lfCentreDot - std::fabs(lfProj);
    const f32 lfMax = std::fabs(lfProj) + lfCentreDot;

    // Lane-0 broadcasts stored at out+0x00 / out+0x10 (stvx128 v0, r4 /
    // stvx128 v13, r4+0x10). Interval::padding is not written.
    arInterval.min = Splat(lfMin);
    arInterval.max = Splat(lfMax);
}

// ===========================================================================
// rw::collision::GPCylinder::GetIntervals @ 0x82BAD938
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals. Per direction, the identical math to GetInterval;
// the instance rows are reloaded every iteration on the console -- kept
// inside the loop body here. Output stride 0x30 == sizeof(Interval)
// (`addi r10, r10, 0x30`); the Interval padding words are NOT touched.
// ===========================================================================
void GPCylinder::GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                              u32 auNumDirs, Interval* lapIntervals)
{
    // cmplwi cr6, r7, 0 / beq: zero directions -> nothing to do.
    if (auNumDirs == 0)
    {
        return;
    }

    const Vec4* lpDir      = lapDirs;       // r6, += 0x10
    Interval*   lpInterval = lapIntervals;  // r10, += 0x30
    u32         luRemaining = auNumDirs;    // r7, addic. decrement

    do
    {
        // Instance rows reloaded per iteration on the console (lvx128 inside
        // the loop body): axis @+0x40, centre @+0x00, dimensions @+0x70.
        const f32 lfAxisDot   = Dot3(lpThis->mEdgeDirections[0], *lpDir);  // v13
        const f32 lfCentreDot = Dot3(lpThis->mPos, *lpDir);                // v0

        // fnmsubs f13 = -(axisDot*axisDot - 1.0f) = 1 - axisDot^2, then fabs
        // on both the term and the dot (staged as {value,0,0,0} quads).
        const f32 lfSinSq      = 1.0f - lfAxisDot * lfAxisDot;     // f13
        const f32 lfAbsAxisDot = std::fabs(lfAxisDot);             // var_70
        const f32 lfAbsSinSq   = std::fabs(lfSinSq);               // var_80

        // vrsqrtefp + 2x Newton-Raphson (vcfsx 1.0/0.5), x * rsqrt(x), forced
        // to 0 by vcmpeqfp v3 / vsel when x == 0 -- the guarded sqrt.
        const f32 lfSin = (lfAbsSinSq == 0.0f)
                              ? 0.0f
                              : lfAbsSinSq * (1.0f / std::sqrt(lfAbsSinSq));

        // v0 = splat(radius) * sin; vmaddfp v0, v7, v0, v6:
        // v0 = splat(halfHeight) * splat(|axisDot|) + v0.
        const f32 lfProj = lpThis->mDimensions.x * lfAbsAxisDot
                         + lpThis->mDimensions.y * lfSin;

        // fsubs f11 = centreDot - |proj|; fadds f0 = |proj| + centreDot
        // (fabs'd twice, f11/f0, exactly as GetInterval does).
        const f32 lfMin = lfCentreDot - std::fabs(lfProj);
        const f32 lfMax = std::fabs(lfProj) + lfCentreDot;

        // Lane-0 broadcasts stored at r10+0x00 / r10+0x10; padding untouched.
        lpInterval->min = Splat(lfMin);
        lpInterval->max = Splat(lfMax);

        ++lpDir;         // addi r6, r6, 0x10
        ++lpInterval;    // addi r10, r10, 0x30 (== sizeof(Interval))
    }
    while (--luRemaining != 0);   // addic. / bne
}

} // namespace collision
} // namespace rw
