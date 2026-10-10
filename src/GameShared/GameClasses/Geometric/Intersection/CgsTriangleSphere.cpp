// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp
//
// ⭐⭐ THE SPHERE CONTACT KERNEL (walls leg 2, 2026-08-14). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   CgsGeometric::IntersectTriangle4Sphere_HackyBurnoutVersion @0x8283D2E0 (497)
//
// This is the narrow phase of race-car world contact generation: the worker
// ContactGeneratorJob::ExecuteSphereListWithTriangleList runs it once per
// (sensor sphere, Triangle4 batch) pair, and every wall/floor contact the
// deformation penetration solver will ever see is produced here.
//
// ─── HOW THIS WAS DERIVED (method, verbatim reproducible) ───────────────────
// The console body is 497 straight-line VMX128 instructions with no calls and a
// single rodata reference. It was NOT paraphrased from IDA's pseudocode:
//   1. every instruction word was read out of the image (x360rd) and its
//      OPERANDS re-decoded from the raw word -- IDA prints VMX128 source
//      registers "+32" per operand field, and this body had SEVENTY-EIGHT such
//      misprints (all vor128/vmulfp128/vmaddfp128 B-fields);
//   2. the decoded body was executed numerically, lane-exact, on float32
//      (scratchpad sim_kernel.py; vrsqrtefp modelled as exact 1/sqrt -- the
//      same abstraction this lowering ships, see PC LOWERING below);
//   3. this scalar implementation was fuzz-compared against that execution:
//      600 single-lane + 500x4 distinct-triangle lane trials across random
//      triangles/spheres/paddings/edge-cosines = 2600 checks, 0 mismatches.
//      Two structural facts were settled BY probe, not by reading:
//      the vsel cascade's precedence (the LAST violated edge in order
//      P0->P1, P1->P2, P2->P0 supplies the closest point -- a sphere outside
//      two edge lines takes the SECOND edge's clamp) and the row pairing of
//      the three edge-cosine thresholds (+0xB0 = edge P0->P1, +0xC0 = P1->P2,
//      +0xD0 = P2->P0).
// The PS3 twin (DecFIGS PPU ELF @0xB591CC, full DWARF) supplied the parameter
// names and the KF_MIN_PLANE_DIST identity; the X360 image is the authority
// for every constant and the structure.
//
// ─── WHY "HackyBurnoutVersion" ──────────────────────────────────────────────
// Two deliberate hacks vs the textbook sphere/triangle contact:
//   1. the reported contact normal is the DIRECTION FROM CLOSEST POINT TO
//      CENTRE for edge/vertex contacts (both output normals are the same
//      register on both consoles), and
//   2. edge contacts are FILTERED BY PER-EDGE COSINE THRESHOLDS baked into the
//      Triangle4 by the cache-fill leg (LoadEdgeCosines): an edge contact is
//      kept only if dot(contactDir, faceNormal) clears the edge's threshold.
//      That is the classic internal-edge ("ghost bump") suppression -- a car
//      sliding along a tessellated wall must not catch on interior seams.
//
// ─── PC LOWERING, stated once ───────────────────────────────────────────────
// Scalar per-lane float math, the established precedent for this exact family
// (CgsPolygonSoupTests.cpp TestSphereTriangle4SOA, ContactGeneratorJob.cpp):
// once the SoA lanes are array indexes there is no swizzle left to get wrong.
// ⚠️ FLAGGED non-bit-identical spots, same wording as the precedent: the
// console's vrsqrtefp + Newton-Raphson refinements are lowered to 1/sqrt()
// (more accurate by a few ulps; changes no accept/reject except within a ulp
// of a threshold).
// ============================================================================

#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"  // the swept kernel below
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"     // CgsGeometric::Triangle (IntersectTriangleSweptSphere)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (IntersectTriangleSweptSphere)
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::IsValid(Vector3)
#include "pc/geometric/SweptSphereSIMD.h"

#include <cmath>     // std::sqrt (the vrsqrtefp lowering)
#include <cstring>   // std::memcpy (mask lane bit patterns)
#include <cstdlib>

namespace CgsGeometric
{
    // unk_83039220 -- ZERO in the image; written at static-init by sub_82C6DCF0
    // (CgsNumeric::CreateFloatVector(0.001f) -> stvx128). The PS3 initializer
    // builds the identical splat. Modelled at its post-init value; a namespace
    // const here because the only consumer is this TU (the X360 kernel is its
    // single exported reader).
    const VecFloat KF_MIN_PLANE_DIST = { 0.001f, 0.001f, 0.001f, 0.001f };

    Triangle4::Mask4 IntersectTriangle4Sphere_HackyBurnoutVersion(
        const Sphere&    lSphere,
        const Triangle4& lTriangles,
        VecFloat         lInPadding,
        Vector3& lContactNormal0, Vector3& lTriangleNormal0,
        Vector3Plus& lSphereContactPoint0, Vector3Plus& lTriangleContactPoint0,
        Vector3& lContactNormal1, Vector3& lTriangleNormal1,
        Vector3Plus& lSphereContactPoint1, Vector3Plus& lTriangleContactPoint1,
        Vector3& lContactNormal2, Vector3& lTriangleNormal2,
        Vector3Plus& lSphereContactPoint2, Vector3Plus& lTriangleContactPoint2,
        Vector3& lContactNormal3, Vector3& lTriangleNormal3,
        Vector3Plus& lSphereContactPoint3, Vector3Plus& lTriangleContactPoint3)
    {
        // The output groups, indexable per lane (the console receives these as
        // sixteen distinct reference parameters; grouping them is pure syntax).
        Vector3*     lapContactNormal[4]       = { &lContactNormal0, &lContactNormal1,
                                                   &lContactNormal2, &lContactNormal3 };
        Vector3*     lapTriangleNormal[4]      = { &lTriangleNormal0, &lTriangleNormal1,
                                                   &lTriangleNormal2, &lTriangleNormal3 };
        Vector3Plus* lapSphereContactPoint[4]  = { &lSphereContactPoint0, &lSphereContactPoint1,
                                                   &lSphereContactPoint2, &lSphereContactPoint3 };
        Vector3Plus* lapTriangleContactPoint[4]= { &lTriangleContactPoint0, &lTriangleContactPoint1,
                                                   &lTriangleContactPoint2, &lTriangleContactPoint3 };

        // lvx128 v11, r0, r3 + four vspltw: centre xyz and radius.
        const f32 lfCx = lSphere.mPositionRadius.x;
        const f32 lfCy = lSphere.mPositionRadius.y;
        const f32 lfCz = lSphere.mPositionRadius.z;
        const f32 lfRadius = lSphere.mPositionRadius.w;

        // vaddfp v2, v21, v2 -- the reach of the test is radius + padding.
        const f32 lfReach = lfRadius + lInPadding.x;

        Triangle4::Mask4 lResult;
        lResult.SetZero();

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const f32 lfP0x = (&lTriangles.mVertex0X.x)[liLane];
            const f32 lfP0y = (&lTriangles.mVertex0Y.x)[liLane];
            const f32 lfP0z = (&lTriangles.mVertex0Z.x)[liLane];
            const f32 lfP1x = (&lTriangles.mVertex1X.x)[liLane];
            const f32 lfP1y = (&lTriangles.mVertex1Y.x)[liLane];
            const f32 lfP1z = (&lTriangles.mVertex1Z.x)[liLane];
            const f32 lfP2x = (&lTriangles.mVertex2X.x)[liLane];
            const f32 lfP2y = (&lTriangles.mVertex2Y.x)[liLane];
            const f32 lfP2z = (&lTriangles.mVertex2Z.x)[liLane];

            // ---- the edge loop e0 = P1-P0, e1 = P2-P1, e2 = P0-P2 and the face
            // normal n = normalize(cross(e0, e1)) (vrsqrtefp + NR -> 1/sqrt) ----
            const f32 lfE0x = lfP1x - lfP0x, lfE0y = lfP1y - lfP0y, lfE0z = lfP1z - lfP0z;
            const f32 lfE1x = lfP2x - lfP1x, lfE1y = lfP2y - lfP1y, lfE1z = lfP2z - lfP1z;
            const f32 lfE2x = lfP0x - lfP2x, lfE2y = lfP0y - lfP2y, lfE2z = lfP0z - lfP2z;

            const f32 lfNx = (lfE0y * lfE1z) - (lfE0z * lfE1y);
            const f32 lfNy = (lfE0z * lfE1x) - (lfE0x * lfE1z);
            const f32 lfNz = (lfE0x * lfE1y) - (lfE0y * lfE1x);
            const f32 lfInvNLen =
                1.0f / std::sqrt((lfNx * lfNx) + (lfNy * lfNy) + (lfNz * lfNz));
            const f32 lfUNx = lfNx * lfInvNLen;
            const f32 lfUNy = lfNy * lfInvNLen;
            const f32 lfUNz = lfNz * lfInvNLen;

            // ---- signed plane distance of the centre, and its projection ------
            const f32 lfPlaneDist = ((lfCx - lfP0x) * lfUNx)
                                  + ((lfCy - lfP0y) * lfUNy)
                                  + ((lfCz - lfP0z) * lfUNz);
            const f32 lfFacePx = lfCx - (lfPlaneDist * lfUNx);
            const f32 lfFacePy = lfCy - (lfPlaneDist * lfUNy);
            const f32 lfFacePz = lfCz - (lfPlaneDist * lfUNz);

            // ---- the three outside-half-plane tests (vcmpgtfp x3): the face
            // projection against each edge's outward perpendicular cross(e, n) --
            const f32 lafAx[3] = { lfP0x, lfP1x, lfP2x };
            const f32 lafAy[3] = { lfP0y, lfP1y, lfP2y };
            const f32 lafAz[3] = { lfP0z, lfP1z, lfP2z };
            const f32 lafEx[3] = { lfE0x, lfE1x, lfE2x };
            const f32 lafEy[3] = { lfE0y, lfE1y, lfE2y };
            const f32 lafEz[3] = { lfE0z, lfE1z, lfE2z };

            bool labOutside[3];
            for (s32 liEdge = 0; liEdge < 3; ++liEdge)
            {
                const f32 lfOwx = (lafEy[liEdge] * lfUNz) - (lafEz[liEdge] * lfUNy);
                const f32 lfOwy = (lafEz[liEdge] * lfUNx) - (lafEx[liEdge] * lfUNz);
                const f32 lfOwz = (lafEx[liEdge] * lfUNy) - (lafEy[liEdge] * lfUNx);
                labOutside[liEdge] =
                    (((lfFacePx - lafAx[liEdge]) * lfOwx)
                   + ((lfFacePy - lafAy[liEdge]) * lfOwy)
                   + ((lfFacePz - lafAz[liEdge]) * lfOwz)) > 0.0f;
            }
            const bool lbInsideFace = !labOutside[0] && !labOutside[1] && !labOutside[2];

            // ---- closest point: face projection, overridden by each violated
            // edge's clamped segment point. ⚠️ THE CASCADE ORDER IS LOAD-BEARING:
            // the LAST violated edge in order 0,1,2 wins (probe-settled; a centre
            // outside the e0 AND e1 lines takes e1's clamp = the shared vertex).
            f32 lfClosestX = lfFacePx, lfClosestY = lfFacePy, lfClosestZ = lfFacePz;
            for (s32 liEdge = 0; liEdge < 3; ++liEdge)
            {
                if (!labOutside[liEdge])
                {
                    continue;
                }
                const f32 lfELen = std::sqrt((lafEx[liEdge] * lafEx[liEdge])
                                           + (lafEy[liEdge] * lafEy[liEdge])
                                           + (lafEz[liEdge] * lafEz[liEdge]));
                const f32 lfInvELen = 1.0f / lfELen;   // vrsqrtefp + NR -> 1/sqrt
                const f32 lfEux = lafEx[liEdge] * lfInvELen;
                const f32 lfEuy = lafEy[liEdge] * lfInvELen;
                const f32 lfEuz = lafEz[liEdge] * lfInvELen;

                // t = clamp(dot(C - A, eu), 0, |e|)   (vmaxfp with 0, vminfp with |e|)
                f32 lfT = ((lfCx - lafAx[liEdge]) * lfEux)
                        + ((lfCy - lafAy[liEdge]) * lfEuy)
                        + ((lfCz - lafAz[liEdge]) * lfEuz);
                if (lfT < 0.0f)   { lfT = 0.0f; }
                if (lfT > lfELen) { lfT = lfELen; }

                lfClosestX = lafAx[liEdge] + (lfEux * lfT);
                lfClosestY = lafAy[liEdge] + (lfEuy * lfT);
                lfClosestZ = lafAz[liEdge] + (lfEuz * lfT);
            }

            // ---- distance and the effective contact direction -----------------
            const f32 lfDx = lfCx - lfClosestX;
            const f32 lfDy = lfCy - lfClosestY;
            const f32 lfDz = lfCz - lfClosestZ;
            const f32 lfDist =
                std::sqrt((lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz));
            const f32 lfInvDist = 1.0f / lfDist;       // vrsqrtefp + NR -> 1/sqrt
            const f32 lfDirX = lfDx * lfInvDist;
            const f32 lfDirY = lfDy * lfInvDist;
            const f32 lfDirZ = lfDz * lfInvDist;

            // dot(contactDir, faceNormal) -- what the edge-cosine gates compare.
            const f32 lfDotDN = (lfDirX * lfUNx) + (lfDirY * lfUNy) + (lfDirZ * lfUNz);

            // ---- outputs (written unconditionally, exactly as the console's
            // sixteen stvx128 are; only hit lanes are ever consumed) ------------
            // ⚠️ Both normals are the SAME value on the console (one register,
            // two stores): the effective contact direction.
            lapContactNormal[liLane]->x  = lfDirX;
            lapContactNormal[liLane]->y  = lfDirY;
            lapContactNormal[liLane]->z  = lfDirZ;
            *lapTriangleNormal[liLane]   = *lapContactNormal[liLane];

            lapTriangleContactPoint[liLane]->x = lfClosestX;
            lapTriangleContactPoint[liLane]->y = lfClosestY;
            lapTriangleContactPoint[liLane]->z = lfClosestZ;

            lapSphereContactPoint[liLane]->x = lfCx - (lfDirX * lfRadius);
            lapSphereContactPoint[liLane]->y = lfCy - (lfDirY * lfRadius);
            lapSphereContactPoint[liLane]->z = lfCz - (lfDirZ * lfRadius);

            // ---- the hit mask -------------------------------------------------
            // Console shape: & of (planeDist >= KF_MIN_PLANE_DIST), (reach >=
            // dist), (inside-face | dotDN >= 0), the three per-edge cosine gates,
            // and the batch's own per-lane valid mask. NaN comparisons are false
            // in both the vcmp* and this C, so a degenerate lane self-rejects.
            const bool lbAccept =
                   (lfPlaneDist >= KF_MIN_PLANE_DIST.x)
                && (lfReach >= lfDist)
                && (lbInsideFace || (lfDotDN >= 0.0f))
                && (!labOutside[0] || (lfDotDN >= (&lTriangles.mEdge0Cosigns.x)[liLane]))
                && (!labOutside[1] || (lfDotDN >= (&lTriangles.mEdge1Cosigns.x)[liLane]))
                && (!labOutside[2] || (lfDotDN >= (&lTriangles.mEdge2Cosigns.x)[liLane]));

            // vand with mValidMasks: the producer writes full-word lane masks, so
            // the bitwise AND is reproduced on the exact bit patterns.
            u32 luEnableBits = 0;
            std::memcpy(&luEnableBits, &(&lTriangles.mValidMasks.x)[liLane], sizeof(u32));
            const u32 luLaneMask = (lbAccept ? 0xFFFFFFFFu : 0u) & luEnableBits;
            std::memcpy(&(&lResult.x)[liLane], &luLaneMask, sizeof(u32));
        }

