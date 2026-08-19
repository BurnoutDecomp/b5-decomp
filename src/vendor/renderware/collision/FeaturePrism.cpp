#include "vendor/renderware/collision/GPInstance.hpp"

#include <cmath>   // sqrt, fabs

// ===========================================================================
// rw::collision feature-pair prism intersection family -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass; the hand-vectorised bodies are
// lowered to portable per-lane scalar maths per the committed Feature /
// FeatureEdge precedent, preserving branch polarity, early-outs, store order
// and every caller-visible store).
//
//   rw::collision::FindIntervalOverlap           @ 0x82BB7828
//   rw::collision::FindEdgeEdgePrism             @ 0x82BB78A8
//   rw::collision::FindEdgePointPrism            @ 0x82BB77C0
//   rw::collision::FindFacePointPrism            @ 0x82BB7F18
//   rw::collision::ConstrainPointToFacePrism     @ 0x82BB8050  (TU-local)
//   rw::collision::FindFaceEdgePrismInterval     @ 0x82BB81D0  (TU-local)
//   rw::collision::FindFaceEdgePrism             @ 0x82BB83A8
//   rw::collision::FindFaceFacePrism             @ 0x82BB8798
//   rw::collision::FindFaceFacePrism4x3          @ 0x82BB8CA0
//   rw::collision::FindFaceFacePrism4x4          @ 0x82BB9400
//   rw::collision::FindFeatureIntersectionPrism  @ 0x82BB9BD8
//
// Canonical declarations: Feb-2007 rwccore.h:1810-1836 (the whole family in
// one header). The two workers marked TU-local are unnamed statics of this
// TU in the X360 image (sub_82BB8050 / sub_82BB81D0): rwccore.h does not
// declare them, so their names here are DESCRIPTIVE and their signatures are
// attested from the call sites plus the bodies' own register use. They are
// left at namespace scope with external linkage (the shape the committed
// forward declarations already had, and what the link triage counted).
//
// vmaddfp operand rule used throughout (validated at three sites: the
// Newton-Raphson idioms of RimToEdge/FindFaceEdgePrism and the committed
// Matrix44 Mult chain): IDA lists vmaddfp vD,vA,vB,vC with semantics
// vD = vA*vC + vB (the SECOND displayed operand is the addend); vnmsubfp
// vD,vA,vB,vC computes vB - vA*vC.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; also the discrete
    // vmulfp128 + 2x vmaddfp splat chains, which sum in the same
    // y-term, +x-term, +z-term association order).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // Full 4-lane subtract (vsubfp).
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    // Full 4-lane row * splat + row (vmaddfp with a vmsum3fp128/vspltw splat;
    // all four lanes, w included, exactly as stored).
    inline Vec4 MulAdd(const Vec4& lRow, f32 lfScale, const Vec4& lAdd)
    {
        Vec4 r;
        r.x = lRow.x * lfScale + lAdd.x;
        r.y = lRow.y * lfScale + lAdd.y;
        r.z = lRow.z * lfScale + lAdd.z;
        r.w = lRow.w * lfScale + lAdd.w;
        return r;
    }

    // Full 4-lane row * splat (vmulfp128 against a broadcast register).
    inline Vec4 Scale(const Vec4& lRow, f32 lfScale)
    {
        Vec4 r;
        r.x = lRow.x * lfScale;
        r.y = lRow.y * lfScale;
        r.z = lRow.z * lfScale;
        r.w = lRow.w * lfScale;
        return r;
    }

    // lvlx (from a stack float) + vspltw 0: one scalar broadcast into all four
    // lanes -- the shape every rw::collision Interval bound is stored in.
    inline Vec4 Splat(f32 lfValue)
    {
        Vec4 r;
        r.x = lfValue;
        r.y = lfValue;
        r.z = lfValue;
        r.w = lfValue;
        return r;
    }

    // vmsum3fp128 + vrsqrtefp + 2x Newton-Raphson refine + vmulfp128, with NO
    // zero guard -- used only where the caller has already established that
    // the row is non-degenerate.
    inline Vec4 Normalise(const Vec4& lRow)
    {
        return Scale(lRow, 1.0f / std::sqrt(Dot3(lRow, lRow)));
    }

    // vsubfp + vmsum3fp128 + vmaddfp: the orthogonal projection of lPoint onto
    // the edge's INFINITE line (no segment clamp -- that is
    // FeatureEdge::constrain_point's job).
    inline Vec4 ProjectOntoEdgeLine(const FeatureEdge& arEdge, const Vec4& lPoint)
    {
        return MulAdd(arEdge.dir, Dot3(Sub(lPoint, arEdge.base), arEdge.dir), arEdge.base);
    }

    // Cross product via the VMX two-permute idiom (vpermwi128 0x63 +
    // vmulfp128 + vnmsubfp + rotate back); the w lane is literally
    // a.w*b.w - a.w*b.w (0 for finite inputs; kept verbatim, not folded).
    inline Vec4 CrossRow(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = a.w * b.w - a.w * b.w;
        return r;
    }

    // Project lPoint onto the plane through lOnPlane with unit normal
    // lNormal: vsubfp + vmsum3fp128 + vmaddfp -> point + normal *
    // dot3(normal, onPlane - point), all four lanes.
    inline Vec4 ProjectOntoFacePlane(const Vec4& lNormal,
                                     const Vec4& lOnPlane,
                                     const Vec4& lPoint)
    {
        const f32 lfDist = Dot3(lNormal, Sub(lOnPlane, lPoint));
        return MulAdd(lNormal, lfDist, lPoint);
    }

    // vcmpgtfp. + mfocrf rD,2 + extrwi rD,rD,1,24: CR6 bit 0, the "a > b in
    // ALL four lanes" record bit. Strict compare; a NaN lane compares false
    // and therefore clears the all-lanes bit, exactly as vcmpgtfp does.
    inline bool AllLanesGreater(const Vec4& a, const Vec4& b)
    {
        return a.x > b.x
            && a.y > b.y
            && a.z > b.z
            && a.w > b.w;
    }

    // vcmpgtfp. + mfocrf/extrwi on CR6: true only when splat(lfLhs) > lRhs
    // holds in ALL four lanes. (length is lane-broadcast by the FeatureEdge
    // ctor, so this equals the lane-x test; kept per-lane for fidelity. NaN
    // lanes compare false, defeating the all-true bit, exactly like the
    // scalar > here.) Shared by FindEdgeEdgePrism and FindFaceFacePrism.
    inline bool AllLanesGreaterScalar(f32 lfLhs, const Vec4& lRhs)
    {
        return lfLhs > lRhs.x
            && lfLhs > lRhs.y
            && lfLhs > lRhs.z
            && lfLhs > lRhs.w;
    }

    // -----------------------------------------------------------------------
    // flt_82180968 -- the family's shared degenerate/parallel cutoff, read by
    // FindEdgeEdgePrism (|dir1 x dir2|^2), ConstrainPointToFacePrism
    // (|sepDir x edgeDir|^2) and FindFaceFacePrism (|dot3(dir2, pn1)|). The
    // export carries the value: 0x34000000 == 1.1920929e-7f (FLT_EPSILON).
    // -----------------------------------------------------------------------
    const f32 KF_PARALLEL_EPSILON = 1.1920929e-7f;
}

// ===========================================================================
// rw::collision::FindIntervalOverlap @ 0x82BB7828
// Called by (1): rw::collision::FindEdgeEdgePrism (pending).
//
// The body is three lvx128/vcmpgtfp./mfocrf(CR6) blocks. Every vector select
// is a WHOLE-REGISTER select driven by the CR6[0] "all four lanes true"
// record bit (not a per-lane vsel), so it lowers exactly to a branch on an
// all-lanes strict greater-than. The interval rows are the canonical
// rw::collision::Interval min/max broadcasts. (Three leaf-frame null
// back-chain stack writes around the mfocrf blocks have no observable effect
// and are not modelled.)
//
// result.min = the higher of the two lower-bound rows: interval1's row is
// kept only when interval1.min > interval2.min in ALL lanes (bne on CR6[0]);
// any tie or mixed lane falls through to interval2's row (mr r10, r5).
// result.max = the lower of the two upper-bound rows: note the REVERSED
// compare operands -- interval1's row is kept only when interval2.max >
// interval1.max in all lanes, so a tie selects interval2's row.
// Returns 1 when result.max > result.min in all four lanes (the third
// vcmpgtfp., issued against the chosen max register and the min row re-loaded
// from *lpResult), else 0.
// ===========================================================================
u32 FindIntervalOverlap(Interval* lpResult,
                        const Interval* lpInterval1, const Interval* lpInterval2)
{
    // lvx128 v0,r5 / lvx128 v13,r4 / vcmpgtfp. v13,v0 -> pick r10 = r4 or r5;
    // lvx128 v0,r10 / stvx128 v0,r3. The min row is STORED before the max
    // rows are loaded, exactly as below (matters if lpResult aliases an input).
    const Vec4 lvMin = AllLanesGreater(lpInterval1->min, lpInterval2->min)
                           ? lpInterval1->min
                           : lpInterval2->min;
    lpResult->min = lvMin;

    // lvx128 v0,r4+0x10 / lvx128 v13,r5+0x10 / vcmpgtfp. v13,v0 (i.e. is
    // interval2.max > interval1.max) -> keep r11 = r4+0x10 on all-true, else
    // r11 = r5+0x10; lvx128 v0,r11 loads the chosen max row into a register.
    const Vec4 lvMax = AllLanesGreater(lpInterval2->max, lpInterval1->max)
                           ? lpInterval1->max
                           : lpInterval2->max;

    // lvx128 v13,r3 (re-load the just-stored min row) / vcmpgtfp. v13,v0,v13;
    // the compare is issued before the max store, both on the same register.
    const u32 luOverlaps = AllLanesGreater(lvMax, lpResult->min) ? 1u : 0u;

    // stvx128 v0, r3, 0x10
    lpResult->max = lvMax;

    // extrwi r3, r11, 1,24 -- CR6[0] zero-extended into the return register.
    return luOverlaps;
}

