#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // sqrt, fabs

// ===========================================================================
// rw::collision::GPBox -- the box primitive's VolumeMethods callbacks,
// reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   GPBox::GetMaximumFeature  @ 0x82BA8918   (VolumeMethods +0xA4)
//   GPBox::GetInterval        @ 0x82BA8650   (VolumeMethods +0xA8)
//   GPBox::GetIntervals       @ 0x82BA9238   (VolumeMethods +0xAC)
//
// Canonical declarations Feb-2007 rwccore.h:1216-1218 (non-static const
// members on PS3); reconstructed as static plain-function callbacks matching
// the committed GPInstance::VolumeMethods typedefs per the X360 delta note in
// GPInstance.hpp. Instance layout: mPos = centre, mFaceNormals[0..2] = the
// unit face axes, mDimensions = the half-extents.
//
// GPBox::GetBBox exists in the binary at another address outside this TU and
// is not homed here.
//
// VMX decode notes (asm authoritative):
//   * vmsum3fp128 = xyz dot fold broadcast to all lanes -> scalar Dot3 + Splat.
//   * The lvsl rN / vspltw / vperm triples build lane-splat permute controls
//     that broadcast mDimensions element 2 / 1 / 0 respectively.
//   * vspltisw v0,-1 + vslw = 0x80000000 per lane; vandc against it strips
//     the sign bit = fabs.
//   * vmaddfp vD, vA, vB, vC computes vD = vA*vC + vB (multiplier last,
//     addend third); vnmsubfp vD = -(vA*vC - vB). The vmaddfp128
//     v127, v13, v0, v127 prints are the destructive VMX128 3-op form
//     (vD = vA*vB + vD).
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

    // A vmsum3fp128/vspltw broadcast row: the scalar in all four lanes.
    inline Vec4 Splat(f32 s)
    {
        Vec4 r;
        r.x = s;
        r.y = s;
        r.z = s;
        r.w = s;
        return r;
    }

    // vaddfp / vsubfp: per-lane a +/- b.
    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

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

    // vspltisw(-1)/vslw sign mask + vxor: flip every lane's sign.
    inline Vec4 Negate(const Vec4& a)
    {
        Vec4 r;
        r.x = -a.x;
        r.y = -a.y;
        r.z = -a.z;
        r.w = -a.w;
        return r;
    }

    // Cross product a x b via the VMX two-permute idiom (vpermwi128 0x63 +
    // vmulfp128 + vnmsubfp + vpermwi128); the w lane cancels to 0.
    inline Vec4 Cross(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = a.w * b.w - a.w * b.w;   // == 0 (carried for fidelity)
        return r;
    }

    // Runtime lane pick: the asm's lvsl (4*index) / vspltw / vperm
    // splat-by-index recipe, collapsed to the selected scalar lane.
    inline f32 Lane(const Vec4& v, s32 i)
    {
        switch (i)
        {
            case 0:  return v.x;
            case 1:  return v.y;
            case 2:  return v.z;
            default: return v.w;
        }
    }
}

// flt_82004744: the |dot| classification gate of the axis loop (value from
// the export's pseudocode literal).
static const f32 KF_FACE_DOT_THRESHOLD = 0.2f;

// flt_821801B4: the prism-wall normalisation guard (pseudocode literal;
// == FLT_EPSILON, same value/role as SeparatingDirection.cpp's
// KF_SEP_DIR_EPSILON).
static const f32 KF_WALL_NORMALISE_EPSILON = 1.1920929e-07f;