        return lResult;
    }
}

// =============================================================================================
// ⭐⭐⭐ THE SWEPT (CONTINUOUS) CONTACT KERNEL — swept leg, 2026-08-16.
//
//   CgsGeometric::IntersectTriangle4SweptSphere        @0x8283EF50 (896)
//   CgsGeometric::Intersect2DCircleWithTriangleSOA     @0x82839AC0 (156)  [inlined below]
//
// WHY THIS EXISTS AND WHY THE IN-PLACE KERNEL ABOVE CANNOT STAND IN FOR IT.
// DoRaceCarWorldContactGeneration picks between two contact generators per car per frame:
// below ~6 m/s it posts a SphereList job (the kernel above, which tests the sensor spheres
// where they are RIGHT NOW), and above it a SweptSphereList job (this one, which tests the
// volume each sphere sweeps out over the frame). At 30 m/s a sphere travels ~0.5 m between
// frames, so an in-place test simply misses the wall it passes through. Until this landed the
// swept arm was an unimplemented gate, and the car had NO body-shell collision above walking
// pace anywhere in the map.
//
// ─── HOW THIS WAS DERIVED (method, verbatim reproducible) ────────────────────────────────
//   1. every instruction word read out of the image (scratchpad x360rd, self-test 10/10) and
//      its OPERANDS re-decoded from the raw word — IDA prints VMX128 source registers "+32"
//      per operand FIELD, and these two bodies carry 169 such misprints. The decoder was
//      round-tripped on the already-verified sphere kernel @0x8283D2E0 first;
//   2. the decoded bodies executed SYMBOLICALLY to read the algorithm off the dataflow
//      (scratchpad swept/symtrace.py) and NUMERICALLY on float32 lanes to produce ground
//      truth (scratchpad swept/sim_swept.py, which inlines the @0x82839AC0 call);
//   3. this scalar lowering fuzz-compared against that execution: 2000 random
//      triangle/swept-sphere trials (475 of them console-hits) plus a hand-built adversarial
//      set (sweep parallel to the plane inside and outside the slab, zero-length sweep,
//      back-face approach, exact vertex graze, 800 m sweep, 1e-4 m radius, collinear and
//      fully degenerate triangles, per-lane valid masking) — 0 hit-mask mismatches and
//      0 value mismatches on every output lane.
// The DecFIGS PS3 DWARF (CgsTriangleSphere.cpp:2128) supplied the parameter list and the
// names of the helpers the console inlined into this body: SolveSweptCircleEquationSOA
// (:1958), IsBetween0And1SOA (:1989), Intersect2DSweptCircleWithVertexAndEdgeSOA (:2024).
// The X360 image is the authority for every structural decision below.
//
// ─── THE ALGORITHM ───────────────────────────────────────────────────────────────────────
// Per lane, with C the sphere centre, r its radius and D = direction * length the sweep:
//   * N = normalize(cross(P1-P0, P2-P1)), the single-sided face normal; d = dot(N,P0);
//   * an in-plane orthonormal frame: U = normalize(P1-P0), V = normalize(cross(U,N));
//   * clip the sweep to the plane slab of half-thickness r: the two crossing times
//     (h0 -/+ r) / -dot(D,N) give [tEnter,tExit] clamped to [0,1]; a sweep parallel to the
//     plane is [0,1] if |h0| < r and rejected otherwise;
//   * FACE contact: at tEnter the sphere cuts a circle of radius^2 = r^2 - h(tEnter)^2 in
//     the plane. Intersect2DCircleWithTriangleSOA gives the closest point on the triangle and
//     whether the circle reaches it;
//   * EDGE/VERTEX contact: for each of the three (vertex, outgoing edge) pairs, solve the
//     swept-circle quadratic for the vertex and for the edge's perpendicular component, keep
//     the earlier root that lies in [0,1] (and, for an edge, whose foot lies within [0,|e|]);
//   * the FACE result wins outright when it hits; otherwise the earliest of the three pairs;
//   * outputs: TriangleContactPoint is the contact point on the triangle, SphereContactPoint
//     is the same point brought back into the sphere's START frame (P - D*t) for a swept hit
//     or the point on the sphere's surface (C + n*r) for a face hit, ContactNormal points
//     from C towards it, TriangleNormal is N — and ⭐ THE CONTACT TIME RIDES IN THE w LANE OF
//     BOTH CONTACT POINTS (the console's final vmrghw pairs each xyz with tHit);
//   * the lane is accepted when the slab was not rejected, something hit, the contact normal
//     OPPOSES the face normal (single-sided), and the batch's own valid-mask lane is set.
//
// ⚠️ NO PADDING, NO KF_MIN_PLANE_DIST, NO EDGE-COSINE GATES. All three appear in the in-place
// kernel above and NONE of them appears here — checked instruction by instruction, not
// assumed from the family. The swept kernel's only rejection filters are the slab clip, the
// root windows and the single-sided normal test.
//
// ⚠️ PC LOWERING, same flag and same precedent as the kernel above: the console refines
// `vrsqrtefp` and `vrefp` with in-line Newton-Raphson (two steps for the quadratic solver's
// 1/(2a) and its sqrt(discriminant), one for each normalisation); all of them are lowered to
// the exact operation here. More accurate by a few ulps, decision-identical except within a
// ulp of a threshold, and it is the abstraction the numeric model above was verified against.
// =============================================================================================

