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

#include <cmath>     // std::sqrt (the vrsqrtefp lowering)
#include <cstring>   // std::memcpy (mask lane bit patterns)

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