// .data 0x82F91794..0x82F917DB -- the GPBox feature tables. The export
// carries NO dumped values for this block; every entry below is DERIVED:
//
// * KAI_OTHER_AXES (dword_82F917D4/dword_82F917D8, 3 pairs): for axis i the
//   other two axes in ASCENDING order {1,2},{0,2},{0,1}. Pinned three ways:
//   (a) each entry must name the two axes != i for the face/edge geometry to
//   be a face/edge of the box at all; (b) the fneg-for-axis-1-only winding
//   fix (0x82BA8A34) requires the middle row to be the anti-cyclic (0,2)
//   ordering (a cyclic (2,0) row would need no fix); (c) the vertex case
//   hardcodes lanes (1,2) as the pair for axis 0's +/- enumeration == row 0.
// * KAI_CORNER_INDICES (dword_82F917B4/dword_82F917B8, 4 pairs of 0/1
//   selecting +/-dims[u] / +/-dims[v] corner offsets): UNIQUELY forced by
//   ring closure -- base[k+1] == base[k] + dir[k]*length[k] must close in
//   BOTH winding loops (the committed Find*Prism consumers walk the edges as
//   a closed ring). The unique solution is (+,+),(+,-),(-,-),(-,+).
// * KAF_CORNER_PARAMS (flt_82F91794/flt_82F91798, 4 pairs of +/-1.0f): the
//   SET must be all four (+/-1, +/-1) combinations (the edge/vertex scans
//   otherwise miss extremal features); the ORDER is FLAGGED AS INFERRED to
//   mirror the adjacent corner-index ring (same .data block, index 0 <-> +1,
//   1 <-> -1). Order affects only tie-breaks between equally-projecting
//   corners/edges (exact-tie directions).

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
void GPBox::GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                              const Vec4& arDir, Feature& arFeature)
{
    // ---- Phase 1: classify the direction against the three face axes.
    // (loop 0x82BA8964..0x82BA89A0: lvx128 [this+0x10+0x10*i] + vmsum3fp128,
    //  fabs/fcmpu against flt_82004744 = 0.2f)
    s32 liPerpCount   = 0;   // r8:  axes with |dot3(dir, fn[i])| < 0.2
    s32 liPerpAxis    = 0;   // r30: last such axis
    s32 liAlignedAxis = 0;   // r10: last axis with |dot| >= 0.2
    for (s32 li = 0; li < 3; ++li)
    {
        const f32 lfDot = Dot3(arDir, lpThis->mFaceNormals[li]);
        if (std::fabs(lfDot) >= KF_FACE_DOT_THRESHOLD)   // bge cr6
        {
            liAlignedAxis = li;
        }
        else
        {
            ++liPerpCount;
            liPerpAxis = li;
        }
    }

    if (liPerpCount == 2)
    {
        // ===================================================================
        // FACE feature along the aligned axis.
        // ===================================================================
        const Vec4& lrAxis   = lpThis->mFaceNormals[liAlignedAxis];   // lvx128 [r25]
        const f32   lfDimAxis = Lane(lpThis->mDimensions, liAlignedAxis);

        // fcmpu dot vs flt_82001CC0 (0.0f): ble -> -1.0f (flt_820037C8),
        // else +1.0f (flt_82001C98).
        f32 lfSign = (Dot3(lrAxis, arDir) > 0.0f) ? 1.0f : -1.0f;   // f31

        // v124 quad {sign,0,0,0} is staged BEFORE the axis-1 winding flip:
        // the face centre uses the ORIGINAL sign.
        const f32 lfFaceSign = lfSign;

        // vmaddfp128 v127 (VMX128 3-op FMA): mPos + axis*(dims[axis]*sign).
        const Vec4 lvFaceCentre =
            MaddScalar(lrAxis, lfDimAxis * lfFaceSign, lpThis->mPos);

        // fneg f31 for axis 1 only (0x82BA8A34): the ascending (0,2) in-plane
        // pair anti-commutes the winding for the middle axis.
        if (liAlignedAxis == 1)
        {
            lfSign = -lfSign;
        }

        // The in-plane axis pair (dword_82F917D4/D8 [2*axis]).
        const s32 liAxisU = KAI_OTHER_AXES[liAlignedAxis][0];   // v20
        const s32 liAxisV = KAI_OTHER_AXES[liAlignedAxis][1];   // v21

        const Vec4& lrFnU  = lpThis->mFaceNormals[liAxisU];   // lvx128 16*(u+1)+r3
        const Vec4& lrFnV  = lpThis->mFaceNormals[liAxisV];   // lvx128 16*(v+1)+r3
        const f32   lfDimU = Lane(lpThis->mDimensions, liAxisU);
        const f32   lfDimV = Lane(lpThis->mDimensions, liAxisV);

        // In-plane corner offsets (vperm dim splats, vxor sign flips,
        // vmulfp128; staged at sp var_160/var_150 and var_C0/var_B0).
        Vec4 laOffsetU[2];
        laOffsetU[0] = Scale(lrFnU, lfDimU);    // var_160: +u offset
        laOffsetU[1] = Scale(lrFnU, -lfDimU);   // var_150: -u offset
        Vec4 laOffsetV[2];
        laOffsetV[0] = Scale(lrFnV, lfDimV);    // var_C0:  +v offset
        laOffsetV[1] = Scale(lrFnV, -lfDimV);   // var_B0:  -v offset

        // Edge-direction ring (staged at var_A0/var_90/var_80/var_70).
        Vec4 laEdgeDir[4];
        laEdgeDir[0] = Negate(lrFnV);   // var_A0
        laEdgeDir[1] = Negate(lrFnU);   // var_90
        laEdgeDir[2] = lrFnV;           // var_80
        laEdgeDir[3] = lrFnU;           // var_70

        // Prism-wall plane normals: cross of each in-plane face normal with
        // the winding extrusion direction -- cross(fn, dir) when ccw != 0
        // (branch @ 0x82BA8B40), cross(dir, fn) otherwise (@ 0x82BA8BD8).
        // Each raw cross is stored, then overwritten by its normalised form
        // when the squared length beats flt_821801B4 (vrsqrtefp + TWO Newton-
        // Raphson refines, rendered exact per the committed precedent).
        Vec4 lvWallV = (abCcw != 0) ? Cross(lrFnV, arDir)    // var_120 raw
                                    : Cross(arDir, lrFnV);
        {
            const f32 lfLenSq = Dot3(lvWallV, lvWallV);      // vmsum3fp128
            if (lfLenSq > KF_WALL_NORMALISE_EPSILON)
            {
                lvWallV = Scale(lvWallV, 1.0f / std::sqrt(lfLenSq));
            }
        }
        Vec4 lvWallU = (abCcw != 0) ? Cross(lrFnU, arDir)    // var_110 raw
                                    : Cross(arDir, lrFnU);
        {
            const f32 lfLenSq = Dot3(lvWallU, lvWallU);      // shared join
            if (lfLenSq > KF_WALL_NORMALISE_EPSILON)
            {
                lvWallU = Scale(lvWallU, 1.0f / std::sqrt(lfLenSq));
            }
        }

        // Wall ring, parallel to the direction ring (var_140/130/120/110).
        Vec4 laEdgePn[4];
        laEdgePn[0] = Negate(lvWallV);   // var_140
        laEdgePn[1] = Negate(lvWallU);   // var_130
        laEdgePn[2] = lvWallV;           // var_120
        laEdgePn[3] = lvWallU;           // var_110

        // Full edge lengths: 2*halfdim (flt_82001D9C = 2.0f splats, staged
        // as the v124/v125 lane pair the loops lvlx from).
        const f32 lfLenV = 2.0f * lfDimV;   // v124 -- even ring slots (+/-fnV)
        const f32 lfLenU = 2.0f * lfDimU;   // v125 -- odd ring slots (+/-fnU)

        // fcmpu f31 vs 0.0 -> r11 = (sign < 0) ? 1 : 0, then cmplw r11, r4
        // (0x82BA8CFC..0x82BA8DB4): equal -> forward ring, else reversed.
        const s32 liSignBit = (lfSign < 0.0f) ? 1 : 0;   // blt / li r11, 1
        if (liSignBit == abCcw)
        {
            // Forward ring: output slot k takes ring position k
            // (r30 walks dword_82F917B4 forward; memcpy 0x40 per slot to
            // out+0x10 + 0x40*k).
            for (s32 lk = 0; lk < 4; ++lk)
            {
                FeatureEdge& lrEdge = arFeature.edges[lk];
                const s32 liU = KAI_CORNER_INDICES[lk][0];
                const s32 liV = KAI_CORNER_INDICES[lk][1];

                // vaddfp128 v127 + offsetU, then vaddfp + offsetV.
                lrEdge.base = Add(Add(lvFaceCentre, laOffsetU[liU]),
                                  laOffsetV[liV]);                    // +0x00
                lrEdge.dir  = laEdgeDir[lk];                          // +0x10
                lrEdge.pn   = laEdgePn[lk];                           // +0x20
                // lvlx (4k)&4 off the v124/v125 pair: even -> 2*dimV.
                lrEdge.length = Splat((lk & 1) ? lfLenU : lfLenV);    // +0x30
            }
        }
        else
        {
            // Reversed ring (r30 walks -2..1, writing out+0xD0 down to
            // out+0x10): slot 3-lk takes corner (lk+1)&3 with ring
            // direction/wall position (lk+2)&3 -- the asm's clrlslwi masks
            // ((8*(v72-1))&0x18, (16*v72)&0x30, (4*v72)&4 with v72 = lk-2).
            for (s32 lk = 0; lk < 4; ++lk)
            {
                FeatureEdge& lrEdge = arFeature.edges[3 - lk];
                const s32 liPair = (lk + 1) & 3;
                const s32 liRing = (lk + 2) & 3;
                const s32 liU = KAI_CORNER_INDICES[liPair][0];
                const s32 liV = KAI_CORNER_INDICES[liPair][1];

                lrEdge.base = Add(Add(lvFaceCentre, laOffsetU[liU]),
                                  laOffsetV[liV]);
                lrEdge.dir  = laEdgeDir[liRing];
                lrEdge.pn   = laEdgePn[liRing];
                lrEdge.length = Splat((lk & 1) ? lfLenU : lfLenV);
            }
        }

        // stw 4 -> +0x230, then fneg f31 / vmulfp128 / stvx128 -> +0x210:
        // the face plane normal is axis * (-sign) (the winding-fixed sign).
        arFeature.numedges  = 4;
        arFeature.ownNormal = Scale(lrAxis, -lfSign);
    }
    else if (liPerpCount == 1)
    {
        // ===================================================================
        // EDGE feature parallel to the perpendicular axis.
        // ===================================================================
        const s32 liAxisU = KAI_OTHER_AXES[liPerpAxis][0];   // v93
        const s32 liAxisV = KAI_OTHER_AXES[liPerpAxis][1];   // v96

        const Vec4& lrFnU  = lpThis->mFaceNormals[liAxisU];   // v10
        const Vec4& lrFnV  = lpThis->mFaceNormals[liAxisV];   // v11
        const f32   lfDimU = Lane(lpThis->mDimensions, liAxisU);
        const f32   lfDimV = Lane(lpThis->mDimensions, liAxisV);

        s32  liBest      = -1;                      // r5
        f32  lfBestScore = 0.0f;                    // f11 (flt_82001CC0 seed)
        Vec4 lvBestPoint = Splat(0.0f);             // v4 (vspltisw v4, 0)

        // Scan the 4 edge midpoints (v95 walks flt_82F91794 pair by pair).
        for (s32 lk = 0; lk < 4; ++lk)
        {
            const f32 lfTu = KAF_CORNER_PARAMS[lk][0];   // v129 = v95[0]
            const f32 lfTv = KAF_CORNER_PARAMS[lk][1];   // v124 = v95[1]

            // vmaddfp v9 = fnU*(dims[u]*tU) + mPos;
            // vmaddfp v0 = fnV*(dims[v]*tV) + v9.
            const Vec4 lvPoint =
                MaddScalar(lrFnV, lfDimV * lfTv,
                           MaddScalar(lrFnU, lfDimU * lfTu, lpThis->mPos));

            // vmsum3fp128 v9, v0, v1 -> the projection along dir.
            const f32 lfScore = Dot3(lvPoint, arDir);

            // blt (first iteration forced) / fcmpu strict >.
            if (liBest < 0 || lfScore > lfBestScore)
            {
                lvBestPoint = lvPoint;   // vmr v4, v0
                lfBestScore = lfScore;   // fmr f11, f0
                liBest      = lk;        // mr r5, r29
            }
        }

        // End points along the edge axis: point +/- axis*dims[axis]
        // (vmaddfp v13 = axis*dimsplat + point -> var_180;
        //  vsubfp v0 = point - axis*dimsplat -> var_170).
        const Vec4& lrEdgeAxis = lpThis->mFaceNormals[liPerpAxis];
        const f32   lfHalf     = Lane(lpThis->mDimensions, liPerpAxis);
        const Vec4  lvPointPlus  = MaddScalar(lrEdgeAxis, lfHalf, lvBestPoint);
        const Vec4  lvPointMinus = Sub(lvBestPoint, Scale(lrEdgeAxis, lfHalf));

        // bl FeatureEdge::FeatureEdge (r4 = &var_170 = P1, r5 = &var_180 =
        // P2), then memcpy(out+0x10, ctor result, 0x40).
        const FeatureEdge lEdge(lvPointMinus, lvPointPlus);
        arFeature.edges[0] = lEdge;

        arFeature.numedges = 1;   // stw 1 -> +0x230
    }
    else
    {
        // ===================================================================
        // VERTEX feature (zero or three perpendicular axes).
        // ===================================================================
        s32  liBest      = -1;            // r5
        f32  lfBestScore = 0.0f;          // f11 (flt_82001CC0 seed)
        Vec4 lvBestPoint = Splat(0.0f);   // v4 (vspltisw v4, 0)

        // Scan all 8 corners: k <= 3 -> x sign -1.0f (flt_820037C8), else
        // +1.0f (flt_82001C98); (y,z) parameters from the table at k & 3
        // (the clrlslwi (k&3)*8 lfsx picks off flt_82F91794/flt_82F91798).
        for (s32 lk = 0; lk < 8; ++lk)
        {
            const s32 liPair = lk & 3;
            const f32 lfTy = KAF_CORNER_PARAMS[liPair][0];   // v124 <- 82F91794
            const f32 lfTz = KAF_CORNER_PARAMS[liPair][1];   // v129 <- 82F91798
            const f32 lfTx = (lk <= 3) ? -1.0f : 1.0f;       // fcmpu/ble

            // vmaddfp v0 = fn0*(dims.x*tX) + mPos, then the two vaddfp folds
            // += fn1*(dims.y*tY), += fn2*(dims.z*tZ) in this order.
            Vec4 lvPoint = MaddScalar(lpThis->mFaceNormals[0],
                                      lpThis->mDimensions.x * lfTx, lpThis->mPos);
            lvPoint = Add(lvPoint, Scale(lpThis->mFaceNormals[1],
                                         lpThis->mDimensions.y * lfTy));
            lvPoint = Add(lvPoint, Scale(lpThis->mFaceNormals[2],
                                         lpThis->mDimensions.z * lfTz));

            // vmsum3fp128 v13, v0, v1 -> the projection along dir.
            const f32 lfScore = Dot3(lvPoint, arDir);

            // blt (first iteration forced) / fcmpu strict >.
            if (liBest < 0 || lfScore > lfBestScore)
            {
                lvBestPoint = lvPoint;
                lfBestScore = lfScore;
                liBest      = lk;
            }
        }

        arFeature.numedges = 0;            // stw 0 -> +0x230
        arFeature.pt       = lvBestPoint;  // stvx128 v4, r24, 0x220
    }

    // stw r31 -> +0x000 on every path.
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
void GPBox::GetInterval(const GPInstance* lpThis,
                        const Vec4& arDir, Interval& arInterval)
{
    // lvx128 [this+0x00] + vmsum3fp128 v13,v13,v1: the centre projection.
    const f32 lfCentre = Dot3(lpThis->mPos, arDir);

    // stvx128 v13 -> [r4+0x10] then [r4]: both rows are seeded with the raw
    // centre broadcast BEFORE the radius is folded in (caller-visible stores,
    // dead but preserved -- both rows are overwritten below).
    arInterval.max = Splat(lfCentre);   // stvx128 v13, r0, r10 (r4+0x10)
    arInterval.min = Splat(lfCentre);   // stvx128 v13, r0, r4

    // vmsum3fp128 of each face-normal row against the direction.
    const f32 lfDot0 = Dot3(lpThis->mFaceNormals[0], arDir);   // lvx128 [this+0x10]
    const f32 lfDot1 = Dot3(lpThis->mFaceNormals[1], arDir);   // lvx128 [this+0x20]
    const f32 lfDot2 = Dot3(lpThis->mFaceNormals[2], arDir);   // lvx128 [this+0x30]

    // vperm lane splats of mDimensions (+0x70) times the dot broadcasts,
    // vandc 0x80000000 sign strips, then the two vaddfp folds in exactly
    // this order: (|x-term| + |y-term|) + |z-term|.
    const f32 lfRadius = (std::fabs(lpThis->mDimensions.x * lfDot0)      // v10/v0
                        + std::fabs(lpThis->mDimensions.y * lfDot1))     // v11
                        + std::fabs(lpThis->mDimensions.z * lfDot2);     // v12

    // vsubfp v0, v13, v0 -> [r4]; vaddfp v0, v13, v12 -> [r4+0x10].
    arInterval.min = Splat(lfCentre - lfRadius);
    arInterval.max = Splat(lfCentre + lfRadius);
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
void GPBox::GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                         u32 auNumDirs, Interval* lapIntervals)
{
    // Hoisted projection rows: vspltw [this+0x70] lane 0/1/2 * the
    // face-normal rows (vmulfp128 v10/v9/v8, before the count test).
    const Vec4 lvProj0 = Scale(lpThis->mFaceNormals[0], lpThis->mDimensions.x);   // v10
    const Vec4 lvProj1 = Scale(lpThis->mFaceNormals[1], lpThis->mDimensions.y);   // v9
    const Vec4 lvProj2 = Scale(lpThis->mFaceNormals[2], lpThis->mDimensions.z);   // v8

    // cmplwi cr6, r10, 0 / beqlr cr6.
    if (auNumDirs == 0)
    {
        return;
    }

    // addic. r10, r10, -1 / bne loop (dirs advance 0x10, intervals 0x30).
    u32 luRemaining = auNumDirs;
    const Vec4* lpDir = lapDirs;
    Interval*   lpOut = lapIntervals;
    do
    {
        // vmsum3fp128 folds: the three projection-row dots and the centre dot.
        const f32 lfDot1   = Dot3(lvProj1, *lpDir);            // v7
        const f32 lfDot0   = Dot3(lvProj0, *lpDir);            // v6
        const f32 lfDot2   = Dot3(lvProj2, *lpDir);            // v4
        const f32 lfCentre = Dot3(*lpDir, lpThis->mPos);       // v0 (lvx128 [r3])

        // vandc 0x80000000 fabs strips, then the two vaddfp folds in exactly
        // this order: (|proj0 dot| + |proj1 dot|) + |proj2 dot|.
        const f32 lfRadius = (std::fabs(lfDot0) + std::fabs(lfDot1))
                           + std::fabs(lfDot2);

        // vsubfp -> [out+0x00], vaddfp -> [out+0x10] (stvx128 pair).
        lpOut->min = Splat(lfCentre - lfRadius);
        lpOut->max = Splat(lfCentre + lfRadius);

        ++lpDir;    // addi r9, r9, 0x10
        ++lpOut;    // addi r11, r11, 0x30 (Interval stride 0x30)
    }
    while (--luRemaining != 0);
}

} // namespace collision
} // namespace rw