namespace CgsGeometric
{
    namespace
    {
        // A point in the triangle's own in-plane (U,V) frame. The console has no such type —
        // it carries every one of these as two separate SoA registers, which is exactly why
        // Intersect2DCircleWithTriangleSOA takes eighteen VectorIntrinsics by value.
        struct SweptPlanePoint2 { f32 u, v; };

        // vminfp / vmaxfp: `a < b ? a : b` and `a > b ? a : b`. Spelled out rather than reused
        // from <algorithm> so the NaN behaviour matches the console's — a NaN operand makes the
        // comparison false and the SECOND operand wins, in both.
        inline f32 SweptMin(f32 lfA, f32 lfB) { return (lfA < lfB) ? lfA : lfB; }
        inline f32 SweptMax(f32 lfA, f32 lfB) { return (lfA > lfB) ? lfA : lfB; }

        // -------------------------------------------------------------------------------------
        // CgsGeometric::Intersect2DCircleWithTriangleSOA @0x82839AC0 (156), scalar-lowered.
        // Returns "the circle reaches the triangle" and writes the closest point on the
        // triangle to the circle centre (the centre itself when it is inside).
        //
        // lfRadiusSq doubles as the initial best-distance-squared, which is what makes the
        // "found anything" flag mean "within the radius": a candidate is only taken when its
        // distance squared beats the current best, and the first best IS r^2.
        //
        // ⚠️⚠️ VERTEX C IS NOT A CANDIDATE, AND THAT IS MEASURED, NOT AN OMISSION HERE.
        // The console tests vertex A (against edge AB's direction), then vertex B (against
        // edge BC's) — and then emits the vertex-B test a SECOND time, against the same
        // |B-centre|^2 and the same `0 >= dot(B->centre, dirBC)` predicate (0x82839BAC..
        // 0x82839C64; registers resolved by hand, the repeat selects v6/v7 = B.u/B.v, not the
        // third vertex). The repeat is idempotent, so the observable candidate set really is
        // {A, B, edge AB, edge BC, edge CA}. Reproduced as shipped: a circle sitting in vertex
        // C's Voronoi region falls through to "no face contact" here and is picked up by the
        // VERTEX SWEEP in the caller instead, which does test all three vertices.
        // -------------------------------------------------------------------------------------
        bool Intersect2DCircleWithTriangleSOA(SweptPlanePoint2 lCentre, f32 lfRadiusSq,
                                           SweptPlanePoint2 lA, SweptPlanePoint2 lB,
                                           SweptPlanePoint2 lC,
                                           SweptPlanePoint2 lDirAB, SweptPlanePoint2 lDirBC,
                                           SweptPlanePoint2 lDirCA,
                                           f32 lfLenAB, f32 lfLenBC, f32 lfLenCA,
                                           SweptPlanePoint2& lrClosestPoint)
        {
            // The winding test: the centre is inside when it lies left of all three directed
            // edges (0x82839B24..0x82839B54, three vnmsubfp + three vcmpgefp + two vand).
            const f32 lfSideAB = (lDirAB.u * (lA.v - lCentre.v)) - (lDirAB.v * (lA.u - lCentre.u));
            const f32 lfSideBC = (lDirBC.u * (lB.v - lCentre.v)) - (lDirBC.v * (lB.u - lCentre.u));
            const f32 lfSideCA = (lDirCA.u * (lC.v - lCentre.v)) - (lDirCA.v * (lC.u - lCentre.u));
            const bool lbInside = (lfSideBC >= 0.0f) && (lfSideAB >= 0.0f) && (lfSideCA >= 0.0f);

            const SweptPlanePoint2 laQ[3] =
            {
                { lCentre.u - lA.u, lCentre.v - lA.v },
                { lCentre.u - lB.u, lCentre.v - lB.v },
                { lCentre.u - lC.u, lCentre.v - lC.v },
            };
            const SweptPlanePoint2 laBase[3] = { lA, lB, lC };
            const SweptPlanePoint2 laDir[3]  = { lDirAB, lDirBC, lDirCA };
            const f32              lafLen[3] = { lfLenAB, lfLenBC, lfLenCA };

            // The projection of each vertex-to-centre offset onto that vertex's OUTGOING edge.
            f32  lafProj[3];
            bool labBehind[3];
            for (s32 liEdge = 0; liEdge < 3; ++liEdge)
            {
                lafProj[liEdge] = (laQ[liEdge].u * laDir[liEdge].u)
                                + (laQ[liEdge].v * laDir[liEdge].v);
                labBehind[liEdge] = (0.0f >= lafProj[liEdge]);
            }

            f32  lfBest  = lfRadiusSq;
            bool lbFound = false;
            lrClosestPoint = lCentre;

            // The two vertex candidates, in the console's order. (Vertex C is absent — see the
            // banner above; `liVertex < 2` is the measured bound, not a typo.)
            for (s32 liVertex = 0; liVertex < 2; ++liVertex)
            {
                const f32 lfDistSq = (laQ[liVertex].u * laQ[liVertex].u)
                                   + (laQ[liVertex].v * laQ[liVertex].v);
                if ((lfBest >= lfDistSq) && labBehind[liVertex])
                {
                    lfBest = lfDistSq;
                    lrClosestPoint = laBase[liVertex];
                    lbFound = true;
                }
            }

            // The three edges: the perpendicular offset from the edge line, accepted only when
            // the foot of the projection lies strictly inside the segment. `!(behind || proj >=
            // len)` is the console's vnor of the two tests, kept in that form so a NaN
            // projection rejects the edge exactly as it does there.
            for (s32 liEdge = 0; liEdge < 3; ++liEdge)
            {
                const f32 lfPerpU = laQ[liEdge].u - (lafProj[liEdge] * laDir[liEdge].u);
                const f32 lfPerpV = laQ[liEdge].v - (lafProj[liEdge] * laDir[liEdge].v);
                const f32 lfDistSq = (lfPerpU * lfPerpU) + (lfPerpV * lfPerpV);

                const bool lbOnSegment =
                    !(labBehind[liEdge] || (lafProj[liEdge] >= lafLen[liEdge]));

                if ((lfBest >= lfDistSq) && lbOnSegment)
                {
                    lfBest = lfDistSq;
                    lrClosestPoint.u = (lafProj[liEdge] * laDir[liEdge].u) + laBase[liEdge].u;
                    lrClosestPoint.v = (lafProj[liEdge] * laDir[liEdge].v) + laBase[liEdge].v;
                    lbFound = true;
                }
            }

            // 0x82839D18: an inside centre reports ITSELF as the closest point, overriding
            // whatever the cascade picked.
            if (lbInside)
            {
                lrClosestPoint = lCentre;
            }
            return lbFound || lbInside;
        }

        // -------------------------------------------------------------------------------------
        // CgsGeometric::SolveSweptCircleEquationSOA (DWARF CgsTriangleSphere.cpp:1958), inlined
        // by the console. The smaller root of `a t^2 + b t + c = 0`, plus the discriminant the
        // caller tests separately.
        //
        // ⚠️ THE ZERO-DISCRIMINANT GUARD IS LOAD-BEARING, not defensive: the console computes
        // the square root as `disc * rsqrt(disc)`, and rsqrt(0) is +inf, so 0 * inf would hand
        // back a NaN root for every exactly-tangential sweep. It selects a literal 0 in that
        // case (0x8283F480 `vsel(sqrt, 0, disc == 0)`), and so does this.
        // -------------------------------------------------------------------------------------
        f32 SolveSweptCircleEquation(f32 lfA, f32 lfB, f32 lfC, f32& lrfDiscriminant)
        {
            lrfDiscriminant = (lfB * lfB) - ((4.0f * lfA) * lfC);

            const f32 lfInvTwoA = 1.0f / (2.0f * lfA);              // vrefp + 2 NR
            const f32 lfRoot = (lrfDiscriminant == 0.0f)
                             ? 0.0f
                             : (lrfDiscriminant * (1.0f / std::sqrt(lrfDiscriminant))); // vrsqrtefp + 2 NR

            return SweptMin((-lfB + lfRoot) * lfInvTwoA, (-lfB - lfRoot) * lfInvTwoA);
        }
    }