// ===========================================================================
// rw::collision::FindEdgeEdgePrism @ 0x82BB78A8  (canonical rwccore.h:1816)
//
// Edge/edge contact generation, split on whether the two edge directions are
// (near) parallel -- |dir1 x dir2|^2 against flt_82180968 (FLT_EPSILON):
//
//   (1) SKEW edges -> ONE contact at the two lines' closest approach. The
//       parameter along edge 1 is clamped into [0, length1]; the resulting
//       point is written to the EDGE-2 output row and constrained onto edge 2,
//       and the CONSTRAINED point (not the raw one) is then copied to the
//       edge-1 row and constrained onto edge 1. Both features' region words
//       are re-biased as `region - code + 2`, the FindEdgePointPrism idiom.
//
//   (2) PARALLEL edges -> both segments are projected onto whichever of the
//       two directions is LESS aligned with sepDir, and the resulting 1-D
//       intervals go through FindIntervalOverlap @ 0x82BB7828:
//         * overlapping -> TWO contacts. The clipped bounds are ABSOLUTE dot3
//           projections (measured through the world origin), so `axis * bound`
//           is the point on the axis line at that parameter; each bound point
//           is ray-cast along sepDir onto each edge's own plane and then
//           dropped orthogonally onto that edge's line.
//         * disjoint -> ONE contact at the two FACING endpoints; each
//           feature's region word is stepped +1 when its far endpoint was
//           chosen and -1 when its base was.
// Always returns TRUE (li r3, 1 on every exit path).
//
// Register contract: r3=&res r4=ptsOn1 r5=ptsOn2 r6=&ef1 r7=&ef2 r8=&sepDir;
// ptsOn1 stays with ef1 and ptsOn2 with ef2 on every path.
//
// NOTE (why the register reads below are sound): the compiler kept the axis
// row in v6 and the two feature pointers in r6/r7 ACROSS the
// FindIntervalOverlap call. Those are ABI-volatile, so this is whole-program
// register knowledge -- FindIntervalOverlap @ 0x82BB7828 genuinely touches
// only r3/r4/r5/r8..r11 and v0/v13, which the dump confirms.
// ===========================================================================
RwBool FindEdgeEdgePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                         Feature& arEdgeFeature1, Feature& arEdgeFeature2, const Vec4& arSepDir)
{
    // r25 = ef1+0x20 / r26 = ef2+0x20 (the direction rows), r27 / r7 = +0x10
    // and r6 = +0x10 (the base rows): an edge feature carries exactly one
    // FeatureEdge, and only edges[0] is ever touched.
    const FeatureEdge& lrEdge1 = arEdgeFeature1.edges[0];
    const FeatureEdge& lrEdge2 = arEdgeFeature2.edges[0];

    // vpermwi128 0x63 x2 + vmulfp128 + vnmsubfp + vmsum3fp128.
    const Vec4 lvCross   = CrossRow(lrEdge1.dir, lrEdge2.dir);
    const f32  lfCrossSq = Dot3(lvCross, lvCross);

    // fcmpu cr6 / ble -> the parallel arm, so an unordered compare takes the
    // SKEW arm (the ble is not taken on NaN).
    if (!(lfCrossSq <= KF_PARALLEL_EPSILON))
    {
        // ---- (1) skew edges: the closest approach of the two lines ----
        // n = normalize(dir1 x dir2); n x dir2 spans the plane that contains
        // edge 2 and is perpendicular to n, so the parameter along edge 1 is
        // a plain ray/plane solve against it.
        const Vec4 lvNormal = Normalise(lvCross);
        const Vec4 lvPerp   = CrossRow(lvNormal, lrEdge2.dir);

        // fdivs: both dots are vmsum3fp128 folds bounced through stack floats
        // so the divide runs scalar on lane 0.
        f32 lfT = Dot3(Sub(lrEdge2.base, lrEdge1.base), lvPerp)
                / Dot3(lrEdge1.dir, lvPerp);

        // fcmpu/bge against flt_82001CC0 (0.0f) -- an unordered compare
        // clamps, exactly as the not-taken bge does.
        if (!(lfT >= 0.0f))
        {
            lfT = 0.0f;
        }

        // vcmpgtfp. splat(t) > the broadcast length row, CR6 all-lanes bit.
        if (AllLanesGreaterScalar(lfT, lrEdge1.length))
        {
            lfT = lrEdge1.length.x;                        // lfs 0x40(ef1)
        }

        arRes.m_numpts = 1;                                // stw r10, 0x210(r22)

        // vmaddfp dir1 * splat(t) + base1 -> stvx128 to the EDGE-2 row.
        lapPtsOn2[0] = MulAdd(lrEdge1.dir, lfT, lrEdge1.base);

        // bl constrain_point (this = ef2+0x10) then lwz/subf/addi 2/stw.
        const u32 luRegion2 = lrEdge2.constrain_point(lapPtsOn2[0]);
        arEdgeFeature2.region = arEdgeFeature2.region - luRegion2 + 2;

        // lvx128 from the edge-2 row / stvx128 to the edge-1 row: the copy is
        // taken AFTER the first constrain, so both rows start from the same
        // clamped point.
        lapPtsOn1[0] = lapPtsOn2[0];
        const u32 luRegion1 = lrEdge1.constrain_point(lapPtsOn1[0]);
        arEdgeFeature1.region = arEdgeFeature1.region - luRegion1 + 2;

        return 1;
    }

    // ---- (2) parallel edges: clip the two 1-D projections ----
    // fabs/fabs + fcmpu/blt: keep edge 1's direction only when it is LESS
    // aligned with the sweep direction; a tie (or an unordered compare) takes
    // edge 2's row, because the vmr sits on the not-taken side of the blt.
    const Vec4& lrAxis =
        (std::fabs(Dot3(arSepDir, lrEdge1.dir)) < std::fabs(Dot3(arSepDir, lrEdge2.dir)))
            ? lrEdge1.dir
            : lrEdge2.dir;

    // The four projections, in the asm's order: each edge's base, then its far
    // endpoint (vmaddfp dir * length + base), each folded with vmsum3fp128.
    const f32 lfA0 = Dot3(lrAxis, lrEdge1.base);
    const f32 lfB0 = Dot3(lrAxis, lrEdge2.base);
    const f32 lfA1 = Dot3(lrAxis, MulAdd(lrEdge1.dir, lrEdge1.length.x, lrEdge1.base));
    const f32 lfB1 = Dot3(lrAxis, MulAdd(lrEdge2.dir, lrEdge2.length.x, lrEdge2.base));

    // vcmpgefp (first >= second) + vnot + vsel pairs on the broadcast rows:
    // whole-register min/max selects, so an exact tie takes the second row on
    // the min side and the first on the max side. A NaN defeats the vcmpgefp
    // mask exactly as it defeats the C >= here.
    Interval lInterval1;
    Interval lInterval2;
    Interval lOverlap;

    lInterval1.min = Splat((lfA0 >= lfA1) ? lfA1 : lfA0);   // sp+var_F0
    lInterval1.max = Splat((lfA0 >= lfA1) ? lfA0 : lfA1);   // sp+var_E0
    lInterval2.min = Splat((lfB0 >= lfB1) ? lfB1 : lfB0);   // sp+var_C0
    lInterval2.max = Splat((lfB0 >= lfB1) ? lfB0 : lfB1);   // sp+var_B0

    if (FindIntervalOverlap(&lOverlap, &lInterval1, &lInterval2))
    {
        // ---- the projections overlap: two contacts at the clipped ends ----
        // n1 / n2 = normalize((sepDir x dir) x dir) -- the sweep direction's
        // component perpendicular to each edge (negated; the sign cancels in
        // the ray/plane quotient below).
        const Vec4 lvNormal1 = Normalise(CrossRow(CrossRow(arSepDir, lrEdge1.dir), lrEdge1.dir));
        const Vec4 lvNormal2 = Normalise(CrossRow(CrossRow(arSepDir, lrEdge2.dir), lrEdge2.dir));

        // vmulfp128 of the axis row by the broadcast interval bound.
        const Vec4 lvAnchor0 = Scale(lrAxis, lOverlap.min.x);
        const Vec4 lvAnchor1 = Scale(lrAxis, lOverlap.max.x);

        // Ray-cast each anchor along sepDir onto each edge's plane. The asm
        // builds 1/denominator with vrefp + two Newton-Raphson refines;
        // rendered as the exact division.
        lapPtsOn1[0] = MulAdd(arSepDir,
                              Dot3(Sub(lrEdge1.base, lvAnchor0), lvNormal1)
                                  / Dot3(arSepDir, lvNormal1),
                              lvAnchor0);                       // stvx128 -> [r30]
        lapPtsOn1[1] = MulAdd(arSepDir,
                              Dot3(Sub(lrEdge1.base, lvAnchor1), lvNormal1)
                                  / Dot3(arSepDir, lvNormal1),
                              lvAnchor1);                       // stvx128 -> [r30+0x10]
        lapPtsOn2[0] = MulAdd(arSepDir,
                              Dot3(Sub(lrEdge2.base, lvAnchor0), lvNormal2)
                                  / Dot3(arSepDir, lvNormal2),
                              lvAnchor0);                       // stvx128 -> [r28]
        lapPtsOn2[1] = MulAdd(arSepDir,
                              Dot3(Sub(lrEdge2.base, lvAnchor1), lvNormal2)
                                  / Dot3(arSepDir, lvNormal2),
                              lvAnchor1);                       // stvx128 -> [r28+0x10]

        // Then drop each contact orthogonally onto its own edge's line, in the
        // asm's re-store order (each row is re-loaded from the output array).
        lapPtsOn1[0] = ProjectOntoEdgeLine(lrEdge1, lapPtsOn1[0]);
        lapPtsOn1[1] = ProjectOntoEdgeLine(lrEdge1, lapPtsOn1[1]);
        lapPtsOn2[0] = ProjectOntoEdgeLine(lrEdge2, lapPtsOn2[0]);
        lapPtsOn2[1] = ProjectOntoEdgeLine(lrEdge2, lapPtsOn2[1]);

        arRes.m_numpts = 2;                                     // stw r11, 0x210(r22)
        return 1;
    }

    // ---- the projections are disjoint: one contact at the facing ends ----
    arRes.m_numpts = 1;                                         // stw r11, 0x210(r22)

    // vcmpgtfp. interval2.min > interval1.min in ALL four lanes: edge 2 lies
    // FURTHER along the axis, so edge 1 contributes its HIGH endpoint and
    // edge 2 its LOW one. A tie (or a NaN) clears the bit and swaps the roles.
    // Each endpoint choice is a bare fcmpu on the two raw projections, and the
    // branch polarities below are the asm's (an unordered compare always falls
    // through to the base-endpoint arm on the ble/bge sides).
    if (AllLanesGreater(lInterval2.min, lInterval1.min))
    {
        if (lfA0 <= lfA1)                                       // ble cr6
        {
            lapPtsOn1[0] = MulAdd(lrEdge1.dir, lrEdge1.length.x, lrEdge1.base);
            arEdgeFeature1.region = arEdgeFeature1.region + 1;
        }
        else
        {
            lapPtsOn1[0] = lrEdge1.base;
            arEdgeFeature1.region = arEdgeFeature1.region - 1;
        }

        if (lfB0 >= lfB1)                                       // bge cr6
        {
            lapPtsOn2[0] = MulAdd(lrEdge2.dir, lrEdge2.length.x, lrEdge2.base);
            arEdgeFeature2.region = arEdgeFeature2.region + 1;
        }
        else
        {
            lapPtsOn2[0] = lrEdge2.base;
            arEdgeFeature2.region = arEdgeFeature2.region - 1;
        }
    }
    else
    {
        if (lfA0 >= lfA1)                                       // bge cr6
        {
            lapPtsOn1[0] = MulAdd(lrEdge1.dir, lrEdge1.length.x, lrEdge1.base);
            arEdgeFeature1.region = arEdgeFeature1.region + 1;
        }
        else
        {
            lapPtsOn1[0] = lrEdge1.base;
            arEdgeFeature1.region = arEdgeFeature1.region - 1;
        }

        if (lfB0 > lfB1)                                        // bgt cr6
        {
            lapPtsOn2[0] = lrEdge2.base;
            arEdgeFeature2.region = arEdgeFeature2.region - 1;
        }
        else
        {
            lapPtsOn2[0] = MulAdd(lrEdge2.dir, lrEdge2.length.x, lrEdge2.base);
            arEdgeFeature2.region = arEdgeFeature2.region + 1;
        }
    }

    return 1;
}

