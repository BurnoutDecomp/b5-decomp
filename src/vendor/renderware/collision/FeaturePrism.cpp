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
//   rw::collision::FindEdgePointPrism            @ 0x82BB77C0
//   rw::collision::FindFacePointPrism            @ 0x82BB7F18
//   rw::collision::FindFaceEdgePrism             @ 0x82BB83A8
//   rw::collision::FindFaceFacePrism             @ 0x82BB8798
//   rw::collision::FindFaceFacePrism4x3          @ 0x82BB8CA0
//   rw::collision::FindFaceFacePrism4x4          @ 0x82BB9400
//   rw::collision::FindFeatureIntersectionPrism  @ 0x82BB9BD8
//
// Canonical declarations: Feb-2007 rwccore.h:1810-1836 (the whole family in
// one header). FindEdgeEdgePrism is PENDING (not delivered this wave); the
// dispatcher calls it through the declaration in GPInstance.hpp.
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
// ===========================================================================

// PENDING @ 0x82BB81D0 (unnamed static of this TU; not delivered this wave) --
// clips the edge feature's segment against the face prism. Outputs the
// parametric interval [*lpfT0, *lpfT1] along ef.edges[0] plus a crossing
// point / crossing direction pair reused by the parallel-overlap path;
// returns nonzero when the segment pierces the prism cross-section.
// (X360: r3=&ff r4=&ef r5=&sepDir r6=t0 r7=t1 r8=crossPoint r9=crossDir.)
// FLAGGED: descriptive name, signature attested purely from this call site.
extern RwBool FindFaceEdgePrismInterval(Feature& arFaceFeature, Feature& arEdgeFeature,
                                        const Vec4& arSepDir,
                                        f32* lpfT0, f32* lpfT1,
                                        Vec4* lpCrossPoint, Vec4* lpCrossDir);

// PENDING @ 0x82BB8050 (unnamed static of this TU; not delivered this wave) --
// constrains *lpPoint into the face feature's prism cross-section in place
// (sweep direction sepDir) and returns a region-code contribution; callers
// accumulate it into Feature::region (FindFaceEdgePrism strips bit 0 on its
// two-point path). (X360: r3=&ff r4=&sepDir r5=point.) FLAGGED name.
extern u32 ConstrainPointToFacePrism(Feature& arFaceFeature, const Vec4& arSepDir,
                                     Vec4* lpPoint);

namespace
{
    const f32 KF_POINT_CONTACT_TOLERANCE = 0.001f;          // flt_821809D4
    const f32 KF_MIN_SEPARATION_LENGTH   = 1.1754944e-38f;  // flt_82180934 (FLT_MIN)
}

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
    // flt_82180968 (value attested by the export).
    const f32 KF_PARALLEL_EPSILON = 1.1920929e-7f;

    // Both faces cap the walk at 8 accumulated points (cmpwi r19, 8).
    const s32 KI_MAX_PRISM_POINTS = 8;

    // vcmpgtfp. + mfocrf/extrwi on CR6: true only when splat(lfLhs) > lRhs
    // holds in ALL four lanes. (length is lane-broadcast by the FeatureEdge
    // ctor, so this equals the lane-x test; kept per-lane for fidelity. NaN
    // lanes compare false, defeating the all-true bit, exactly like the
    // scalar > here.)
    inline bool AllLanesGreaterScalar(f32 lfLhs, const Vec4& lRhs)
    {
        return lfLhs > lRhs.x
            && lfLhs > lRhs.y
            && lfLhs > lRhs.z
            && lfLhs > lRhs.w;
    }

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
// .rdata: unk_8327EFC0 -- the parallel-crossing guard vector, reached only by
// reference (no value in the export). FLAGGED as inferred: the generic
// FindFaceFacePrism (same TU) guards the identical |denominator| with the
// attested literal FLT_EPSILON, so that value is used for both
// specialisations. (An alternative reading by analogy with the neighbouring
// unk_8327EFD0 epsilon would be 1.0e-12f; the conductor picked the
// same-quantity generic-body attestation.)
// ===========================================================================

namespace
{
    // unk_8327EFC0 (inferred -- see the block comment above).
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