    Triangle4::Mask4 IntersectTriangle4SweptSphere(
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
#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        // FLAG PC-platform leaf: original four-wide execution using SSE2 and the
        // established PC arithmetic. BRN_SWEPT_SPHERE_SIMD=0 selects the scalar
        // comparison path; platforms without SSE2 retain that path as well.
        static const bool sbSimd = [] {
            const char* lpValue = std::getenv("BRN_SWEPT_SPHERE_SIMD");
            return !lpValue || lpValue[0] != '0';
        }();
        if (sbSimd)
        {
            return SweptSpherePC::Run(lSweptSphere, lTriangles,
                lContactNormal0, lTriangleNormal0, lSphereContactPoint0, lTriangleContactPoint0,
                lContactNormal1, lTriangleNormal1, lSphereContactPoint1, lTriangleContactPoint1,
                lContactNormal2, lTriangleNormal2, lSphereContactPoint2, lTriangleContactPoint2,
                lContactNormal3, lTriangleNormal3, lSphereContactPoint3, lTriangleContactPoint3);
        }
#endif
        Vector3*     lapContactNormal[4]        = { &lContactNormal0, &lContactNormal1,
                                                    &lContactNormal2, &lContactNormal3 };
        Vector3*     lapTriangleNormal[4]       = { &lTriangleNormal0, &lTriangleNormal1,
                                                    &lTriangleNormal2, &lTriangleNormal3 };
        Vector3Plus* lapSphereContactPoint[4]   = { &lSphereContactPoint0, &lSphereContactPoint1,
                                                    &lSphereContactPoint2, &lSphereContactPoint3 };
        Vector3Plus* lapTriangleContactPoint[4] = { &lTriangleContactPoint0, &lTriangleContactPoint1,
                                                    &lTriangleContactPoint2, &lTriangleContactPoint3 };

        // 0x8283EF68..0x8283F000: the two packed lanes, then four vspltw each. The sweep is
        // stored as a DIRECTION plus a LENGTH and the console multiplies them here; the
        // direction is used as authored and is never re-normalised.
        const Vector3Plus lPositionAndRadius  = lSweptSphere.GetPositionAndRadius();
        const Vector3Plus lDirectionAndLength = lSweptSphere.GetDirectionAndLength();

        const f32 lfCx = lPositionAndRadius.x;
        const f32 lfCy = lPositionAndRadius.y;
        const f32 lfCz = lPositionAndRadius.z;
        const f32 lfRadius = lPositionAndRadius.w;

        const f32 lfDx = lDirectionAndLength.x * lDirectionAndLength.w;
        const f32 lfDy = lDirectionAndLength.y * lDirectionAndLength.w;
        const f32 lfDz = lDirectionAndLength.z * lDirectionAndLength.w;

        Triangle4::Mask4 lResult;
        lResult.SetZero();

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const f32 lfP0x = (&lTriangles.mVertex0X.x)[liLane];
            const f32 lfP0y = (&lTriangles.mVertex0Y.x)[liLane];
            const f32 lfP0z = (&lTriangles.mVertex0Z.x)[liLane];
            const f32 lfP1x = (&lTriangles.mVertex1X.x)[liLane];
            const f32 lfP1y = (&lTriangles.mVertex1Y.x)[liLane];
            const f32 lfP1z = (&lTriangles.mVertex1Z.x)[liLane];
            const f32 lfP2x = (&lTriangles.mVertex2X.x)[liLane];
            const f32 lfP2y = (&lTriangles.mVertex2Y.x)[liLane];
            const f32 lfP2z = (&lTriangles.mVertex2Z.x)[liLane];

            // ---- the face normal, from cross(P1-P0, P2-P1) (0x8283EFD4..0x8283F064) --------
            const f32 lfE1x = lfP1x - lfP0x, lfE1y = lfP1y - lfP0y, lfE1z = lfP1z - lfP0z;
            const f32 lfE2x = lfP2x - lfP1x, lfE2y = lfP2y - lfP1y, lfE2z = lfP2z - lfP1z;

            const f32 lfCrx = (lfE1y * lfE2z) - (lfE1z * lfE2y);
            const f32 lfCry = (lfE1z * lfE2x) - (lfE1x * lfE2z);
            const f32 lfCrz = (lfE1x * lfE2y) - (lfE1y * lfE2x);
            const f32 lfInvCr =
                1.0f / std::sqrt((lfCrx * lfCrx) + (lfCry * lfCry) + (lfCrz * lfCrz));
            const f32 lfNx = lfCrx * lfInvCr;
            const f32 lfNy = lfCry * lfInvCr;
            const f32 lfNz = lfCrz * lfInvCr;
            const f32 lfPlaneD = (lfNx * lfP0x) + (lfNy * lfP0y) + (lfNz * lfP0z);

            // ---- the in-plane frame: U along P1-P0, V = normalize(cross(U,N)) -------------
            const f32 lfInvE1 =
                1.0f / std::sqrt((lfE1x * lfE1x) + (lfE1y * lfE1y) + (lfE1z * lfE1z));
            const f32 lfUx = lfE1x * lfInvE1;
            const f32 lfUy = lfE1y * lfInvE1;
            const f32 lfUz = lfE1z * lfInvE1;

            f32 lfVx = (lfUy * lfNz) - (lfUz * lfNy);
            f32 lfVy = (lfUz * lfNx) - (lfUx * lfNz);
            f32 lfVz = (lfUx * lfNy) - (lfUy * lfNx);
            const f32 lfInvV =
                1.0f / std::sqrt((lfVx * lfVx) + (lfVy * lfVy) + (lfVz * lfVz));
            lfVx *= lfInvV;
            lfVy *= lfInvV;
            lfVz *= lfInvV;

            // ---- clip the sweep to the plane slab of half-thickness r ---------------------
            // 0x8283F148..0x8283F1E8. Note the reciprocal is of MINUS dot(D,N), so a sweep
            // heading into the front face produces positive times.
            const f32 lfDotDN = (lfDx * lfNx) + (lfDy * lfNy) + (lfDz * lfNz);
            const f32 lfH0 = ((lfCx * lfNx) + (lfCy * lfNy) + (lfCz * lfNz)) - lfPlaneD;

            const f32 lfInvNegDN = 1.0f / (-lfDotDN);           // vrefp + 2 NR
            const f32 lfTa = (lfH0 - lfRadius) * lfInvNegDN;
            const f32 lfTb = (lfH0 + lfRadius) * lfInvNegDN;

            const bool lbParallel = (lfDotDN == 0.0f);
            const f32 lfTExit  = lbParallel ? 1.0f : SweptMin(SweptMax(lfTa, lfTb), 1.0f);
            const f32 lfTEnter = lbParallel ? 0.0f : SweptMax(SweptMin(lfTa, lfTb), 0.0f);

            // ⚠️ |h0| is the console's vandc against the sign bit, i.e. plain fabs. A sweep
            // exactly parallel to the plane survives only while it is already inside the slab.
            const f32 lfAbsH0 = (lfH0 < 0.0f) ? -lfH0 : lfH0;
            const bool lbRejectSlab =
                  (lbParallel && (lfAbsH0 >= lfRadius))
               || (!lbParallel && ((0.0f >= lfTExit) || (lfTEnter >= 1.0f)));

            // ---- everything the tests need, expressed in the (U,V) frame with origin P0 ---
            const f32 lfW2x = lfP2x - lfP0x, lfW2y = lfP2y - lfP0y, lfW2z = lfP2z - lfP0z;
            const f32 lfRelx = lfCx - lfP0x, lfRely = lfCy - lfP0y, lfRelz = lfCz - lfP0z;

            const SweptPlanePoint2 lCentre2 = { (lfRelx * lfUx) + (lfRely * lfUy) + (lfRelz * lfUz),
                                                (lfRelx * lfVx) + (lfRely * lfVy) + (lfRelz * lfVz) };
            const SweptPlanePoint2 lSweep2  = { (lfDx * lfUx) + (lfDy * lfUy) + (lfDz * lfUz),
                                                (lfDx * lfVx) + (lfDy * lfVy) + (lfDz * lfVz) };

            const SweptPlanePoint2 laVertex[3] =
            {
                { 0.0f, 0.0f },
                { (lfE1x * lfUx) + (lfE1y * lfUy) + (lfE1z * lfUz),
                  (lfE1x * lfVx) + (lfE1y * lfVy) + (lfE1z * lfVz) },
                { (lfW2x * lfUx) + (lfW2y * lfUy) + (lfW2z * lfUz),
                  (lfW2x * lfVx) + (lfW2y * lfVy) + (lfW2z * lfVz) },
            };

            // The three edge directions and lengths. ⭐ The console gets the LENGTH out of the
            // same reciprocal square root it uses for the direction (`lenSq * rsqrt(lenSq)`),
            // and that shape is kept rather than calling sqrt twice.
            SweptPlanePoint2 laDir[3];
            f32 lafLen[3];
            for (s32 liEdge = 0; liEdge < 3; ++liEdge)
            {
                const SweptPlanePoint2& lrFrom = laVertex[liEdge];
                const SweptPlanePoint2& lrTo   = laVertex[(liEdge + 1) % 3];
                const f32 lfEu = lrTo.u - lrFrom.u;
                const f32 lfEv = lrTo.v - lrFrom.v;
                const f32 lfLenSq = (lfEu * lfEu) + (lfEv * lfEv);
                const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
                laDir[liEdge].u = lfEu * lfInvLen;
                laDir[liEdge].v = lfEv * lfInvLen;
                lafLen[liEdge]  = lfLenSq * lfInvLen;
            }

            // ---- the FACE test: the circle the sphere cuts in the plane at tEnter ---------
            // 0x8283F2E0..0x8283F3A0. r^2 - h(tEnter)^2 is that circle's squared radius; at a
            // genuine slab entry h == +/-r and it is 0, and it grows as the sphere sinks in.
            const f32 lfHEnter = (((lfCx + lfDx * lfTEnter) * lfNx)
                                + ((lfCy + lfDy * lfTEnter) * lfNy)
                                + ((lfCz + lfDz * lfTEnter) * lfNz)) - lfPlaneD;
            const f32 lfCircleRadiusSq = (lfRadius * lfRadius) - (lfHEnter * lfHEnter);

            const SweptPlanePoint2 lCircleCentre =
                { (lfTEnter * lSweep2.u) + lCentre2.u, (lfTEnter * lSweep2.v) + lCentre2.v };

            SweptPlanePoint2 lFacePoint = { 0.0f, 0.0f };
            const bool lbFaceHit =
                Intersect2DCircleWithTriangleSOA(lCircleCentre, lfCircleRadiusSq,
                                              laVertex[0], laVertex[1], laVertex[2],
                                              laDir[0], laDir[1], laDir[2],
                                              lafLen[0], lafLen[1], lafLen[2],
                                              lFacePoint);

            // ---- the three (vertex, edge) swept tests ------------------------------------
            // Intersect2DSweptCircleWithVertexAndEdgeSOA (DWARF :2024), inlined three times.
            // The quadratic is written in the (U,V,N) frame, which is orthonormal, so its
            // constant term carries the out-of-plane offset h0 and its leading term the
            // out-of-plane sweep component — i.e. these ARE the full 3D sweeps against the
            // vertex sphere and the edge cylinder, not a flattened 2D approximation.
            const f32 lfAFull = ((lSweep2.u * lSweep2.u) + (lSweep2.v * lSweep2.v))
                              + (lfDotDN * lfDotDN);               // == |D|^2
            const f32 lfCOffset = (lfH0 * lfH0) - (lfRadius * lfRadius);
            const f32 lfBOffset = (2.0f * lfH0) * lfDotDN;

            bool labPairHit[3];
            f32  lafPairTime[3];
            SweptPlanePoint2 laPairPoint[3];

            for (s32 liPair = 0; liPair < 3; ++liPair)
            {
                const f32 lfQu = lCentre2.u - laVertex[liPair].u;
                const f32 lfQv = lCentre2.v - laVertex[liPair].v;

                // --- the vertex sweep ---
                f32 lfDiscV = 0.0f;
                const f32 lfVertexTime = SolveSweptCircleEquation(
                    lfAFull,
                    (((lfQu * lSweep2.u) + (lfQv * lSweep2.v)) * 2.0f) + lfBOffset,
                    ((lfQu * lfQu) + (lfQv * lfQv)) + lfCOffset,
                    lfDiscV);
                // IsBetween0And1SOA (DWARF :1989) is the `(t >= 0) && (1 >= t)` pair.
                const bool lbVertexHit = (lfAFull != 0.0f) && (lfDiscV >= 0.0f)
                                      && (lfVertexTime >= 0.0f) && (1.0f >= lfVertexTime);

                // --- the edge sweep: the components perpendicular to the edge ---
                const f32 lfQAlong = (lfQu * laDir[liPair].u) + (lfQv * laDir[liPair].v);
                const f32 lfDAlong = (lSweep2.u * laDir[liPair].u) + (lSweep2.v * laDir[liPair].v);
                const f32 lfQpu = lfQu - (lfQAlong * laDir[liPair].u);
                const f32 lfQpv = lfQv - (lfQAlong * laDir[liPair].v);
                const f32 lfDpu = lSweep2.u - (lfDAlong * laDir[liPair].u);
                const f32 lfDpv = lSweep2.v - (lfDAlong * laDir[liPair].v);

                const f32 lfEdgeA = ((lfDpu * lfDpu) + (lfDpv * lfDpv)) + (lfDotDN * lfDotDN);
                f32 lfDiscE = 0.0f;
                const f32 lfEdgeTime = SolveSweptCircleEquation(
                    lfEdgeA,
                    (((lfQpu * lfDpu) + (lfQpv * lfDpv)) * 2.0f) + lfBOffset,
                    ((lfQpu * lfQpu) + (lfQpv * lfQpv)) + lfCOffset,
                    lfDiscE);

                // how far along the edge the contact lands, at that root
                const f32 lfAlong = (((lSweep2.u * lfEdgeTime) + lfQu) * laDir[liPair].u)
                                  + (((lSweep2.v * lfEdgeTime) + lfQv) * laDir[liPair].v);

                const bool lbEdgeHit = (lfEdgeA != 0.0f) && (lfDiscE >= 0.0f)
                                    && (lfEdgeTime >= 0.0f) && (1.0f >= lfEdgeTime)
                                    && (lfAlong >= 0.0f) && (lafLen[liPair] >= lfAlong);

                // The console carries a 2.0 sentinel for "this feature did not hit", which is
                // outside [0,1] and therefore loses every subsequent earliest-time compare.
                const f32 lfVertexT = lbVertexHit ? lfVertexTime : 2.0f;
                const f32 lfEdgeT   = lbEdgeHit   ? lfEdgeTime   : 2.0f;
                const bool lbUseVertex = (lfEdgeT >= lfVertexT);

                labPairHit[liPair]  = lbVertexHit || lbEdgeHit;
                lafPairTime[liPair] = labPairHit[liPair]
                                    ? (lbUseVertex ? lfVertexT : lfEdgeT)
                                    : 2.0f;
                laPairPoint[liPair].u = lbUseVertex
                                      ? laVertex[liPair].u
                                      : ((lfAlong * laDir[liPair].u) + laVertex[liPair].u);
                laPairPoint[liPair].v = lbUseVertex
                                      ? laVertex[liPair].v
                                      : ((lfAlong * laDir[liPair].v) + laVertex[liPair].v);
            }

            // The earliest of the three pairs (0x8283FAA0..0x8283FB14; the compare is `>=`, so
            // on an exact tie the LATER pair wins — reproduced in that order).
            f32 lfSweptTime = lafPairTime[0];
            SweptPlanePoint2 lSweptPoint = laPairPoint[0];
            for (s32 liPair = 1; liPair < 3; ++liPair)
            {
                if (lfSweptTime >= lafPairTime[liPair])
                {
                    lfSweptTime = lafPairTime[liPair];
                    lSweptPoint = laPairPoint[liPair];
                }
            }

            // ⭐ THE FACE RESULT WINS OUTRIGHT when it hits — it is not entered into the
            // earliest-time race (0x8283FB1C/0x8283FB28/0x8283FB3C select on the face mask
            // alone, after the three-way minimum has already been resolved).
            const f32 lfHitTime = lbFaceHit ? lfTEnter : lfSweptTime;
            const SweptPlanePoint2 lHitPoint = lbFaceHit ? lFacePoint : lSweptPoint;

            // ---- back to world space, and the two contact points -------------------------
            const f32 lfPx = ((lHitPoint.u * lfUx) + (lHitPoint.v * lfVx)) + lfP0x;
            const f32 lfPy = ((lHitPoint.u * lfUy) + (lHitPoint.v * lfVy)) + lfP0y;
            const f32 lfPz = ((lHitPoint.u * lfUz) + (lHitPoint.v * lfVz)) + lfP0z;

            // the face case: the direction from the centre to the contact point, and where
            // that direction meets the sphere's surface
            const f32 lfFx = lfPx - lfCx, lfFy = lfPy - lfCy, lfFz = lfPz - lfCz;
            const f32 lfInvF =
                1.0f / std::sqrt((lfFx * lfFx) + (lfFy * lfFy) + (lfFz * lfFz));
            const f32 lfFaceNx = lfFx * lfInvF;
            const f32 lfFaceNy = lfFy * lfInvF;
            const f32 lfFaceNz = lfFz * lfInvF;

            // the swept case: the contact point brought BACK into the sphere's start frame,
            // which is the frame the caller's spheres and the penetration solver live in
            const f32 lfBackX = lfPx - (lfDx * lfHitTime);
            const f32 lfBackY = lfPy - (lfDy * lfHitTime);
            const f32 lfBackZ = lfPz - (lfDz * lfHitTime);
            const f32 lfGx = lfBackX - lfCx, lfGy = lfBackY - lfCy, lfGz = lfBackZ - lfCz;
            const f32 lfInvG =
                1.0f / std::sqrt((lfGx * lfGx) + (lfGy * lfGy) + (lfGz * lfGz));
            const f32 lfSweptNx = lfGx * lfInvG;
            const f32 lfSweptNy = lfGy * lfInvG;
            const f32 lfSweptNz = lfGz * lfInvG;

            const f32 lfNormalX = lbFaceHit ? lfFaceNx : lfSweptNx;
            const f32 lfNormalY = lbFaceHit ? lfFaceNy : lfSweptNy;
            const f32 lfNormalZ = lbFaceHit ? lfFaceNz : lfSweptNz;

            // ---- outputs (written unconditionally, as the console's sixteen stvx128 are) --
            // ⭐ The final vmrghw/vmrglw transpose pairs each contact point's xyz with the
            // CONTACT TIME, so w carries t; and it pairs each normal's xyz with its own z, so
            // w duplicates z. Both are reproduced — the w lane of a Vector3Plus travels into
            // the queued PrimitiveTestResult and must not be left as whatever the caller's
            // record happened to hold.
            lapContactNormal[liLane]->x = lfNormalX;
            lapContactNormal[liLane]->y = lfNormalY;
            lapContactNormal[liLane]->z = lfNormalZ;
            lapContactNormal[liLane]->w = lfNormalZ;

            lapTriangleNormal[liLane]->x = lfNx;
            lapTriangleNormal[liLane]->y = lfNy;
            lapTriangleNormal[liLane]->z = lfNz;
            lapTriangleNormal[liLane]->w = lfNz;

            lapSphereContactPoint[liLane]->x = lbFaceHit ? (lfFaceNx * lfRadius) + lfCx : lfBackX;
            lapSphereContactPoint[liLane]->y = lbFaceHit ? (lfFaceNy * lfRadius) + lfCy : lfBackY;
            lapSphereContactPoint[liLane]->z = lbFaceHit ? (lfFaceNz * lfRadius) + lfCz : lfBackZ;
            lapSphereContactPoint[liLane]->w = lfHitTime;

            lapTriangleContactPoint[liLane]->x = lfPx;
            lapTriangleContactPoint[liLane]->y = lfPy;
            lapTriangleContactPoint[liLane]->z = lfPz;
            lapTriangleContactPoint[liLane]->w = lfHitTime;

            // ---- the hit mask (0x8283FCFC..0x8283FD3C) -----------------------------------
            // ⚠️ The single-sided test is on the CONTACT normal against the FACE normal, and it
            // is strict: a contact direction perpendicular to the face is rejected.
            const f32 lfNormalAgainstFace =
                (lfNormalX * lfNx) + (lfNormalY * lfNy) + (lfNormalZ * lfNz);

            const bool lbAccept =
                   !lbRejectSlab
                && (lbFaceHit || labPairHit[0] || labPairHit[1] || labPairHit[2])
                && (0.0f > lfNormalAgainstFace);

            u32 luEnableBits = 0;
            std::memcpy(&luEnableBits, &(&lTriangles.mValidMasks.x)[liLane], sizeof(u32));
            const u32 luLaneMask = (lbAccept ? 0xFFFFFFFFu : 0u) & luEnableBits;
            std::memcpy(&(&lResult.x)[liLane], &luLaneMask, sizeof(u32));
        }