// ===========================================================================
// rw::collision::FindEdgePointPrism @ 0x82BB77C0
//
// Edge-vs-point case. Emits exactly one contact point: the point feature's
// position, written to both out rows, with the copy in ptsOn1[0] then
// constrained onto the edge in place. The edge feature's region/id word is
// refined by the constrain region code (2 = on segment leaves it unchanged,
// 1 = before base -> +1, 3 = past the length -> -1). Always returns 1.
//
// Asm walk (authoritative):
//   stw   r9, 0x210(r3)       r9 = 1        -> result point count = 1
//   lvx128 v0, r0, r7+0x220 ; stvx128 -> r4 -> ptsOn1[0] = pf.pt
//   lvx128 v0, r0, r7+0x220 ; stvx128 -> r5 -> ptsOn2[0] = pf.pt
//   bl    FeatureEdge::constrain_point       this = r6+0x10 (ef.edges[0])
//   lwz r11,0(r6); subf; addi 2; stw 0(r6)  -> ef.region -= region, += 2
//   li r3,1 ; blr
// The 6th (sepDir) argument is forwarded by the dispatcher but never read.
// ===========================================================================
RwBool FindEdgePointPrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                          Feature& arEdgeFeature, Feature& arPointFeature, const Vec4& arSepDir)
{
    (void)arSepDir;

    // stw r9, 0x210(r10) with r9 = 1: this case always emits one point.
    arRes.m_numpts = 1;

    // lvx128 v0, r0, r11 (r11 = pf + 0x220) / stvx128 to r4 and again to r5:
    // the point feature's position row goes to BOTH out rows.
    lapPtsOn1[0] = arPointFeature.pt;
    lapPtsOn2[0] = arPointFeature.pt;

    // bl FeatureEdge::constrain_point (this = ef+0x10, point = r4): constrain
    // the copy in ptsOn1[0] onto the edge, in place, and get the region code
    // (1 = before base, 2 = on segment, 3 = past the length).
    const u32 luRegion = arEdgeFeature.edges[0].constrain_point(lapPtsOn1[0]);

    // lwz r11,0(r31) / subf / addi 2 / stw: refine the edge feature's region
    // word by the region code (region 2 leaves it unchanged).
    arEdgeFeature.region = arEdgeFeature.region - luRegion + 2;

    // li r3, 1: return the number of contact points emitted.
    return 1;
}

// ===========================================================================
// rw::collision::FindFacePointPrism @ 0x82BB7F18
//
//   point  = dir * dot3(dir, faceVertex0 - pointPos) + pointPos
//            (lvx128 x3 / vsubfp / vmsum3fp128 / vmaddfp v0,v0,v13,v12)
//   for each face edge i in [0, numedges):              (0x40-stride loop)
//       dist_i = dot3(pn_i, point - base_i)             (vsubfp/vmsum3fp128)
//       track the LARGEST dist (first iteration always wins: best index
//       seeded to -1, blt cr6 short-circuits the fcmpu)
//   if largest dist > 0 (the point sits outside the winning face plane):
//       point -= pn_best * dist                         (lvlx+vspltw /
//                                                        vmulfp128 / vsubfp)
//       region = edges[best].constrain_point(point)     (bl, in-place clamp)
//       ff.region += region + 2*best                    (lwz/add/stw @ ff+0)
//   ptsOnF[0] = point                                   (stvx128 -> r28)
//   ptsOnP[0] = pf.pt                                   (lvx128 r29/stvx128 r27)
//   return 1
//
// The initial best distance is seeded from .rdata flt_82001CC0 == 0.0f. NaN
// polarity of the max-track matches the asm: fcmpu+ble skips the update when
// the compare is unordered, exactly as (lfDist > lfBest) does.
// ===========================================================================
RwBool FindFacePointPrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOnF, Vec4* lapPtsOnP,
                          Feature& arFaceFeature, Feature& arPointFeature, const Vec4& arSepDir)
{
    // stw r10, 0x210(r3) -- exactly one contact point is produced.
    arRes.m_numpts = 1;

    // lvx128 v0  <- sepDir (r8); lvx128 v13 <- pf.pt (+0x220);
    // lvx128 v12 <- face base vertex (ff+0x10 == edges[0].base).
    const Vec4  lvDir     = arSepDir;
    const Vec4  lvOther   = arPointFeature.pt;
    const Vec4& lrVertex0 = arFaceFeature.edges[0].base;

    // vsubfp v12, v12, v13 : vertex0 - pointPos (all four lanes);
    // vmsum3fp128 v12, v0, v12 : t = dot3(dir, delta).
    const f32 lfT = Dot3(lvDir, Sub(lrVertex0, lvOther));

    // vmaddfp v0, v0, v13, v12 (vD = vA*vC + vB) : point = dir*t + pointPos --
    // the point feature's position advanced along the prism direction onto the
    // face's base-vertex plane. All four lanes, w included.
    Vec4 lvPoint = MulAdd(lvDir, lfT, lvOther);

    // f0 = f12 = flt_82001CC0 (0.0f); r30 = -1. (The asm also spills f0 to a
    // stack float it later lvlx+vspltw's back -- purely local traffic.)
    f32 lfBest = 0.0f;
    s32 liBest = -1;

    // lwz r10, 0x230(r31); cmpwi/ble cr6 -- skip everything when no edges.
    if (arFaceFeature.numedges > 0)
    {
        const s32 liCount = arFaceFeature.numedges;   // lwz r9, 0x230(r31)

        // r10 walks ff+0x30 (the plane-normal row) in 0x40 strides; the vertex
        // row is loaded at r10-0x20.
        for (s32 liEdge = 0; liEdge < liCount; ++liEdge)
        {
            const FeatureEdge& lrEdge = arFaceFeature.edges[liEdge];

            // vsubfp v12, v0, v12 : point - base_i;
            // vmsum3fp128 v13, v13, v12 : signed distance to face plane i.
            const f32 lfDist = Dot3(lrEdge.pn, Sub(lvPoint, lrEdge.base));

            // blt cr6 (liBest < 0 short-circuit) / fcmpu+ble (take on >):
            // unordered compares skip the update, matching NaN behaviour.
            if (liBest < 0 || lfDist > lfBest)
            {
                lfBest = lfDist;
                liBest = liEdge;
            }
        }

        // stfs f0 (spill) ; fcmpu cr6, f0, f12 ; ble -- act only when the
        // point ended up OUTSIDE the most-violated face plane.
        if (lfBest > 0.0f)
        {
            const FeatureEdge& lrBestEdge = arFaceFeature.edges[liBest];

            // lvlx+vspltw broadcast lfBest; lvx128 v12 <- pn of the winning
            // face edge; vmulfp128 + vsubfp : point -= pn * dist (all lanes).
            lvPoint.x -= lrBestEdge.pn.x * lfBest;
            lvPoint.y -= lrBestEdge.pn.y * lfBest;
            lvPoint.z -= lrBestEdge.pn.z * lfBest;
            lvPoint.w -= lrBestEdge.pn.w * lfBest;

            // bl constrain_point with r3 = ff + (best<<6) + 0x10 (the embedded
            // FeatureEdge) and r4 = the point buffer: clamp onto the edge, in
            // place. The asm reloads v0 from the buffer afterwards, so the
            // clamped point IS the output.
            const u32 luRegion = lrBestEdge.constrain_point(lvPoint);

            // lwz/add/stw @ (r31)+0 : fold the region code and edge index into
            // the face feature's region accumulator.
            arFaceFeature.region += luRegion + 2 * static_cast<u32>(liBest);
        }
    }

    // stvx128 v0 -> *r28, then lvx128 (pf+0x220) / stvx128 -> *r27, in that
    // order (the point-side output is re-read from the feature at exit).
    lapPtsOnF[0] = lvPoint;
    lapPtsOnP[0] = arPointFeature.pt;

    return 1;   // li r3, 1
}

namespace
{
    // .rdata literals of the face/edge pair. Every value is attested by the
    // export (the addresses are @ha/@l pairs in the bodies below).
    const f32 KF_POINT_CONTACT_TOLERANCE = 0.001f;          // flt_821809D4
    const f32 KF_MIN_SEPARATION_LENGTH   = 1.1754944e-38f;  // flt_82180934 (FLT_MIN)