        return lResult;
    }
}

// =============================================================================================
// The SINGLE-triangle swept kernel:
//   CgsGeometric::IntersectTriangleSweptSphere   (1171 instructions; the last 780 follow its twelve IsValid asserts)
//   CgsGeometric::Intersect2DCircleWithTriangle  (150, straight-line VMX)
// One caller: TriangleCollisionDebugComponent::TestPolySoupSweptSphereCollision.
//
// The algorithm is the four-wide kernel's (the banner above), written per triangle in AoS
// form, and the instruction stream decides every difference from that kernel:
//   * the 2D circle test DOES take vertex C as a candidate (the SOA function above does not),
//     and its compares are `!(d > best)`;
//   * no facing test on the contact normal: the result is the slab clip and the hit masks;
//   * the edge sweeps keep the vertex unless it is LATER than the edge (`!(v > e)`), and of
//     the three pairs a later one wins a tie;
//   * the triangle normal output is the unit normal on all four lanes; the contact normal is
//     computed from the contact point carrying the w lane the caller's TriangleContactPoint
//     held on entry (the console writes the point back with that w, then derives both
//     normals from the stored vector, and only then puts the contact time in the w lanes).
// Local names are the declaration's; the inlined helpers SolveSweptCircleEquation,
// IsBetween0And1, Intersect2DSweptCircleWithVertexAndEdge, Transform3DTo2D /
// Transform3DVectorTo2D and NormalizeReturnMagnitude are file-local functions below.
//
// HOW IT WAS CHECKED: every VMX128 operand was read from its raw instruction word (39 printed
// vA fields corrected, the fused forms' operand roles per their encodings), the corrected
// stream was executed on float32 lanes by a small interpreter, and this lowering was compared
// with it on 3,500 random sweeps (aimed, planar, parallel to the plane, exactly parallel,
// zero length, collinear triangles, non-zero w lanes): no hit-mask differences; the only value
// differences are rejected sweeps parallel to the plane to within a float ulp, where the
// slab times divide by a dot product of ~1e-8 whose sign the rounding decides.
// PC LOWERING, as for the four-wide kernel: vrefp / vrsqrtefp with their Newton-Raphson
// steps are a divide / 1/sqrt, the fused multiply-adds a multiply then an add, EXCEPT the
// cross products, whose fused subtract is std::fma (it decides the normal of a nearly
// degenerate triangle).
// =============================================================================================

namespace CgsGeometric
{
    namespace
    {
        // vminfp / vmaxfp: a NaN operand gives a NaN (the first operand's when both are), and
        // equal operands of opposite sign order -0 below +0.
        inline f32 SweptVmxMin(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;
            return (lfA < lfB) ? lfA : lfB;
        }
        inline f32 SweptVmxMax(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            if (lfA == lfB) return std::signbit(lfA) ? lfB : lfA;
            return (lfA > lfB) ? lfA : lfB;
        }

        inline rw::math::vpu::MaskScalar SweptMask(bool lbValue)
        {
            rw::math::vpu::MaskScalar lMask;
            const u32 luLane = lbValue ? 0xFFFFFFFFu : 0u;
            std::memcpy(&lMask.x, &luLane, sizeof(u32));
            std::memcpy(&lMask.y, &luLane, sizeof(u32));
            std::memcpy(&lMask.z, &luLane, sizeof(u32));
            std::memcpy(&lMask.w, &luLane, sizeof(u32));
            return lMask;
        }

        // A 2D point as the console builds it: `vperm` of two splatted dot products with the
        // (A.w0, B.w1, A.w0, A.w0) control vector, so z and w repeat x.
        inline Vector2 SweptPlanePoint(f32 lfX, f32 lfY)
        {
            Vector2 lPoint;
            lPoint.x = lfX;
            lPoint.y = lfY;
            lPoint.z = lfX;
            lPoint.w = lfX;
            return lPoint;
        }

        // The x+y dot of two 2D vectors (vmulfp128, then splat x + splat y).
        inline f32 SweptDot2(const Vector2& lrA, const Vector2& lrB)
        {
            return (lrA.x * lrB.x) + (lrA.y * lrB.y);
        }

        inline f32 SweptDot3(const Vector3& lrA, const Vector3& lrB)
        {
            return ((lrA.x * lrB.x) + (lrA.y * lrB.y)) + (lrA.z * lrB.z);
        }

        // Normalize on all four lanes: vmsum3fp128 + vrsqrtefp + two Newton-Raphson steps.
        inline Vector3 SweptNormalize(const Vector3& lrV)
        {
            const f32 lfInverseLength = 1.0f / std::sqrt(SweptDot3(lrV, lrV));
            Vector3 lResult;
            lResult.x = lrV.x * lfInverseLength;
            lResult.y = lrV.y * lfInverseLength;
            lResult.z = lrV.z * lfInverseLength;
            lResult.w = lrV.w * lfInverseLength;
            return lResult;
        }

        // a x b by the yzx-permute idiom, on all four lanes (the permute keeps w in w): one
        // rounded product (vmulfp128), then the other subtracted FUSED (vnmsubfp), which is
        // std::fma. The fused form is kept because it decides the normal of a nearly
        // degenerate triangle, where the two products cancel.
        inline Vector3 SweptCross(const Vector3& lrA, const Vector3& lrB)
        {
            Vector3 lResult;
            lResult.x = std::fma(-lrA.z, lrB.y, lrA.y * lrB.z);
            lResult.y = std::fma(-lrA.x, lrB.z, lrA.z * lrB.x);
            lResult.z = std::fma(-lrA.y, lrB.x, lrA.x * lrB.y);
            lResult.w = std::fma(-lrA.w, lrB.w, lrA.w * lrB.w);
            return lResult;
        }

        // Transform3DTo2D / Transform3DVectorTo2D (the declaration's names; both inlined).
        inline Vector2 SweptTransform3DTo2D(const Vector3& lrBasisOrigin, const Vector3& lrBasisX,
                                            const Vector3& lrBasisY, const Vector3& lrPoint)
        {
            Vector3 lOffset;
            lOffset.x = lrPoint.x - lrBasisOrigin.x;
            lOffset.y = lrPoint.y - lrBasisOrigin.y;
            lOffset.z = lrPoint.z - lrBasisOrigin.z;
            lOffset.w = lrPoint.w - lrBasisOrigin.w;
            return SweptPlanePoint(SweptDot3(lOffset, lrBasisX), SweptDot3(lOffset, lrBasisY));
        }
        inline Vector2 SweptTransform3DVectorTo2D(const Vector3& lrBasisX, const Vector3& lrBasisY,
                                                  const Vector3& lrVector)
        {
            return SweptPlanePoint(SweptDot3(lrVector, lrBasisX), SweptDot3(lrVector, lrBasisY));
        }

        // NormalizeReturnMagnitude(Vector2): direction = v * rsqrt(|v|^2) on all lanes (no zero
        // guard), magnitude = |v|^2 * rsqrt(|v|^2), selected to 0 when |v|^2 == 0.
        inline f32 SweptNormalizeReturnMagnitude(const Vector2& lrV, Vector2& lrDirection)
        {
            const f32 lfLengthSq      = SweptDot2(lrV, lrV);
            const f32 lfInverseLength = 1.0f / std::sqrt(lfLengthSq);
            lrDirection.x = lrV.x * lfInverseLength;
            lrDirection.y = lrV.y * lfInverseLength;
            lrDirection.z = lrV.z * lfInverseLength;
            lrDirection.w = lrV.w * lfInverseLength;
            return (lfLengthSq == 0.0f) ? 0.0f : (lfLengthSq * lfInverseLength);
        }

        // SolveSweptCircleEquation: a t^2 + b t + c = 0 with
        // a = |V|^2 + R1, b = 2 (P.V) + R2, c = |P|^2 + R3. Returns the smaller root
        // (`vminfp` of the + root and the - root, in that order) and reports a and the
        // discriminant for the caller's validity mask. The square root is disc * rsqrt(disc),
        // selected to 0 when disc == 0.
        inline f32 SweptSolveCircleEquation(const Vector2& lrP, const Vector2& lrV,
                                            f32 lfR1, f32 lfR2, f32 lfR3,
                                            f32& lrfA, f32& lrfDiscriminant)
        {
            const f32 lfA = SweptDot2(lrV, lrV) + lfR1;
            const f32 lfB = (2.0f * SweptDot2(lrP, lrV)) + lfR2;
            const f32 lfC = SweptDot2(lrP, lrP) + lfR3;

            lrfA            = lfA;
            lrfDiscriminant = (lfB * lfB) - ((4.0f * lfA) * lfC);

            const f32 lfInverseTwoA = 1.0f / (2.0f * lfA);
            const f32 lfRoot = (lrfDiscriminant == 0.0f)
                             ? 0.0f
                             : (lrfDiscriminant * (1.0f / std::sqrt(lrfDiscriminant)));
            const f32 lfMinusB = -1.0f * lfB;
            return SweptVmxMin((lfMinusB + lfRoot) * lfInverseTwoA, (lfMinusB - lfRoot) * lfInverseTwoA);
        }

        // IsBetween0And1: (t >= 0) & !(t > 1).
        inline bool SweptIsBetween0And1(f32 lfValue)
        {
            return (lfValue >= 0.0f) && !(lfValue > 1.0f);
        }

        // Intersect2DSweptCircleWithVertexAndEdge, inlined three times. The vertex sweep solves for P = circle - vertex; the edge sweep for
        // the components of P and of the translation perpendicular to the edge, and is kept only
        // when the contact's projection on the edge lies in [0, |edge|]. Each solution is carried
        // as 2.0 when it is not valid; the vertex wins unless it is later than the edge.
        // Returns lGotVertexSolution | lGotEdgeSolution.
        inline bool SweptCircleWithVertexAndEdge(const Vector2& lrCirclePos, const Vector2& lrCircleTranslation,
                                                 f32 lfR1, f32 lfR2, f32 lfR3,
                                                 const Vector2& lrVertex, const Vector2& lrEdgeDir,
                                                 f32 lfEdgeMagnitude, f32& lrfOutT, Vector2& lrOutPosition)
        {
            const f32 KF_NO_SOLUTION = 2.0f;

            Vector2 lOffset;
            lOffset.x = lrCirclePos.x - lrVertex.x;
            lOffset.y = lrCirclePos.y - lrVertex.y;
            lOffset.z = lrCirclePos.z - lrVertex.z;
            lOffset.w = lrCirclePos.w - lrVertex.w;

            f32 lfVertexA = 0.0f, lfVertexDiscriminant = 0.0f;
            const f32 lfVertexT = SweptSolveCircleEquation(lOffset, lrCircleTranslation, lfR1, lfR2, lfR3,
                                                           lfVertexA, lfVertexDiscriminant);
            const bool lbGotVertexSolution = !(lfVertexA == 0.0f) && (lfVertexDiscriminant >= 0.0f)
                                          && SweptIsBetween0And1(lfVertexT);

            const f32 lfOffsetAlong      = SweptDot2(lOffset, lrEdgeDir);
            const f32 lfTranslationAlong = SweptDot2(lrCircleTranslation, lrEdgeDir);
            Vector2 lOffsetPerp, lTranslationPerp;
            lOffsetPerp.x      = lOffset.x - (lrEdgeDir.x * lfOffsetAlong);
            lOffsetPerp.y      = lOffset.y - (lrEdgeDir.y * lfOffsetAlong);
            lOffsetPerp.z      = lOffset.z - (lrEdgeDir.z * lfOffsetAlong);
            lOffsetPerp.w      = lOffset.w - (lrEdgeDir.w * lfOffsetAlong);
            lTranslationPerp.x = lrCircleTranslation.x - (lrEdgeDir.x * lfTranslationAlong);
            lTranslationPerp.y = lrCircleTranslation.y - (lrEdgeDir.y * lfTranslationAlong);
            lTranslationPerp.z = lrCircleTranslation.z - (lrEdgeDir.z * lfTranslationAlong);
            lTranslationPerp.w = lrCircleTranslation.w - (lrEdgeDir.w * lfTranslationAlong);

            f32 lfEdgeA = 0.0f, lfEdgeDiscriminant = 0.0f;
            const f32 lfEdgeT = SweptSolveCircleEquation(lOffsetPerp, lTranslationPerp, lfR1, lfR2, lfR3,
                                                         lfEdgeA, lfEdgeDiscriminant);

            Vector2 lEdgeSolutionOffset;
            lEdgeSolutionOffset.x = (lrCircleTranslation.x * lfEdgeT) + lOffset.x;
            lEdgeSolutionOffset.y = (lrCircleTranslation.y * lfEdgeT) + lOffset.y;
            lEdgeSolutionOffset.z = (lrCircleTranslation.z * lfEdgeT) + lOffset.z;
            lEdgeSolutionOffset.w = (lrCircleTranslation.w * lfEdgeT) + lOffset.w;
            const f32 lfEdgeSolutionParam = SweptDot2(lEdgeSolutionOffset, lrEdgeDir);

            const bool lbEdgeSolutionIR = (lfEdgeSolutionParam >= 0.0f) && !(lfEdgeSolutionParam > lfEdgeMagnitude);
            const bool lbGotEdgeSolution = !(lfEdgeA == 0.0f) && (lfEdgeDiscriminant >= 0.0f)
                                        && SweptIsBetween0And1(lfEdgeT) && lbEdgeSolutionIR;

            const f32 lfVertexSolution = lbGotVertexSolution ? lfVertexT : KF_NO_SOLUTION;
            const f32 lfEdgeSolution   = lbGotEdgeSolution   ? lfEdgeT   : KF_NO_SOLUTION;
            const bool lbPickVertex    = !(lfVertexSolution > lfEdgeSolution);

            lrfOutT = lbPickVertex ? lfVertexSolution : lfEdgeSolution;
            if (lbPickVertex)
            {
                lrOutPosition = lrVertex;
            }
            else
            {
                lrOutPosition.x = (lrEdgeDir.x * lfEdgeSolutionParam) + lrVertex.x;
                lrOutPosition.y = (lrEdgeDir.y * lfEdgeSolutionParam) + lrVertex.y;
                lrOutPosition.z = (lrEdgeDir.z * lfEdgeSolutionParam) + lrVertex.z;
                lrOutPosition.w = (lrEdgeDir.w * lfEdgeSolutionParam) + lrVertex.w;
            }
            return lbGotVertexSolution || lbGotEdgeSolution;
        }
    }