    // flt_821809D4 again -- the SAME 0.001f literal, but gating a different
    // quantity (the rebuilt prism-wall normal's length, not an interval
    // width), so it carries its own name rather than being borrowed.
    const f32 KF_DEGENERATE_WALL_LENGTH  = 0.001f;          // flt_821809D4

    // flt_821809D0 == 0x3D4CCCCD == 0.05f. Both operands of the guarded dot
    // are unit rows, so this is a ~2.9-degree parallelism cone, NOT an
    // epsilon -- do not "tidy" it toward FLT_EPSILON.
    const f32 KF_PRISM_PARALLEL_DENOM    = 0.05f;           // flt_821809D0
}

// ===========================================================================
// rw::collision::ConstrainPointToFacePrism @ 0x82BB8050
// (X360 sub_82BB8050 -- an unnamed static of this TU. FLAGGED: the name is
// descriptive; the signature is attested by the three FindFaceEdgePrism call
// sites plus this body's own register use -- r3=&ff r4=&sepDir r5=point.)
//
// Clamps *lpPoint into the face feature's prism cross-section, in place, and
// returns the region-code contribution the caller folds into Feature::region.
//
// The walls are NOT the stored FeatureEdge::pn rows -- each is REBUILT as
// cross(sepDir, edge.dir) and normalised, which orients it INWARD, the
// opposite sign convention to the outward pn FindFacePointPrism uses. So this
// loop tracks the MOST NEGATIVE signed distance (the most violated wall) and
// acts only when that distance is negative: the exact mirror of
// FindFacePointPrism's "track the largest, act when positive".
//
// A degenerate wall (|n|^2 <= flt_82180968 == FLT_EPSILON, i.e. the face edge
// runs along the sweep direction) is left UNNORMALISED rather than skipped --
// the asm's ble simply jumps over the vrsqrtefp block, so the near-zero row
// still yields a (near-zero) distance and can still win the tracking.
// Reproduced verbatim rather than "fixed".
//
// Returns 0 when the point already sits inside every wall: r3 is loaded with
// 0 in the prologue and only overwritten on the constrain path.
// ===========================================================================
u32 ConstrainPointToFacePrism(Feature& arFaceFeature, const Vec4& arSepDir,
                              Vec4* lpPoint)
{
    // li r3, 0 / li r31, -1 / the splat of flt_82001CC0 (0.0f). The seeded
    // 0.0f best is dead: liBest == -1 forces the first edge to take its own
    // distance unconditionally (blt cr6 straight to the update).
    u32  luRegion = 0;      // r3
    s32  liBest   = -1;     // r31
    f32  lfBest   = 0.0f;   // v8 (lane-broadcast; every candidate is a
                            //     vmsum3fp128 broadcast, so the lanes agree)
    Vec4 lvBestNormal;      // sp+var_20 -- only read when liBest >= 0

    const s32 liCount = arFaceFeature.numedges;   // lwz r9, 0x230(r10)
    for (s32 liEdge = 0; liEdge < liCount; ++liEdge)
    {
        const FeatureEdge& lrEdge = arFaceFeature.edges[liEdge];

        // vpermwi128 0x63 x2 + vmulfp128 + vnmsubfp: the inward wall normal.
        Vec4 lvNormal = CrossRow(arSepDir, lrEdge.dir);

        // vmsum3fp128 + fcmpu/ble around the vrsqrtefp + 2x Newton-Raphson
        // block: an unordered compare NORMALISES (the ble is not taken).
        const f32 lfLenSq = Dot3(lvNormal, lvNormal);
        if (!(lfLenSq <= KF_PARALLEL_EPSILON))
        {
            lvNormal = Scale(lvNormal, 1.0f / std::sqrt(lfLenSq));
        }

        // vsubfp + vmsum3fp128: the signed distance from this rebuilt wall.
        const f32 lfDist = Dot3(lvNormal, Sub(*lpPoint, lrEdge.base));

        // blt cr6 (liBest < 0) short-circuits to the update; otherwise
        // vcmpgtfp. best > dist on all four lanes gates it, so the most
        // negative distance wins and a NaN lane loses.
        if (liBest < 0 || lfBest > lfDist)
        {
            lfBest       = lfDist;
            liBest       = liEdge;
            lvBestNormal = lvNormal;   // stvx128 -> sp+var_20
        }
    }

    // vcmpgtfp. splat(0.0f) > best on all four lanes: act only when the point
    // is outside the most violated wall.
    if (0.0f > lfBest)
    {
        const FeatureEdge& lrBestEdge = arFaceFeature.edges[liBest];

        // vmulfp128 + vsubfp: slide the point onto that wall plane, in place
        // (bestDist is negative here, so this moves the point along +n).
        *lpPoint = Sub(*lpPoint, Scale(lvBestNormal, lfBest));

        // bl constrain_point with this = ff + (best << 6) + 0x10, then
        // add r3, r3, best << 1.
        luRegion = lrBestEdge.constrain_point(*lpPoint)
                 + 2u * static_cast<u32>(liBest);
    }

    return luRegion;
}

// ===========================================================================
// rw::collision::FindFaceEdgePrismInterval @ 0x82BB81D0
// (X360 sub_82BB81D0 -- an unnamed static of this TU. FLAGGED name; register
// contract r3=&ff r4=&ef r5=&sepDir r6=t0 r7=t1 r8=crossPoint r9=crossDir.)
//
// Clips the edge feature's segment against the face feature's prism walls.
// The interval is parametrised in ABSOLUTE length units along ef.edges[0]
// (the caller feeds t straight into base + dir*t), seeded to
// [0, ef.edges[0].length] and then narrowed wall by wall.
//
// Each wall normal is rebuilt as cross(sepDir, faceEdge.dir), exactly as
// ConstrainPointToFacePrism does. A wall whose rebuilt normal is SHORTER than
// flt_821809D4 (0.001f) contributes no clip. Otherwise, with the unit normal
// n, denom = dot3(ef.dir, n) and numer = dot3(faceEdge.base - ef.base, n):
//   * |denom| < flt_821809D0 (0.05f): the segment runs along the wall. Only a
//     POSITIVE numerator records the caller's crossing pair
//     (*lpCrossPoint = faceEdge.base, *lpCrossDir = n) and marks the clip
//     unresolved; the walk CONTINUES either way, so a later parallel wall
//     overwrites the pair.
//   * denom > 0: raise *lpfT0 to numer/denom; if that inverts the interval,
//     collapse it (*lpfT0 = *lpfT1), mark unresolved and stop.
//   * denom <= 0: lower *lpfT1 to numer/denom; if that inverts the interval,
//     collapse it (*lpfT1 = *lpfT0), mark unresolved and stop.
//
// Return: cntlzw r11, r31 + extrwi r3, r11, 1,26 extracts bit 5 of the
// leading-zero count, i.e. 1 exactly when the flag register is zero -- TRUE
// means "clipped cleanly by every wall". That is why the crossing pair only
// has to be valid on the FALSE side: FindFaceEdgePrism reads it only there.
// ===========================================================================
RwBool FindFaceEdgePrismInterval(Feature& arFaceFeature, Feature& arEdgeFeature,
                                 const Vec4& arSepDir,
                                 f32* lpfT0, f32* lpfT1,
                                 Vec4* lpCrossPoint, Vec4* lpCrossDir)
{
    const FeatureEdge& lrEdge = arEdgeFeature.edges[0];

    // stfs flt_82001CC0 (0.0f) -> *t0 ; lfs 0x40(r4) -> *t1: the whole edge.
    *lpfT0 = 0.0f;
    *lpfT1 = lrEdge.length.x;

    bool lbUnresolved = false;   // r31

    // lwz r11, 0x230(r3) guards entry and is RE-READ at the bottom of every
    // pass, so the count comes from the feature on each iteration.
    for (s32 liEdge = 0; liEdge < arFaceFeature.numedges; ++liEdge)
    {
        const FeatureEdge& lrFaceEdge = arFaceFeature.edges[liEdge];

        // The rebuilt wall normal and its LENGTH: vrsqrtefp + 2x
        // Newton-Raphson, times lenSq (== sqrt), with a vcmpeqfp/vsel guard
        // that forces 0 where lenSq is exactly 0.
        const Vec4 lvWall  = CrossRow(arSepDir, lrFaceEdge.dir);
        const f32  lfLenSq = Dot3(lvWall, lvWall);
        const f32  lfLen   = (lfLenSq == 0.0f)
                           ? 0.0f
                           : lfLenSq * (1.0f / std::sqrt(lfLenSq));

        if (lfLen < KF_DEGENERATE_WALL_LENGTH)   // fcmpu cr6 / blt -> next wall
        {
            continue;
        }

        // The unit normal: a second vrsqrtefp pair on the same lenSq (no zero
        // guard needed -- the length test above already passed).
        const Vec4 lvNormal = Scale(lvWall, 1.0f / std::sqrt(lfLenSq));

        const f32 lfDenom = Dot3(lrEdge.dir, lvNormal);                        // vmsum3fp128
        const f32 lfNumer = Dot3(Sub(lrFaceEdge.base, lrEdge.base), lvNormal); // vsubfp + vmsum3fp128

        // fabs + fcmpu/bge selects the crossing arm, so an unordered compare
        // falls into the parallel arm below.
        if (!(std::fabs(lfDenom) >= KF_PRISM_PARALLEL_DENOM))
        {
            // fcmpu against flt_82001CC0 (0.0f) with ble -> skip, so an
            // unordered compare RECORDS.
            if (!(lfNumer <= 0.0f))
            {
                *lpCrossDir   = lvNormal;          // stvx128 -> [r9]
                lbUnresolved  = true;              // li r31, 1
                *lpCrossPoint = lrFaceEdge.base;   // stvx128 -> [r8]
            }
            continue;
        }

        // fdivs, issued before the sign test in the asm; kept in that order.
        const f32 lfT = lfNumer / lfDenom;

        // fcmpu cr6, denom, 0.0 / ble -> the exit-wall arm, so an unordered
        // compare takes the entry-wall arm.
        if (!(lfDenom <= 0.0f))
        {
            // Entry wall: raise the interval start (ble -> skip the store).
            if (!(lfT <= *lpfT0))
            {
                *lpfT0 = lfT;
            }
            if (!(*lpfT0 <= *lpfT1))
            {
                *lpfT0       = *lpfT1;   // fmr/stfs: collapse to a point
                lbUnresolved = true;
                break;
            }
        }
        else
        {
            // Exit wall: lower the interval end (bge -> skip the store).
            if (!(lfT >= *lpfT1))
            {
                *lpfT1 = lfT;
            }
            // blt cr6 -- a STRICT compare here, unlike the entry arm's ble.
            if (*lpfT1 < *lpfT0)
            {
                *lpfT1       = *lpfT0;
                lbUnresolved = true;
                break;
            }
        }
    }

    return lbUnresolved ? 0 : 1;
}