    rw::math::vpu::MaskScalar Intersect2DCircleWithTriangle(const Vector2&  lCentre,
                                                            const VecFloat& lRadiusSquared,
                                                            const Vector2&  lVert0,
                                                            const Vector2&  lVert1,
                                                            const Vector2&  lVert2,
                                                            const Vector2&  lEdge01Dir,
                                                            const Vector2&  lEdge12Dir,
                                                            const Vector2&  lEdge20Dir,
                                                            const VecFloat& lfEdge01Magnitude,
                                                            const VecFloat& lfEdge12Magnitude,
                                                            const VecFloat& lfEdge20Magnitude,
                                                            Vector2&        lIntersectPoint)
    {
        const Vector2* lapVertex[3]  = { &lVert0, &lVert1, &lVert2 };
        const Vector2* lapEdgeDir[3] = { &lEdge01Dir, &lEdge12Dir, &lEdge20Dir };
        const f32      lafEdgeMagnitude[3] = { lfEdge01Magnitude.x, lfEdge12Magnitude.x, lfEdge20Magnitude.x };

        bool lbInside = true;
        f32  lafProjection[3];
        bool labBehind[3];
        Vector2 laOffset[3];
        for (s32 liEdge = 0; liEdge < 3; ++liEdge)
        {
            const Vector2& lrVertex = *lapVertex[liEdge];
            const Vector2& lrDir    = *lapEdgeDir[liEdge];

            // The winding test: the centre lies left of every directed edge.
            const f32 lfSide = (lrDir.x * (lrVertex.y - lCentre.y)) - (lrDir.y * (lrVertex.x - lCentre.x));
            lbInside = lbInside && (lfSide >= 0.0f);

            laOffset[liEdge].x = lCentre.x - lrVertex.x;
            laOffset[liEdge].y = lCentre.y - lrVertex.y;
            laOffset[liEdge].z = lCentre.z - lrVertex.z;
            laOffset[liEdge].w = lCentre.w - lrVertex.w;
            lafProjection[liEdge] = SweptDot2(laOffset[liEdge], lrDir);
            labBehind[liEdge]     = !(lafProjection[liEdge] > 0.0f);
        }

        // Candidates in the console's order -- the three vertices, then the three edges -- each
        // taken when it is not farther than the best so far (`!(d > best)`, the first best being
        // the squared radius) and its own gate holds.
        f32     lfBest  = lRadiusSquared.x;
        bool    lbFound = false;
        Vector2 lPoint  = lCentre;

        for (s32 liVertex = 0; liVertex < 3; ++liVertex)
        {
            const f32 lfDistanceSq = SweptDot2(laOffset[liVertex], laOffset[liVertex]);
            if (!(lfDistanceSq > lfBest) && labBehind[liVertex])
            {
                lfBest  = lfDistanceSq;
                lPoint  = *lapVertex[liVertex];
                lbFound = true;
            }
        }

        for (s32 liEdge = 0; liEdge < 3; ++liEdge)
        {
            const Vector2& lrVertex = *lapVertex[liEdge];
            const Vector2& lrDir    = *lapEdgeDir[liEdge];
            const f32      lfProj   = lafProjection[liEdge];

            Vector2 lPerp;
            lPerp.x = laOffset[liEdge].x - (lrDir.x * lfProj);
            lPerp.y = laOffset[liEdge].y - (lrDir.y * lfProj);
            const f32 lfDistanceSq = SweptDot2(lPerp, lPerp);

            const bool lbOnSegment = !(labBehind[liEdge] || (lfProj >= lafEdgeMagnitude[liEdge]));
            if (!(lfDistanceSq > lfBest) && lbOnSegment)
            {
                lfBest   = lfDistanceSq;
                lPoint.x = (lrDir.x * lfProj) + lrVertex.x;
                lPoint.y = (lrDir.y * lfProj) + lrVertex.y;
                lPoint.z = (lrDir.z * lfProj) + lrVertex.z;
                lPoint.w = (lrDir.w * lfProj) + lrVertex.w;
                lbFound  = true;
            }
        }

        if (lbInside)
        {
            lPoint = lCentre;
        }
        lIntersectPoint = lPoint;
        return SweptMask(lbFound || lbInside);
    }