// ===========================================================================
// rw::collision::FindFaceEdgePrism @ 0x82BB83A8   (canonical rwccore.h:1822)
//
// Builds the contact-point "prism" between a face feature ff and an edge
// feature ef, separated along the unit direction sepDir. Three paths:
//   (1) the edge segment PIERCES the prism cross-section (helper true):
//       two contacts at the interval ends, each ray-cast along sepDir onto
//       the face plane;
//   (2) the clipped interval is a POINT (width <= 0.001): one contact,
//       orthogonally offset along sepDir, then clamped into the face region
//       and back onto the edge (region codes accumulate into ff/ef);
//   (3) the interval OVERLAPS (parallel-ish case): two contacts swept along
//       the edge from an anchor dropped into the prism cross-section,
//       clamped both ways; when the clamp separated the point pairs, the
//       contact normal is overridden with dir x (sep x dir).
// Always returns TRUE (r3 = 1 on every exit path).
//
// .rdata constants (both VALUES present in the export's literals):
//   flt_821809D4 = 0.001f          (point-contact interval tolerance)
//   flt_82180934 = 1.1754944e-38f  (FLT_MIN, separation-length gate)
//
// Both helpers it calls are defined ABOVE (they are TU-local statics in the
// X360 image, at lower addresses in the same run).
// ===========================================================================

RwBool FindFaceEdgePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOnF, Vec4* lapPtsOnE,
                         Feature& arFaceFeature, Feature& arEdgeFeature, const Vec4& arSepDir)
{
    f32  lfT0 = 0.0f;    // sp+0x50 (var_90) -- interval start along ef.edges[0]
    f32  lfT1 = 0.0f;    // sp+0x58 (var_88) -- interval end
    Vec4 lvCrossDir;     // sp+0x60 (var_80) -- helper out, r9
    Vec4 lvCrossPoint;   // sp+0x70 (var_70) -- helper out, r8

    const FeatureEdge& lrEdge = arEdgeFeature.edges[0];

    if (FindFaceEdgePrismInterval(arFaceFeature, arEdgeFeature, arSepDir,
                                  &lfT0, &lfT1, &lvCrossPoint, &lvCrossDir))
    {
        // ---- (1) the segment pierces the prism: two contacts ----
        arRes.m_numpts = 2;                                   // stw 2, 0x210(r24)

        // Edge points at the interval ends (vmaddfp: dir * splat(t) + base).
        lapPtsOnE[0] = MulAdd(lrEdge.dir, lfT0, lrEdge.base); // stvx128 -> [r29]
        lapPtsOnE[1] = MulAdd(lrEdge.dir, lfT1, lrEdge.base); // stvx128 -> [r29+0x10]

        // Ray-cast each edge point along sepDir onto the face plane (plane
        // point ff.edges[0].base, plane normal ff.ownNormal):
        //   t = dot3(ffBase - pt, ownNormal) / dot3(sepDir, ownNormal)
        // Both dots are vmsum3fp128 folds bounced through stack rows so the
        // divide is a SCALAR fdivs on lane 0 (the asm swaps which scratch row
        // holds numerator/denominator between the two casts -- same math).
        const Vec4 lvToFace0 = Sub(arFaceFeature.edges[0].base, lapPtsOnE[0]);   // vsubfp
        const f32  lfRay0    = Dot3(lvToFace0, arFaceFeature.ownNormal)
                             / Dot3(arSepDir, arFaceFeature.ownNormal);          // fdivs
        lapPtsOnF[0] = MulAdd(arSepDir, lfRay0, lapPtsOnE[0]);   // stvx128 -> [r30]

        const Vec4 lvToFace1 = Sub(arFaceFeature.edges[0].base, lapPtsOnE[1]);
        const f32  lfRay1    = Dot3(lvToFace1, arFaceFeature.ownNormal)
                             / Dot3(arSepDir, arFaceFeature.ownNormal);          // fdivs
        lapPtsOnF[1] = MulAdd(arSepDir, lfRay1, lapPtsOnE[1]);   // stvx128 -> [r30+0x10]

        return 1;
    }

    if (lfT1 - lfT0 <= KF_POINT_CONTACT_TOLERANCE)            // fcmpu cr6 / ble
    {
        // ---- (2) degenerate interval: single point contact ----
        arRes.m_numpts = 1;                                   // stw 1, 0x210(r24)

        // Edge point at the interval start.
        lapPtsOnE[0] = MulAdd(lrEdge.dir, lfT0, lrEdge.base); // stvx128 -> [r29]

        // Orthogonal offset along sepDir toward the face plane -- NO divide
        // on this path (sepDir is unit length):
        //   ptsOnF[0] = ptsOnE[0] + sepDir * dot3(sepDir, ffBase - ptsOnE[0])
        const Vec4 lvToFace = Sub(arFaceFeature.edges[0].base, lapPtsOnE[0]);
        lapPtsOnF[0] = MulAdd(arSepDir, Dot3(arSepDir, lvToFace), lapPtsOnE[0]);

        // Clamp the face point into the face region (in place); the returned
        // region bits accumulate into the face feature.
        arFaceFeature.region += ConstrainPointToFacePrism(arFaceFeature, arSepDir,
                                                          &lapPtsOnF[0]);

        // The edge point mirrors the clamped face point, then is constrained
        // back onto the edge segment; the edge feature's region code is
        // re-biased by the result.
        lapPtsOnE[0] = lapPtsOnF[0];                          // lvx128/stvx128 copy
        arEdgeFeature.region =
            arEdgeFeature.region - arEdgeFeature.edges[0].constrain_point(lapPtsOnE[0]) + 2;

        return 1;
    }

    // ---- (3) overlapping interval: two contacts swept along the edge ----
    const f32 lfSpan = lfT1 - lfT0;                           // stfs back over var_88
    arRes.m_numpts = 2;                                       // stw 2, 0x210(r24)

    // Drop the edge base into the prism cross-section:
    //   anchor  = efBase + sepDir   * dot3(sepDir,   ffBase     - efBase)
    //   anchor += crossDir          * dot3(crossDir, crossPoint - anchor)
    const Vec4 lvBaseToFace = Sub(arFaceFeature.edges[0].base, lrEdge.base);      // vsubfp
    Vec4 lvAnchor = MulAdd(arSepDir, Dot3(arSepDir, lvBaseToFace), lrEdge.base);
    const Vec4 lvToCross = Sub(lvCrossPoint, lvAnchor);                           // vsubfp
    lvAnchor = MulAdd(lvCrossDir, Dot3(lvCrossDir, lvToCross), lvAnchor);

    // Sweep along the edge direction across the clipped interval.
    lapPtsOnF[0] = MulAdd(lrEdge.dir, lfT0, lvAnchor);        // stvx128 -> [r30]
    lapPtsOnF[1] = MulAdd(lrEdge.dir, lfSpan, lapPtsOnF[0]);  // stvx128 -> [r30+0x10]

    // Clamp both face points into the face region; keep the (unsigned) larger
    // region code with its low bit stripped (cmplw max + clrrwi r11,r3,1).
    const u32 luRegion0 = ConstrainPointToFacePrism(arFaceFeature, arSepDir, &lapPtsOnF[0]);
    u32       luRegion  = ConstrainPointToFacePrism(arFaceFeature, arSepDir, &lapPtsOnF[1]);
    if (luRegion0 > luRegion)
    {
        luRegion = luRegion0;
    }
    arFaceFeature.region += (luRegion & 0xFFFFFFFEu);

    // Edge points mirror the clamped face points, constrained back onto the
    // edge segment (region results DISCARDED on this path).
    lapPtsOnE[0] = lapPtsOnF[0];                              // stvx128 -> [r29]
    lapPtsOnE[1] = lapPtsOnF[1];                              // stvx128 -> [r29+0x10]
    arEdgeFeature.edges[0].constrain_point(lapPtsOnE[0]);
    arEdgeFeature.edges[0].constrain_point(lapPtsOnE[1]);

    // Clamped-separation length |ptsOnF[0] - ptsOnE[0]|, guarded exactly like
    // the VMX sequence at 0x82BB8648..0x82BB8684:
    //   lenSq = vmsum3fp128(d, d)
    //   len   = lenSq * rsqrt(lenSq)      (vrsqrtefp + 2x Newton-Raphson refine)
    //   len   = (lenSq == 0) ? 0 : len    (vcmpeqfp zero mask + vsel)
    //   if (len > FLT_MIN)                (vcmpgtfp. vs flt_82180934)
    const Vec4 lvSep    = Sub(lapPtsOnF[0], lapPtsOnE[0]);
    const f32  lfLenSq  = Dot3(lvSep, lvSep);
    const f32  lfLen    = (lfLenSq == 0.0f)
                        ? 0.0f
                        : lfLenSq * (1.0f / std::sqrt(lfLenSq));
    if (lfLen > KF_MIN_SEPARATION_LENGTH)
    {
        // The clamps separated the point pairs: override the contact normal
        // with dir x (sep x dir) -- the component of the separation
        // perpendicular to the edge -- normalised.
        arRes.normalOverride = 1;                             // stw 1, 0x214(r24)

        const Vec4 lvSepAgain = Sub(lapPtsOnF[0], lapPtsOnE[0]);   // rows reloaded
        const Vec4 lvCross    = CrossRow(lvSepAgain, lrEdge.dir);  // sep x dir
        const Vec4 lvNormal   = CrossRow(lrEdge.dir, lvCross);     // dir x (sep x dir)

        // Normalise WITHOUT a zero guard on this path (vrsqrtefp + 2x
        // Newton-Raphson refine at 0x82BB86E0..0x82BB8700, then vmulfp128).
        const f32 lfScale = 1.0f / std::sqrt(Dot3(lvNormal, lvNormal));
        arRes.normal.x = lvNormal.x * lfScale;                // stvx128 -> r24+0x200
        arRes.normal.y = lvNormal.y * lfScale;
        arRes.normal.z = lvNormal.z * lfScale;
        arRes.normal.w = lvNormal.w * lfScale;
    }

    return 1;
}

// ===========================================================================
// rw::collision::FindFaceFacePrism @ 0x82BB8798   (canonical rwccore.h:1825)
//
// Face/face contact generation between two convex polygonal features. Walks
// both polygons' edge rings simultaneously, collecting (a) crossings of an
// ff2 edge through an ff1 edge's prism-wall plane and (b) the vertices that
// lie between consecutive crossings, capped at 8 points. When no crossing
// exists it falls back to containment (one face fully inside the other's
// prism) or reports separation.
//
// NOTE: the sepDir argument (r8, forwarded by the dispatcher) is never read
// by this body -- IDA accordingly recovered a 5-arg signature; the canonical
// 6-arg declaration is kept and the parameter is unused.
//
// rodata: flt_82180968 -- the parallel-edge cutoff on |dot3(dir2, pn1)|; the
// export carries the value 0.00000011920929 (FLT_EPSILON). flt_82001CC0 is
// the shared 0.0f every fcmpu sign test compares against.
// ===========================================================================

namespace
{
    // (flt_82180968 -- KF_PARALLEL_EPSILON -- and AllLanesGreaterScalar now
    // live in the shared block at the top of this file: FindEdgeEdgePrism and
    // ConstrainPointToFacePrism read the same literal and use the same test.)

    // Both faces cap the walk at 8 accumulated points (cmpwi r19, 8).
    const s32 KI_MAX_PRISM_POINTS = 8;

    // vmulfp128 (length * splat(scale)), vandc against the vspltisw(-1) +
    // vslw 0x80000000 sign mask (a lane fabs), then the same CR6 all-lanes
    // vcmpgtfp. test: true only when lfAbsLhs > |lLength.lane * lfScale| in
    // all four lanes.
    inline bool AllLanesAbsGreater(f32 lfAbsLhs, const Vec4& lLength, f32 lfScale)
    {
        return lfAbsLhs > std::fabs(lLength.x * lfScale)
            && lfAbsLhs > std::fabs(lLength.y * lfScale)
            && lfAbsLhs > std::fabs(lLength.z * lfScale)
            && lfAbsLhs > std::fabs(lLength.w * lfScale);
    }
}

RwBool FindFaceFacePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                         Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir)
{
    (void)arSepDir;   // r8 forwarded by the dispatcher, never read here

    // stw r9, 0x210(r3): the result count is zeroed up front, before any test.
    arRes.m_numpts = 0;

    s32 liState     = 0;    // r18  0 = no crossing yet; 1/2 = last crossing side
    s32 liPushVert1 = 0;    // r25  collect ff1 vertices while advancing ring 1
    s32 liPushVert2 = 0;    // r24  collect ff2 vertices while advancing ring 2
    s32 liAdvances1 = 0;    // r21  ring-1 advances (reset at the first crossing)
    s32 liAdvances2 = 0;    // r22  ring-2 advances (reset at the first crossing)
    s32 liNumPoints = 0;    // r19
    s32 liIndex2    = 0;    // r27  current edge of ff2
    s32 liIndex1    = 0;    // r26  current edge of ff1

    Vec4 laPoints[KI_MAX_PRISM_POINTS];   // the stack buffer behind r17

    // Bottom-tested ring walk (the asm's do/while back to loc_82BB87EC).
    for (;;)
    {
        const FeatureEdge& lrEdge1 = arFaceFeature1.edges[liIndex1];
        const FeatureEdge& lrEdge2 = arFaceFeature2.edges[liIndex2];

        // var_160 -> r23: ff1's edge direction against ff2's prism-wall normal.
        const bool lbDir1Negative = Dot3(lrEdge1.dir, lrEdge2.pn) < 0.0f;

        // var_130 -> r28: is ff1's edge origin outside ff2's prism wall?
        const bool lb1Outside2 =
            Dot3(lrEdge2.pn, Sub(lrEdge1.base, lrEdge2.base)) < 0.0f;

        // var_140 -> r29: is ff2's edge origin outside ff1's prism wall?
        const bool lb2Outside1 =
            Dot3(lrEdge1.pn, Sub(lrEdge2.base, lrEdge1.base)) < 0.0f;

        // var_120 / var_150: edge 2's crossing parameter with edge 1's wall
        // plane is lfNum / lfDenom.
        const f32 lfDenom = Dot3(lrEdge2.dir, lrEdge1.pn);
        const f32 lfNum   = Dot3(Sub(lrEdge1.base, lrEdge2.base), lrEdge1.pn);

        // ---- crossing attempt; any rejection falls through to the ring
        //      advance below (the asm's branches into loc_82BB89FC) ----
        if (!(std::fabs(lfDenom) < KF_PARALLEL_EPSILON))    // fabs + fcmpu/blt
        {
            // fdivs runs before the sign test in the asm; kept in that order.
            const f32 lfT = lfNum / lfDenom;

            // fmuls + fcmpu/blt rejects t < 0; the all-lanes test rejects
            // |t| beyond edge 2's length without dividing.
            if (!(lfDenom * lfNum < 0.0f) &&
                !AllLanesAbsGreater(std::fabs(lfNum), lrEdge2.length, lfDenom))
            {
                // P = dir2 * t + base2  (lvlx/vspltw t; vmaddfp).
                const Vec4 lvPoint = MulAdd(lrEdge2.dir, lfT, lrEdge2.base);

                // var_1A0: P's parameter along edge 1.
                const f32 lfS = Dot3(Sub(lvPoint, lrEdge1.base), lrEdge1.dir);
                if (!(lfS < 0.0f) && !AllLanesGreaterScalar(lfS, lrEdge1.length))
                {
                    // First crossing restarts the advance budget (r21/r22).
                    if (liState == 0)
                    {
                        liAdvances2 = 0;
                        liAdvances1 = 0;
                    }
                    laPoints[liNumPoints] = lvPoint;    // stvx128 to r17
                    ++liNumPoints;
                    if (liNumPoints >= KI_MAX_PRISM_POINTS)
                    {
                        break;                          // -> loc_82BB8AC0
                    }
                    if (!lb1Outside2)
                    {
                        liState     = 2;   // crossing exits ff2's prism: collect ff2 verts
                        liPushVert1 = 0;
                        liPushVert2 = 1;
                    }
                    else
                    {
                        liState     = 1;   // crossing enters ff2's prism: collect ff1 verts
                        liPushVert1 = 1;
                        liPushVert2 = 0;
                    }
                }
            }
        }

        // ---- ring advance (loc_82BB89FC): pick which face's edge retreats ----
        const bool lbAdvance1 = lbDir1Negative ? lb2Outside1 : !lb1Outside2;
        if (lbAdvance1)
        {
            // (idx + n - 1) % n: step backwards around ff1. The divw sits
            // behind twllei/twlgei divide-guard traps in the asm.
            const s32 liCount1 = arFaceFeature1.numedges;
            ++liAdvances1;
            liIndex1 = (liIndex1 + liCount1 - 1) % liCount1;
            if (liPushVert1)
            {
                // stvx128 v10: the origin of the ff1 edge we just left.
                laPoints[liNumPoints] = lrEdge1.base;
                ++liNumPoints;
            }
        }
        else
        {
            const s32 liCount2 = arFaceFeature2.numedges;
            ++liAdvances2;
            liIndex2 = (liIndex2 + liCount2 - 1) % liCount2;
            if (liPushVert2)
            {
                // stvx128 v9: the origin of the ff2 edge we just left.
                laPoints[liNumPoints] = lrEdge2.base;
                ++liNumPoints;
            }
        }

        if (liNumPoints >= KI_MAX_PRISM_POINTS)
        {
            break;
        }
        // Both rings fully traversed since the first crossing? (counts are
        // re-read from the features each pass, as the asm does.)
        if (!(liAdvances2 + liAdvances1 <
              arFaceFeature1.numedges + arFaceFeature2.numedges))
        {
            break;
        }
    }

    if (liNumPoints > 0)
    {
        // loc_82BB8C54: project every accumulated point onto both face
        // planes -- out = ownNormal * dot3(ownNormal, vert0 - P) + P.
        for (s32 liPoint = 0; liPoint < liNumPoints; ++liPoint)
        {
            const Vec4& lrPoint = laPoints[liPoint];

            lapPtsOn1[liPoint] = ProjectOntoFacePlane(arFaceFeature1.ownNormal,
                                                      arFaceFeature1.edges[0].base,
                                                      lrPoint);
            lapPtsOn2[liPoint] = ProjectOntoFacePlane(arFaceFeature2.ownNormal,
                                                      arFaceFeature2.edges[0].base,
                                                      lrPoint);
        }
        arRes.m_numpts = liNumPoints;   // stw r19, 0x210(r3)
        return 1;
    }

    // No crossings at all: containment. Is ff1's vertex 0 strictly inside
    // every prism wall of ff2? (loc_82BB8AC8; an empty ff2 counts as inside,
    // matching the ble straight to the emit path.)
    bool lb1Inside2 = true;
    const s32 liCount2 = arFaceFeature2.numedges;
    if (liCount2 > 0)
    {
        const Vec4 lvVert10 = arFaceFeature1.edges[0].base;   // lvx128 [r29]
        for (s32 liEdge = 0; liEdge < liCount2; ++liEdge)
        {
            const FeatureEdge& lrEdge = arFaceFeature2.edges[liEdge];
            if (!(Dot3(lrEdge.pn, Sub(lvVert10, lrEdge.base)) < 0.0f))
            {
                lb1Inside2 = false;   // bge -> loc_82BB8B84
                break;
            }
        }
    }

    if (lb1Inside2)
    {
        // LABEL_29 (loc_82BB8B1C): ff1 sits inside ff2's prism -- emit every
        // ff1 vertex raw, paired with its projection onto ff2's plane.
        s32 liEmitted = 0;   // r31; the count is re-read every pass (8B6C)
        while (liEmitted < arFaceFeature1.numedges)
        {
            const Vec4& lrVert1 = arFaceFeature1.edges[liEmitted].base;
            lapPtsOn1[liEmitted] = lrVert1;
            lapPtsOn2[liEmitted] = ProjectOntoFacePlane(arFaceFeature2.ownNormal,
                                                        arFaceFeature2.edges[0].base,
                                                        lrVert1);
            ++liEmitted;
        }
        arRes.m_numpts = arFaceFeature1.numedges;
        return 1;
    }

    // loc_82BB8B84: ff1's vertex is outside ff2 -- is ff2's vertex 0 inside
    // every prism wall of ff1? (an empty ff1 counts as inside.)
    const s32 liCount1 = arFaceFeature1.numedges;
    if (liCount1 > 0)
    {
        const Vec4 lvVert20 = arFaceFeature2.edges[0].base;
        for (s32 liEdge = 0; liEdge < liCount1; ++liEdge)
        {
            const FeatureEdge& lrEdge = arFaceFeature1.edges[liEdge];
            if (!(Dot3(lrEdge.pn, Sub(lvVert20, lrEdge.base)) < 0.0f))
            {
                return 0;   // bge -> loc_82BB8C20: fully separated
            }
        }
    }

    // LABEL_37 (loc_82BB8BD0, a do/while): ff2 sits inside ff1's prism --
    // emit every ff2 vertex projected onto ff1's plane, paired with the raw
    // vertex. Only reachable with a non-empty ff2 (the containment scan above
    // rejected on a real ff2 edge).
    s32 liEmitted = 0;   // r9 (v6)
    do
    {
        const Vec4& lrVert2 = arFaceFeature2.edges[liEmitted].base;
        lapPtsOn1[liEmitted] = ProjectOntoFacePlane(arFaceFeature1.ownNormal,
                                                    arFaceFeature1.edges[0].base,
                                                    lrVert2);
        lapPtsOn2[liEmitted] = lrVert2;
        ++liEmitted;
    } while (liEmitted < arFaceFeature2.numedges);
    arRes.m_numpts = arFaceFeature2.numedges;   // clrlwi+stw
    return 1;
}