    const rw::math::vpu::MaskScalar IntersectTriangleSweptSphere(const SweptSphere& lSphere,
                                                                 const Triangle&    lTriangle,
                                                                 Vector3&           lOutContactNormal,
                                                                 Vector3&           lOutTriangleNormal,
                                                                 Vector3Plus&       lOutSphereContactPoint,
                                                                 Vector3Plus&       lOutTriangleContactPoint)
    {
        const f32 KF_NO_SOLUTION = 2.0f;

        // ---- unpack ----------------------------------------------------------------------------
        const Vector3 lTriVert0 = lTriangle.Get(0);
        const Vector3 lTriVert1 = lTriangle.Get(1);
        const Vector3 lTriVert2 = lTriangle.Get(2);

        const Vector3Plus lPositionAndRadius  = lSphere.GetPositionAndRadius();
        const Vector3Plus lDirectionAndLength = lSphere.GetDirectionAndLength();
        const Vector3     lSpherePos          = { lPositionAndRadius.x, lPositionAndRadius.y,
                                                  lPositionAndRadius.z, lPositionAndRadius.w };
        const Vector3     lSphereDir          = { lDirectionAndLength.x, lDirectionAndLength.y,
                                                  lDirectionAndLength.z, lDirectionAndLength.w };
        const f32         lfSphereRad         = lPositionAndRadius.w;
        const f32         lfSphereLength      = lDirectionAndLength.w;
        Vector3 lSphereTranslation;
        lSphereTranslation.x = lDirectionAndLength.x * lfSphereLength;
        lSphereTranslation.y = lDirectionAndLength.y * lfSphereLength;
        lSphereTranslation.z = lDirectionAndLength.z * lfSphereLength;
        lSphereTranslation.w = lDirectionAndLength.w * lfSphereLength;

        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert0), "IsValid(lTriVert0)");
        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert1), "IsValid(lTriVert1)");
        CGS_ASSERT(rw::math::vpu::IsValid(lTriVert2), "IsValid(lTriVert2)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSpherePos), "IsValid(lSpherePos)");
        CGS_ASSERT(lfSphereRad == lfSphereRad, "IsValid(lSphereRad)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSphereDir), "IsValid(lSphereDir)");
        CGS_ASSERT(lfSphereLength == lfSphereLength, "IsValid(lSphereLength)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSphereTranslation), "IsValid(lSphereTranslation)");

        // ---- the triangle's plane and the in-plane basis ----------------------------------------
        const Vector3 lEdge01 = lTriangle.CalcEdge01();
        const Vector3 lEdge12 = lTriangle.CalcEdge12();
        CGS_ASSERT(rw::math::vpu::IsValid(lEdge01), "IsValid(lEdge01)");
        CGS_ASSERT(rw::math::vpu::IsValid(lEdge12), "IsValid(lEdge12)");

        const Vector3 lTriangleNormal = SweptNormalize(SweptCross(lEdge01, lEdge12));
        CGS_ASSERT(rw::math::vpu::IsValid(lTriangleNormal), "IsValid(lTriangleNormal)");

        const f32 lfPlaneOffset = SweptDot3(lTriangleNormal, lTriVert0);
        CGS_ASSERT(lfPlaneOffset == lfPlaneOffset, "IsValid(lfPlaneOffset)");

        const Vector3& lBasisOrigin = lTriVert0;
        const Vector3  lBasisX      = SweptNormalize(lEdge01);
        const Vector3  lBasisY      = SweptNormalize(SweptCross(lBasisX, lTriangleNormal));

        // ---- clip the sweep to the slab of half-thickness r about the plane ---------------------
        const f32 lfProjectTranslationOntoPlaneNormal = SweptDot3(lSphereTranslation, lTriangleNormal);
        const f32 lfPosPlaneOffset = SweptDot3(lSpherePos, lTriangleNormal) - lfPlaneOffset;

        // Both times divide by MINUS the translation's normal component (the `vxor` with the sign
        // splat, then `vrefp` + two Newton-Raphson steps).
        const f32 lfInverseApproach = 1.0f / (-lfProjectTranslationOntoPlaneNormal);
        const f32 lfFaceCollisionT0 = lfInverseApproach * (lfPosPlaneOffset - lfSphereRad);
        const f32 lfFaceCollisionT1 = lfInverseApproach * (lfPosPlaneOffset + lfSphereRad);

        const bool lbMovingParallel = (lfProjectTranslationOntoPlaneNormal == 0.0f);
        const f32  lfT0 = lbMovingParallel ? 0.0f
                                           : SweptVmxMax(SweptVmxMin(lfFaceCollisionT0, lfFaceCollisionT1), 0.0f);
        const f32  lfT1 = lbMovingParallel ? 1.0f
                                           : SweptVmxMin(SweptVmxMax(lfFaceCollisionT0, lfFaceCollisionT1), 1.0f);

        const f32  lfAbsPosPlaneOffset = std::fabs(lfPosPlaneOffset);
        const bool lbFailedParallel    = (lfAbsPosPlaneOffset >= lfSphereRad) && lbMovingParallel;
        const bool lbFailedNoneParallel = ((lfT0 >= 1.0f) || !(lfT1 > 0.0f)) && !lbMovingParallel;
        const bool lbFailed            = lbFailedParallel || lbFailedNoneParallel;

        // ---- everything in the plane's 2D frame, origin at vertex 0 -----------------------------
        const Vector2 l2DTriangleV0 = SweptPlanePoint(0.0f, 0.0f);
        const Vector2 l2DTriangleV1 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lTriVert1);
        const Vector2 l2DTriangleV2 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lTriVert2);
        const Vector2 l2DCirclePos  = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lSpherePos);
        const Vector2 l2DCircleTranslation = SweptTransform3DVectorTo2D(lBasisX, lBasisY, lSphereTranslation);

        Vector2 l2DEdge01, l2DEdge12, l2DEdge20;
        l2DEdge01.x = l2DTriangleV1.x - l2DTriangleV0.x;  l2DEdge01.y = l2DTriangleV1.y - l2DTriangleV0.y;
        l2DEdge01.z = l2DTriangleV1.z - l2DTriangleV0.z;  l2DEdge01.w = l2DTriangleV1.w - l2DTriangleV0.w;
        l2DEdge12.x = l2DTriangleV2.x - l2DTriangleV1.x;  l2DEdge12.y = l2DTriangleV2.y - l2DTriangleV1.y;
        l2DEdge12.z = l2DTriangleV2.z - l2DTriangleV1.z;  l2DEdge12.w = l2DTriangleV2.w - l2DTriangleV1.w;
        l2DEdge20.x = l2DTriangleV0.x - l2DTriangleV2.x;  l2DEdge20.y = l2DTriangleV0.y - l2DTriangleV2.y;
        l2DEdge20.z = l2DTriangleV0.z - l2DTriangleV2.z;  l2DEdge20.w = l2DTriangleV0.w - l2DTriangleV2.w;

        Vector2 l2DEdge01Dir, l2DEdge12Dir, l2DEdge20Dir;
        const f32 lfEdge01Magnitude = SweptNormalizeReturnMagnitude(l2DEdge01, l2DEdge01Dir);
        const f32 lfEdge12Magnitude = SweptNormalizeReturnMagnitude(l2DEdge12, l2DEdge12Dir);
        const f32 lfEdge20Magnitude = SweptNormalizeReturnMagnitude(l2DEdge20, l2DEdge20Dir);

        // ---- the FACE test at the slab entry time --------------------------------------------------
        Vector3 lSpherePosAtT0;
        lSpherePosAtT0.x = (lSphereTranslation.x * lfT0) + lPositionAndRadius.x;
        lSpherePosAtT0.y = (lSphereTranslation.y * lfT0) + lPositionAndRadius.y;
        lSpherePosAtT0.z = (lSphereTranslation.z * lfT0) + lPositionAndRadius.z;
        lSpherePosAtT0.w = (lSphereTranslation.w * lfT0) + lPositionAndRadius.w;

        const f32 lfHeightAtT0 = SweptDot3(lSpherePosAtT0, lTriangleNormal) - lfPlaneOffset;
        VecFloat lRadiusSquaredAtT0;
        lRadiusSquaredAtT0.x = (lfSphereRad * lfSphereRad) - (lfHeightAtT0 * lfHeightAtT0);
        lRadiusSquaredAtT0.y = lRadiusSquaredAtT0.x;
        lRadiusSquaredAtT0.z = lRadiusSquaredAtT0.x;
        lRadiusSquaredAtT0.w = lRadiusSquaredAtT0.x;

        const Vector2 l2DCirclePosAtT0 = SweptTransform3DTo2D(lBasisOrigin, lBasisX, lBasisY, lSpherePosAtT0);

        VecFloat lEdge01Magnitude, lEdge12Magnitude, lEdge20Magnitude;
        lEdge01Magnitude.x = lEdge01Magnitude.y = lEdge01Magnitude.z = lEdge01Magnitude.w = lfEdge01Magnitude;
        lEdge12Magnitude.x = lEdge12Magnitude.y = lEdge12Magnitude.z = lEdge12Magnitude.w = lfEdge12Magnitude;
        lEdge20Magnitude.x = lEdge20Magnitude.y = lEdge20Magnitude.z = lEdge20Magnitude.w = lfEdge20Magnitude;

        Vector2 l2DResultAtT0;
        const bool lbResultValidAtT0 = Intersect2DCircleWithTriangle(
            l2DCirclePosAtT0, lRadiusSquaredAtT0,
            l2DTriangleV0, l2DTriangleV1, l2DTriangleV2,
            l2DEdge01Dir, l2DEdge12Dir, l2DEdge20Dir,
            lEdge01Magnitude, lEdge12Magnitude, lEdge20Magnitude,
            l2DResultAtT0).GetBool();

        // ---- the three (vertex, edge) sweeps ------------------------------------------------------
        const f32 lfVDotN       = lfProjectTranslationOntoPlaneNormal;
        const f32 lfPDotNMinusK = lfPosPlaneOffset;
        const f32 lfR1 = lfVDotN * lfVDotN;
        const f32 lfR2 = (2.0f * lfPDotNMinusK) * lfVDotN;
        const f32 lfR3 = (lfPDotNMinusK * lfPDotNMinusK) - (lfSphereRad * lfSphereRad);

        f32 lfEdge01T, lfEdge12T, lfEdge20T;
        Vector2 lEdge01P, lEdge12P, lEdge20P;
        const bool lbEdge01Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV0, l2DEdge01Dir, lfEdge01Magnitude,
                                                               lfEdge01T, lEdge01P);
        const bool lbEdge12Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV1, l2DEdge12Dir, lfEdge12Magnitude,
                                                               lfEdge12T, lEdge12P);
        const bool lbEdge20Mask = SweptCircleWithVertexAndEdge(l2DCirclePos, l2DCircleTranslation, lfR1, lfR2, lfR3,
                                                               l2DTriangleV2, l2DEdge20Dir, lfEdge20Magnitude,
                                                               lfEdge20T, lEdge20P);

        // The earliest of the three; a later pair wins a tie (`!(later > earlier)`).
        const f32 lfMaskedEdge01T = lbEdge01Mask ? lfEdge01T : KF_NO_SOLUTION;
        const f32 lfMaskedEdge12T = lbEdge12Mask ? lfEdge12T : KF_NO_SOLUTION;
        const f32 lfMaskedEdge20T = lbEdge20Mask ? lfEdge20T : KF_NO_SOLUTION;

        const bool lPickEdge12 = !(lfMaskedEdge12T > lfMaskedEdge01T);
        f32     lfEdgeResultT = lPickEdge12 ? lfMaskedEdge12T : lfMaskedEdge01T;
        Vector2 lEdgeResultP  = lPickEdge12 ? lEdge12P : lEdge01P;

        const bool lPickEdge20 = !(lfMaskedEdge20T > lfEdgeResultT);
        lfEdgeResultT = lPickEdge20 ? lfMaskedEdge20T : lfEdgeResultT;
        lEdgeResultP  = lPickEdge20 ? lEdge20P : lEdgeResultP;

        // The face contact wins outright when it is valid.
        const f32     lfResultT = lbResultValidAtT0 ? lfT0 : lfEdgeResultT;
        const Vector2 lResultP  = lbResultValidAtT0 ? l2DResultAtT0 : lEdgeResultP;

        const bool lPassed = !lbFailed && (lbResultValidAtT0 || lbEdge01Mask || lbEdge12Mask || lbEdge20Mask);

        // ---- outputs -------------------------------------------------------------------------------
        lOutTriangleNormal = lTriangleNormal;

        // Transform2DTo3D: (BasisX * p.x + BasisY * p.y) + origin. The console stores it with the
        // w lane the caller's TriangleContactPoint already held, and computes both normals from
        // that vector before the w lane is replaced by the contact time.
        Vector3 lTriangleContactPoint;
        lTriangleContactPoint.x = ((lBasisX.x * lResultP.x) + (lBasisY.x * lResultP.y)) + lBasisOrigin.x;
        lTriangleContactPoint.y = ((lBasisX.y * lResultP.x) + (lBasisY.y * lResultP.y)) + lBasisOrigin.y;
        lTriangleContactPoint.z = ((lBasisX.z * lResultP.x) + (lBasisY.z * lResultP.y)) + lBasisOrigin.z;
        lTriangleContactPoint.w = lOutTriangleContactPoint.w;

        Vector3 lToContact;
        lToContact.x = lTriangleContactPoint.x - lPositionAndRadius.x;
        lToContact.y = lTriangleContactPoint.y - lPositionAndRadius.y;
        lToContact.z = lTriangleContactPoint.z - lPositionAndRadius.z;
        lToContact.w = lTriangleContactPoint.w - lPositionAndRadius.w;
        const Vector3 lSphereNormalIfPenetration = SweptNormalize(lToContact);

        Vector3 lSphereContactPointIfPenetration;
        lSphereContactPointIfPenetration.x = (lSphereNormalIfPenetration.x * lfSphereRad) + lPositionAndRadius.x;
        lSphereContactPointIfPenetration.y = (lSphereNormalIfPenetration.y * lfSphereRad) + lPositionAndRadius.y;
        lSphereContactPointIfPenetration.z = (lSphereNormalIfPenetration.z * lfSphereRad) + lPositionAndRadius.z;

        Vector3 lSphereContactPointIfFuture;
        lSphereContactPointIfFuture.x = lTriangleContactPoint.x - (lSphereTranslation.x * lfResultT);
        lSphereContactPointIfFuture.y = lTriangleContactPoint.y - (lSphereTranslation.y * lfResultT);
        lSphereContactPointIfFuture.z = lTriangleContactPoint.z - (lSphereTranslation.z * lfResultT);
        lSphereContactPointIfFuture.w = lTriangleContactPoint.w - (lSphereTranslation.w * lfResultT);

        Vector3 lToFuture;
        lToFuture.x = lSphereContactPointIfFuture.x - lPositionAndRadius.x;
        lToFuture.y = lSphereContactPointIfFuture.y - lPositionAndRadius.y;
        lToFuture.z = lSphereContactPointIfFuture.z - lPositionAndRadius.z;
        lToFuture.w = lSphereContactPointIfFuture.w - lPositionAndRadius.w;
        const Vector3 lSphereNormalIfFuture = SweptNormalize(lToFuture);

        lOutContactNormal = lbResultValidAtT0 ? lSphereNormalIfPenetration : lSphereNormalIfFuture;

        const Vector3& lrSphereContactPoint = lbResultValidAtT0 ? lSphereContactPointIfPenetration
                                                                : lSphereContactPointIfFuture;
        lOutSphereContactPoint.x = lrSphereContactPoint.x;
        lOutSphereContactPoint.y = lrSphereContactPoint.y;
        lOutSphereContactPoint.z = lrSphereContactPoint.z;
        lOutSphereContactPoint.w = lfResultT;

        lOutTriangleContactPoint.x = lTriangleContactPoint.x;
        lOutTriangleContactPoint.y = lTriangleContactPoint.y;
        lOutTriangleContactPoint.z = lTriangleContactPoint.z;
        lOutTriangleContactPoint.w = lfResultT;

        return SweptMask(lPassed);
    }
}