// ===========================================================================
// rw::collision::FindFaceFacePrism4x3 @ 0x82BB8CA0  (canonical rwccore.h:1828)
// rw::collision::FindFaceFacePrism4x4 @ 0x82BB9400  (canonical rwccore.h:1831)
//
// Fully vectorised unrollings of the generic walk for a 4-edge face against a
// 3-edge (4x3) or 4-edge (4x4) face. Three candidate families, emitted in
// this order:
//   1. edge(ff2) x prism-wall-plane(ff1) crossings (4x3 or 4x4 pairs): a
//      candidate is valid when the edge is not parallel to the plane
//      (|denom| > epsilon), the hit parameter lies within the ff2 edge's
//      length, and the hit point lies within the ff1 edge's span. Every
//      candidate point is projected onto BOTH face planes; the valid ones are
//      appended to the two output rows.
//   2. ff1's vertices swept along sepDir onto the plane through ff2's base
//      vertex and kept when strictly inside all of ff2's prism walls;
//   3. symmetrically, ff2's vertices against ff1's walls.
// Epilogue: res.m_numpts = count (the ONLY store to res); return count != 0
// (the subfic/subfe/clrlwi idiom).
//
// The asm processes the candidates SIMD-parallel in SoA form (vmrghw/vmrglw
// transposes + vspltw broadcasts); only word 0 of each candidate's mask row
// is consumed by the scatter loops (lwzx), so the per-candidate masks are
// scalar bools and the vector compares against the length rows read lane x --
// exactly the lane the hardware mask word 0 came from. The crossing
// reciprocal is vrefp + two vnmsubfp/vmaddfp Newton-Raphson refines, rendered
// as the exact division (the |denom| guard kills the divergent case). The
// vertex points rebuilt from SoA carry a 1.0f w lane (interleave against the
// vcfsx(1,0) row), reproduced explicitly.
//
// unk_8327EFC0 -- the parallel-crossing guard vector. NO LONGER INFERRED
// (waveQ5 rwc2): it is a .DATA row that reads all-zero in the image because it
// is DYN-INIT SEEDED, not a .rdata constant (AGENTS gotcha 13). The seeding
// thunk is 4 instructions at 0x82C73DF0:
//     lis/addi r11, flt_82180968 ; lvlx v0 ; vspltw v0,v0,0 ;
//     stvx128 v0 -> unk_8327EFC0
// so the vector is splat(flt_82180968) == splat(1.1920929e-7f) == FLT_EPSILON.
// The previously inferred value was therefore correct and is now ATTESTED; the
// competing 1.0e-12f reading is wrong. (Two siblings are seeded by the same
// thunk run and are NOT yet corrected in the tree -- see the owner note:
// unk_8327EFD0 = splat(unk_82180A28) and unk_8327EEA0 = splat(flt_821801B4),
// both 0x34000000 == FLT_EPSILON, while Feature.cpp:29 and FeatureEdge.cpp:30
// still define them as the guessed 1.0e-12f.)
// ===========================================================================

namespace
{
    // unk_8327EFC0 = splat(flt_82180968) via the 0x82C73DF0 dyn-init thunk.
    const f32 KF_PARALLEL_EPSILON_VEC = 1.1920929e-7f;

    // -----------------------------------------------------------------------
    // One vertex-sweep block (phases 2/3 of both specialisations; the X360
    // body contains two structurally identical inline copies per function).
    // For each vertex V of arVertexFace (aiNumVerts of them):
    //   d = dot3(sepDir, planeBase - V)        planeBase = arPlaneFace's
    //                                          edge-0 base (+0x10)
    //   P = V + d*sepDir                       (w lane forced to 1.0f by the
    //                                          AoS rebuild against vcfsx(1,0))
    //   inside iff, for every edge m of arPlaneFace (aiNumPlanes of them),
    //       !(dot3(pn_m, P - base_m) >= 0)     (vcmpgefp + vnot + vand chain;
    //                                          a NaN plane test counts as
    //                                          inside, matching the asm)
    // Both support-plane projections are computed for ALL candidates first,
    // then the mask words are walked and accepted pairs are emitted.
    // -----------------------------------------------------------------------
    void SweepFaceVerticesPrism(const Feature& arVertexFace, s32 aiNumVerts,
                                const Feature& arPlaneFace, s32 aiNumPlanes,
                                const Vec4& arSepDir,
                                const Vec4& arBase1, const Vec4& arNorm1,
                                const Vec4& arBase2, const Vec4& arNorm2,
                                Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                                s32& riNumPts)
    {
        const Vec4& lrPlaneBase = arPlaneFace.edges[0].base;

        bool labAccept[4];   // one mask lane per vertex
        Vec4 laProj1[4];
        Vec4 laProj2[4];

        for (s32 lk = 0; lk < aiNumVerts; ++lk)
        {
            const Vec4& lrVert = arVertexFace.edges[lk].base;

            // d = dot3(sepDir, planeBase - V), accumulated y -> x -> z as the
            // vmulfp128 + two-vmaddfp chain does.
            const f32 lfD = (lrPlaneBase.z - lrVert.z) * arSepDir.z
                          + ((lrPlaneBase.x - lrVert.x) * arSepDir.x
                           + ((lrPlaneBase.y - lrVert.y) * arSepDir.y));

            // P = V + d*sepDir (vmaddfp per component); w = 1.0f from the
            // vmrghw/vmrglw AoS rebuild against the vcfsx(1,0) row.
            Vec4 lvSwept;
            lvSwept.x = lfD * arSepDir.x + lrVert.x;
            lvSwept.y = lfD * arSepDir.y + lrVert.y;
            lvSwept.z = lfD * arSepDir.z + lrVert.z;
            lvSwept.w = 1.0f;

            // Prism-wall containment: accept iff no plane reports
            // dot3(pn_m, P - base_m) >= 0.
            bool lbInside = true;
            for (s32 lm = 0; lm < aiNumPlanes; ++lm)
            {
                const FeatureEdge& lrEdge = arPlaneFace.edges[lm];
                const f32 lfPlaneTest =
                      (lvSwept.z - lrEdge.base.z) * lrEdge.pn.z
                    + ((lvSwept.x - lrEdge.base.x) * lrEdge.pn.x
                     + ((lvSwept.y - lrEdge.base.y) * lrEdge.pn.y));
                lbInside = lbInside && !(lfPlaneTest >= 0.0f);   // vcmpgefp + vnot
            }
            labAccept[lk] = lbInside;

            // Projections onto both faces' support planes (vmsum3fp128 +
            // vmaddfp), computed for every vertex regardless of the mask.
            laProj1[lk] = ProjectOntoFacePlane(arNorm1, arBase1, lvSwept);
            laProj2[lk] = ProjectOntoFacePlane(arNorm2, arBase2, lvSwept);
        }

        // Emit loop: walk the mask words in vertex order.
        for (s32 lk = 0; lk < aiNumVerts; ++lk)
        {
            if (labAccept[lk])
            {
                lapPtsOn1[riNumPts] = laProj1[lk];
                lapPtsOn2[riNumPts] = laProj2[lk];
                ++riNumPts;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Phase-1 crossing pass shared by both specialisations: edge j of ff2
    // against the prism-wall plane of edge i of ff1 (aiNum1 x aiNum2
    // candidates, slot aiNum2*i + j), staged then projected then scattered
    // exactly as the asm banks them.
    // -----------------------------------------------------------------------
    void CrossFaceEdgesPrism(const Feature& arFace1, s32 aiNum1,
                             const Feature& arFace2, s32 aiNum2,
                             const Vec4& arBase1, const Vec4& arNorm1,
                             const Vec4& arBase2, const Vec4& arNorm2,
                             Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                             s32& riNumPts)
    {
        Vec4 laCrossPoint[16];
        bool labValid[16];

        for (s32 li = 0; li < aiNum1; ++li)
        {
            const FeatureEdge& lrEdge1 = arFace1.edges[li];

            for (s32 lj = 0; lj < aiNum2; ++lj)
            {
                const FeatureEdge& lrEdge2 = arFace2.edges[lj];
                const s32 liSlot = li * aiNum2 + lj;

                // denom = dot3(dir2, pn1)                (vmsum3fp128)
                // numer = dot3(base1 - base2, pn1)       (vsubfp + vmsum3fp128)
                const f32 lfDenom = Dot3(lrEdge2.dir, lrEdge1.pn);
                const f32 lfNumer = Dot3(Sub(lrEdge1.base, lrEdge2.base), lrEdge1.pn);

                // t = numer/denom: vrefp + two Newton-Raphson refines,
                // rendered as the exact division (equivalent whenever the
                // |denom| guard below passes).
                const f32 lfT = lfNumer / lfDenom;

                // Candidate P = base2 + dir2*t (vmaddfp, all four lanes),
                // stored unconditionally like the asm's staging stvx128.
                laCrossPoint[liSlot] = MulAdd(lrEdge2.dir, lfT, lrEdge2.base);

                // Parameter-range rejection: vnot(vcmpgefp(num*denom, 0)) OR
                // vcmpgtfp(|num|, |length2 * denom|) marks INVALID, i.e. the
                // hit parameter must satisfy 0 <= t <= length2 without a
                // divide (vandc |.| against the sign mask). length rows are
                // broadcasts; lane x carries the compare.
                const bool lbInRange2 =
                    (lfNumer * lfDenom >= 0.0f) &&
                    !(std::fabs(lfNumer) > std::fabs(lrEdge2.length.x * lfDenom));

                // Foot-of-crossing span test along edge i:
                // s = dot3(P - base1, dir1); keep 0 <= s <= length1.
                const f32 lfS = Dot3(Sub(laCrossPoint[liSlot], lrEdge1.base), lrEdge1.dir);
                const bool lbInRange1 = (lfS >= 0.0f) && !(lfS > lrEdge1.length.x);

                // Accept = |denom| > epsilon AND both range masks.
                labValid[liSlot] = (std::fabs(lfDenom) > KF_PARALLEL_EPSILON_VEC) &&
                                   lbInRange2 && lbInRange1;
            }
        }

        const s32 liNumSlots = aiNum1 * aiNum2;

        // Projection pass over all candidates: project each staged point onto
        // both faces' support planes (vsubfp + vmsum3fp128 + vmaddfp per row;
        // the asm copies the mask rows aside first because it reuses the
        // staging buffer -- separate locals make the copy unnecessary).
        Vec4 laProj1[16];
        Vec4 laProj2[16];
        for (s32 liSlot = 0; liSlot < liNumSlots; ++liSlot)
        {
            laProj1[liSlot] = ProjectOntoFacePlane(arNorm1, arBase1, laCrossPoint[liSlot]);
            laProj2[liSlot] = ProjectOntoFacePlane(arNorm2, arBase2, laCrossPoint[liSlot]);
        }

        // Scatter: word 0 of each mask row gates the append, in slot order.
        for (s32 liSlot = 0; liSlot < liNumSlots; ++liSlot)
        {
            if (labValid[liSlot])
            {
                lapPtsOn1[riNumPts] = laProj1[liSlot];
                lapPtsOn2[riNumPts] = laProj2[liSlot];
                ++riNumPts;
            }
        }
    }
}

RwBool FindFaceFacePrism4x3(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir)
{
    s32 liCount = 0;   // r26, running output row index

    const Vec4& lrBase1 = arFaceFeature1.edges[0].base;   // ff1+0x10
    const Vec4& lrNorm1 = arFaceFeature1.ownNormal;       // ff1+0x210
    const Vec4& lrBase2 = arFaceFeature2.edges[0].base;   // ff2+0x10
    const Vec4& lrNorm2 = arFaceFeature2.ownNormal;       // ff2+0x210

    // Phase 1 -- ff2 edge j x ff1 side plane i, 4x3 = 12 candidates.
    CrossFaceEdgesPrism(arFaceFeature1, 4, arFaceFeature2, 3,
                        lrBase1, lrNorm1, lrBase2, lrNorm2,
                        lapPtsOn1, lapPtsOn2, liCount);

    // Phase 2 -- ff1's 4 vertices inside ff2's 3-wall prism.
    // Phase 3 -- ff2's 3 vertices inside ff1's 4-wall prism.
    SweepFaceVerticesPrism(arFaceFeature1, 4, arFaceFeature2, 3, arSepDir,
                           lrBase1, lrNorm1, lrBase2, lrNorm2,
                           lapPtsOn1, lapPtsOn2, liCount);
    SweepFaceVerticesPrism(arFaceFeature2, 3, arFaceFeature1, 4, arSepDir,
                           lrBase1, lrNorm1, lrBase2, lrNorm2,
                           lapPtsOn1, lapPtsOn2, liCount);

    // stw r26, 0x210(r3): the count is this function's only store to arRes,
    // written once at the end. Return (count != 0) -- subfic/subfe/clrlwi.
    arRes.m_numpts = liCount;
    return liCount != 0;
}

RwBool FindFaceFacePrism4x4(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir)
{
    s32 liCount = 0;   // r11 across all phases

    const Vec4& lrBase1 = arFaceFeature1.edges[0].base;   // r23 = ff1+0x10
    const Vec4& lrNorm1 = arFaceFeature1.ownNormal;       // r22 = ff1+0x210
    const Vec4& lrBase2 = arFaceFeature2.edges[0].base;   // r21 = ff2+0x10
    const Vec4& lrNorm2 = arFaceFeature2.ownNormal;       // r20 = ff2+0x210

    // Phase 1 (0x82BB9444) + 2a/2b (0x82BB956C/0x82BB95C4): the 16 edge-edge
    // crossings, staged, projected, then emitted in pair order.
    CrossFaceEdgesPrism(arFaceFeature1, 4, arFaceFeature2, 4,
                        lrBase1, lrNorm1, lrBase2, lrNorm2,
                        lapPtsOn1, lapPtsOn2, liCount);

    // Phase 3 (0x82BB9614): ff1's vertices swept along sepDir onto the plane
    // through ff2's base vertex, tested against ff2's edge planes.
    // Phase 4 (0x82BB98FC): ff2's vertices against ff1's edge planes.
    SweepFaceVerticesPrism(arFaceFeature1, 4, arFaceFeature2, 4, arSepDir,
                           lrBase1, lrNorm1, lrBase2, lrNorm2,
                           lapPtsOn1, lapPtsOn2, liCount);
    SweepFaceVerticesPrism(arFaceFeature2, 4, arFaceFeature1, 4, arSepDir,
                           lrBase1, lrNorm1, lrBase2, lrNorm2,
                           lapPtsOn1, lapPtsOn2, liCount);

    // Epilogue (0x82BB9BBC): res.m_numpts = count (stw r11, 0x210(r3));
    // return count != 0 (subfic/subfe/clrlwi).
    arRes.m_numpts = liCount;
    return liCount != 0;
}

// ===========================================================================
// rw::collision::FindFeatureIntersectionPrism @ 0x82BB9BD8
// (canonical rwccore.h:1834)
//
// Dispatch on the two features' edge counts (0 = point, 1 = edge, >1 = face
// with that many edges) to the specialised prism worker, resolving the
// point/point case inline. The only VMX in the body is two raw 16-byte row
// copies in the point-point case (whole-register moves, no lane arithmetic).
// Every worker call is a tail call (b), so each return below forwards the
// worker's return value unchanged. The point arrays handed to each worker
// keep each array with its own source feature: m_ptsOn1 always receives
// f1's points, m_ptsOn2 f2's, whichever argument slot the (possibly swapped)
// feature lands in. sepDir (r6, parked in r8) is passed through untouched.
// ===========================================================================
RwBool FindFeatureIntersectionPrism(rwc_FeatureIntersectionPrism& arRes,
                                    Feature& arFeature1, Feature& arFeature2,
                                    const Vec4& arSepDir)
{
    const s32 liCount1 = arFeature1.numedges;   // lwz r10, 0x230(r4)
    const s32 liCount2 = arFeature2.numedges;   // lwz r9,  0x230(r5)

    // add. r7, r9, r10 -- both features are points: the intersection is the
    // single pair of the two feature points.
    if (liCount2 + liCount1 == 0)
    {
        arRes.m_numpts = 1;                     // stw r9(=1), 0x210(r11)
        arRes.m_ptsOn1[0] = arFeature1.pt;      // lvx128 v0, r4, r10 / stvx128
        arRes.m_ptsOn2[0] = arFeature2.pt;      // lvx128 v0, r5, r10 / stvx128
        return 1;
    }

    // mullw. r7, r9, r10 -- zero iff exactly one of the two is a point.
    if (liCount2 * liCount1 != 0)
    {
        if (liCount1 == 1)
        {
            if (liCount2 == 1)
            {
                return FindEdgeEdgePrism(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                         arFeature1, arFeature2, arSepDir);
            }
            // Edge vs face: the face feature (2) takes the face slot.
            return FindFaceEdgePrism(arRes, arRes.m_ptsOn2, arRes.m_ptsOn1,
                                     arFeature2, arFeature1, arSepDir);
        }
        if (liCount2 == 1)
        {
            return FindFaceEdgePrism(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                     arFeature1, arFeature2, arSepDir);
        }
        if (liCount1 == 4 && liCount2 == 3)
        {
            return FindFaceFacePrism4x3(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                        arFeature1, arFeature2, arSepDir);
        }
        if (liCount1 == 3 && liCount2 == 4)
        {
            // The 4-edge face takes the first slot.
            return FindFaceFacePrism4x3(arRes, arRes.m_ptsOn2, arRes.m_ptsOn1,
                                        arFeature2, arFeature1, arSepDir);
        }
        if (liCount1 == 4 && liCount2 == 4)
        {
            return FindFaceFacePrism4x4(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                        arFeature1, arFeature2, arSepDir);
        }
        return FindFaceFacePrism(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                 arFeature1, arFeature2, arSepDir);
    }

    // Exactly one point feature: the non-point feature takes the first slot.
    if (liCount1 != 0)
    {
        if (liCount1 == 1)
        {
            return FindEdgePointPrism(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                      arFeature1, arFeature2, arSepDir);
        }
        return FindFacePointPrism(arRes, arRes.m_ptsOn1, arRes.m_ptsOn2,
                                  arFeature1, arFeature2, arSepDir);
    }
    if (liCount2 == 1)
    {
        return FindEdgePointPrism(arRes, arRes.m_ptsOn2, arRes.m_ptsOn1,
                                  arFeature2, arFeature1, arSepDir);
    }
    return FindFacePointPrism(arRes, arRes.m_ptsOn2, arRes.m_ptsOn1,
                              arFeature2, arFeature1, arSepDir);
}

} // namespace collision
} // namespace rw
