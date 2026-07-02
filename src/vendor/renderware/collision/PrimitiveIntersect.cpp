#include "vendor/renderware/collision/GPInstance.hpp"

#include "vendor/renderware/collision/CollisionVolume.hpp"   // Volume (128-byte image)

#include <cmath>     // sqrt
#include <cstring>   // memmove

// ===========================================================================
// rw::collision narrow-phase batch kernels -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass; every hand-vectorised body is
// lowered to portable per-lane scalar maths per the committed Feature /
// FeatureEdge precedent, preserving branch polarity, store order and every
// caller-visible store).
//
//   rw::collision::PrimitiveBatchIntersect       @ 0x82BABC78
//   rw::collision::GPInstanceBatchIntersect1xN   @ 0x82BAB4A8
//   rw::collision::GPInstanceBatchIntersectNx1   @ 0x82BAACD8
//   rw::collision::ComputeContactPoints          @ 0x82BABDA8
//
// Shared vocabulary (GPInstance / Interval / PrimitivePairIntersectResult /
// rwc_FeatureIntersectionPrism / VolRef1xN) lives in GPInstance.hpp.
// ===========================================================================

namespace rw
{
namespace collision
{

// X360 flt_8218025C -- the degenerate single-contact gap guard (value attested
// by the export's literal 0.00000011920929 == FLT_EPSILON). Shared by both
// batch kernels and ComputeContactPoints.
static const f32 KF_DEGENERATE_GAP_EPSILON = 1.1920929e-7f;

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; the broadcast result's
    // lane 0 is what every consumer reads).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // vsubfp: per-lane a - b (all four lanes, w included).
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    // vaddfp: per-lane a + b.
    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

    // vmulfp128 against a lvlx/vspltw splat: per-lane a * s.
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

    // vspltisw v0,-1 / vslw v0,v0,v0 / vxor: build the 0x80000000 sign-bit
    // splat and flip every lane's sign.
    inline Vec4 Negate(const Vec4& a)
    {
        Vec4 r;
        r.x = -a.x;
        r.y = -a.y;
        r.z = -a.z;
        r.w = -a.w;
        return r;
    }

    // The shared support-interval orientation test of the two batch kernels
    // (inlined twice in each: the single-contact and multi-contact paths):
    //   d1 = interval1.min - interval2.max      (vsubfp on the broadcast rows)
    //   d2 = interval2.min - interval1.max      (vsubfp)
    //   mask           = d1 > d2                (vcmpgtfp)
    //   maxSep         = vsel(d2, d1, mask)     (per-lane max)
    //   lbAllSeparated = ALL lanes: splat(distance) > maxSep  (vcmpgtfp. CR6[all])
    //   lbAnyFlip      = ANY lane of mask set; the asm tests it as
    //                    ALL lanes: (mask-as-float == 0)       (vcmpeqfp. CR6[all])
    //                    -- a true mask lane is 0xFFFFFFFF (NaN as float), so
    //                    the all-equal-zero test is exactly "no lane was >".
    // NaN polarity matches C++: `>` is false on unordered, as vcmpgtfp is.
    inline void ClassifyIntervalSeparation(const Interval& arInterval1,
                                           const Interval& arInterval2,
                                           f32 afDistance,
                                           bool& rbAllSeparated, bool& rbAnyFlip)
    {
        const f32* laf1Min = &arInterval1.min.x;
        const f32* laf1Max = &arInterval1.max.x;
        const f32* laf2Min = &arInterval2.min.x;
        const f32* laf2Max = &arInterval2.max.x;

        rbAllSeparated = true;
        rbAnyFlip      = false;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const f32  lfD1      = laf1Min[liLane] - laf2Max[liLane];
            const f32  lfD2      = laf2Min[liLane] - laf1Max[liLane];
            const bool lbGreater = lfD1 > lfD2;                          // vcmpgtfp
            const f32  lfMax     = lbGreater ? lfD1 : lfD2;              // vsel
            rbAllSeparated = rbAllSeparated && (afDistance > lfMax);     // vcmpgtfp. all
            rbAnyFlip      = rbAnyFlip || lbGreater;                     // !vcmpeqfp(mask,0) all
        }
    }
}

// ===========================================================================
// rw::collision::GPInstanceBatchIntersectNx1 @ 0x82BAACD8
//
// Three passes over min(aiNum, aiResBufMaxSize) pairs:
//   1. dispatch-table separation: per pair, the [typeN][type1] entry writes
//      the separating direction into the slot (sepDir @+0x4A0) and its lane-0
//      distance lands in the slot's distance field (+0x4F0, `addi r30, r26,
//      0x4F0` -- NOT the DWARF sepDist slot; see GPInstance.hpp).
//   2. fatness cull + feature extraction: pairs with
//      distance > fatnessN + padding + fatness1 are marked dead
//      (numPoints = 0, vNindex = -1); live pairs get their maximum features
//      built into f1/f2 (+dir for the N side, -dir for the 1 side).
//   3. prism intersection + contact resolution: live pairs run
//      FindFeatureIntersectionPrism; hits are compacted into
//      lapResults[hitCount] (the output cursor advances only on acceptance).
// Returns the number of accepted intersections.
// ===========================================================================
s32 GPInstanceBatchIntersectNx1(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance* lapInsts1, s32 aiNum,
                                const GPInstance& arInst2,
                                f32 afPadding)
{
    s32 liNumIntersections = 0;

    // r28 = min(aiNum, aiResBufMaxSize)
    s32 liCount = aiNum;
    if (liCount > aiResBufMaxSize)
    {
        liCount = aiResBufMaxSize;
    }

    // ---- pass 1: separating-direction dispatch (0x82BAAD34 loop) ----------
    if (liCount > 0)
    {
        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpSlot     = &lapResults[liPair];

            // off_82F91800[6*typeN + type1](sepDir, N, 1); the returned
            // broadcast's lane 0 (stvx128 v1 + lfs) is the separating distance.
            const FindBestSeparatingDirectionFn lpfnSeparate =
                gapFindBestSeparatingDirection[lpInstance->mVolumeType][arInst2.mVolumeType];
            lpSlot->distance = lpfnSeparate(lpSlot->sepDir, *lpInstance, arInst2);
        }
    }

    // ---- passes 2 + 3 (both under the same count guard, 0x82BAAD84) -------
    if (liCount > 0)
    {
        // ---- pass 2: fatness cull + maximum-feature extraction ------------
        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpSlot     = &lapResults[liPair];

            // fcmpu cr6, distance, (fatnessN + padding) + fatness1; bgt -> cull.
            const f32 lfThreshold = (lpInstance->mFatness + afPadding) + arInst2.mFatness;
            if (lpSlot->distance > lfThreshold)
            {
                lpSlot->numPoints = 0;
                lpSlot->vNindex   = -1;
            }
            else
            {
                lpSlot->vNindex = liPair;

                // N side: maximum feature along +sepDir, ccw = 1.
                const Vec4 lvDir = lpSlot->sepDir;   // lvx128 slot+0x4A0
                lpInstance->mMethods.mGetMaximumFeature(lpInstance, 1, lvDir, lpSlot->f1);

                // 1 side: maximum feature along -sepDir (vspltisw/vslw/vxor
                // sign flip of the reloaded slot direction), ccw = 0.
                const Vec4 lvNegDir = Negate(lpSlot->sepDir);
                arInst2.mMethods.mGetMaximumFeature(&arInst2, 0, lvNegDir, lpSlot->f2);
            }
        }

        // ---- pass 3: prism intersection + contact resolution --------------
        // The asm reuses ONE stack work buffer (sp+0x1C0) across all
        // iterations, so it is hoisted here; only the normalOverride word is
        // re-zeroed per pair (stw r22, var_AC at 0x82BAAE54).
        rwc_FeatureIntersectionPrism lWork;

        for (s32 liPair = 0; liPair < liCount; ++liPair)
        {
            const GPInstance*             lpInstance = &lapInsts1[liPair];
            PrimitivePairIntersectResult* lpIn       = &lapResults[liPair];

            lWork.normalOverride = 0;

            // Fatness-culled pairs (vNindex < 0, signed test) are skipped.
            if (lpIn->vNindex < 0)
            {
                continue;
            }
            if (!FindFeatureIntersectionPrism(lWork, lpIn->f1, lpIn->f2, lpIn->sepDir))
            {
                continue;
            }
            // Capacity check happens AFTER the prism call, and it does not
            // terminate the loop (bge -> loop bottom): remaining pairs still
            // run FindFeatureIntersectionPrism.
            if (liNumIntersections >= aiResBufMaxSize)
            {
                continue;
            }

            // Output slot: results are compacted to lapResults[hitCount]
            // (r31 = output cursor; r23/r27 keep tracking the input pair).
            // NOTE: vNindex of the output slot is NOT rewritten on
            // compaction -- it keeps whatever pass 2 stored there, exactly
            // as the binary leaves it.
            PrimitivePairIntersectResult* lpOut = &lapResults[liNumIntersections];

            // numPoints is published to the output slot before the copy loop.
            const u32 luNumPoints = static_cast<u32>(lWork.m_numpts);
            lpOut->numPoints = luNumPoints;
            for (u32 luPoint = 0; luPoint < luNumPoints; ++luPoint)
            {
                lpOut->pointsOn2[luPoint] = lWork.m_ptsOn2[luPoint];   // buf+0x100 -> out+0x600
                lpOut->pointsOn1[luPoint] = lWork.m_ptsOn1[luPoint];   // buf+0x000 -> out+0x500
            }

            f32 lfDistanceOut;

            if (lpOut->numPoints == 1)
            {
                // ==== single-contact branch (0x82BAAED4) ====================
                // delta = pt2 - pt1; stored to the normal slot immediately
                // (stvx128 v13 -> out+0x4C0), then length-tested.
                const Vec4 lvDelta = Sub(lpOut->pointsOn2[0], lpOut->pointsOn1[0]);
                lpOut->normal = lvDelta;
                const f32 lfLenSq = Dot3(lvDelta, lvDelta);

                bool lbUseInputDir = false;
                if (lfLenSq <= KF_DEGENERATE_GAP_EPSILON)
                {
                    // fcmpu ble -> take the dispatch direction instead.
                    lbUseInputDir = true;
                }
                else
                {
                    // Normalise: vrsqrtefp estimate + two Newton-Raphson
                    // refine steps (vspltisw/vcfsx build the 1.0/0.5
                    // constants; vmulfp128/vnmsubfp/vmaddfp chain), then
                    // delta * recipLen. Rendered as the mathematically
                    // equivalent scalar.
                    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
                    lpOut->normal = Scale(lvDelta, lfInvLen);

                    // Support intervals of both primitives along the
                    // normalised direction (v1 = dir on both calls).
                    Interval lInterval1;
                    Interval lInterval2;
                    lpInstance->mMethods.mGetInterval(lpInstance, lpOut->normal, lInterval1);
                    arInst2.mMethods.mGetInterval(&arInst2, lpOut->normal, lInterval2);

                    // Orientation resolve against the pass-1 distance of the
                    // INPUT slot (lfs 0x50(r23) = in+0x4F0).
                    bool lbAllSeparated;
                    bool lbAnyFlip;
                    ClassifyIntervalSeparation(lInterval1, lInterval2, lpIn->distance,
                                               lbAllSeparated, lbAnyFlip);
                    if (lbAllSeparated)
                    {
                        lbUseInputDir = true;                          // -> LABEL_24
                    }
                    else if (lbAnyFlip)
                    {
                        // vxor sign flip of the stored direction.
                        lpOut->normal = Negate(lpOut->normal);
                    }
                    // else: keep the normalised direction (no store).
                }
                if (lbUseInputDir)
                {
                    // lvx128 v0, r0, r23 (input slot sepDir) -> out+0x4C0.
                    lpOut->normal = lpIn->sepDir;
                }

                // ==== LABEL_26: fatten both sides along the direction =======
                const f32  lfFatness1 = lpInstance->mFatness;   // lvlx + vspltw splat
                const f32  lfFatness2 = arInst2.mFatness;
                const Vec4 lvNormal   = lpOut->normal;

                // pointOn1 = dir*fat1 + pt1[0] (vmaddfp); pointOn2 = pt2[0]
                // - dir*fat2 (vmulfp128 + vsubfp) -- both from the UNfattened
                // point 0.
                lpOut->pointOn1 = MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[0]);
                lpOut->pointOn2 = Sub(lpOut->pointsOn2[0], Scale(lvNormal, lfFatness2));

                // Per-point fatten + separation (loop guarded on numPoints,
                // count re-read from out+0x740 each iteration).
                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointsOn1[luPoint] =
                        MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointsOn2[luPoint] =
                        Sub(lpOut->pointsOn2[luPoint], Scale(lvNormal, lfFatness2));
                    // separation = dot3(pt2'[i] - pt1'[i], dir) (vmsum3fp128 lane 0).
                    lpOut->distances[luPoint] =
                        Dot3(Sub(lpOut->pointsOn2[luPoint], lpOut->pointsOn1[luPoint]),
                             lvNormal);
                }

                // distance = dot3(pointOn2 - pointOn1, dir).
                lfDistanceOut = Dot3(Sub(lpOut->pointOn2, lpOut->pointOn1), lvNormal);
            }
            else
            {
                // ==== multi-contact branch (0x82BAB10C) =====================
                // Seed both averages with the 0.0f constant (flt_82001CC0)
                // and accumulate every contact point (vaddfp loop).
                Vec4 lvZero;
                lvZero.x = 0.0f;
                lvZero.y = 0.0f;
                lvZero.z = 0.0f;
                lvZero.w = 0.0f;
                lpOut->pointOn1 = lvZero;
                lpOut->pointOn2 = lvZero;
                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointOn1 = Add(lpOut->pointOn1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointOn2 = Add(lpOut->pointOn2, lpOut->pointsOn2[luPoint]);
                }

                // 1/count: std/lfd/fcfid (exact s64->f64) + frsp + fdivs of
                // the 1.0f constant (flt_82001C98), splatted via lvlx+vspltw.
                // This kernel performs the divide once and reuses it.
                const f32 lfInvCount = 1.0f / static_cast<f32>(lpOut->numPoints);
                lpOut->pointOn1 = Scale(lpOut->pointOn1, lfInvCount);
                lpOut->pointOn2 = Scale(lpOut->pointOn2, lfInvCount);

                bool lbUseInputDir = false;
                if (lWork.normalOverride != 0)
                {
                    // dir = the prism normal (lvx128 buf+0x200 -> out+0x4C0).
                    lpOut->normal = lWork.normal;

                    Interval lInterval1;
                    Interval lInterval2;
                    lpInstance->mMethods.mGetInterval(lpInstance, lpOut->normal, lInterval1);
                    arInst2.mMethods.mGetInterval(&arInst2, lpOut->normal, lInterval2);

                    bool lbAllSeparated;
                    bool lbAnyFlip;
                    ClassifyIntervalSeparation(lInterval1, lInterval2, lpIn->distance,
                                               lbAllSeparated, lbAnyFlip);
                    if (lbAllSeparated)
                    {
                        lbUseInputDir = true;                          // -> LABEL_37
                    }
                    else if (lbAnyFlip)
                    {
                        lpOut->normal = Negate(lpOut->normal);
                    }
                }
                else
                {
                    lbUseInputDir = true;                              // -> LABEL_37
                }
                if (lbUseInputDir)
                {
                    lpOut->normal = lpIn->sepDir;
                }

                // ==== LABEL_39: fatten around the averaged points ===========
                const f32  lfFatness1 = lpInstance->mFatness;
                const f32  lfFatness2 = arInst2.mFatness;
                const Vec4 lvNormal   = lpOut->normal;

                lpOut->pointOn1 = MaddScalar(lvNormal, lfFatness1, lpOut->pointOn1);
                lpOut->pointOn2 = Sub(lpOut->pointOn2, Scale(lvNormal, lfFatness2));

                for (u32 luPoint = 0; luPoint < lpOut->numPoints; ++luPoint)
                {
                    lpOut->pointsOn1[luPoint] =
                        MaddScalar(lvNormal, lfFatness1, lpOut->pointsOn1[luPoint]);
                    lpOut->pointsOn2[luPoint] =
                        Sub(lpOut->pointsOn2[luPoint], Scale(lvNormal, lfFatness2));
                    lpOut->distances[luPoint] =
                        Dot3(Sub(lpOut->pointsOn2[luPoint], lpOut->pointsOn1[luPoint]),
                             lvNormal);
                }

                lfDistanceOut = Dot3(Sub(lpOut->pointOn2, lpOut->pointOn1), lvNormal);
            }

            // ==== LABEL_43: distance publish + triangle direction gates =====
            lpOut->distance = lfDistanceOut;                 // stfs -> out+0x4F0

            // N side: one-sided triangle rejection with +dir (v1 = dir).
            if (lpInstance->mVolumeType == GPInstance::TRIANGLE &&
                !GPTriangleAcceptContactNormal(lpInstance, lpOut->normal))
            {
                lpOut->numPoints = 0;
            }
            // 1 side: same test with -dir (vspltisw/vslw/vxor sign flip).
            if (arInst2.mVolumeType == GPInstance::TRIANGLE)
            {
                const Vec4 lvNegNormal = Negate(lpOut->normal);
                if (!GPTriangleAcceptContactNormal(&arInst2, lvNegNormal))
                {
                    lpOut->numPoints = 0;
                }
            }

            // ==== acceptance: publish tags/direction/features, advance ======
            if (lpOut->numPoints != 0)
            {
                lpOut->v1   = lpInstance->mVolumeTag;   // N +0x84 -> out+0x00
                lpOut->tag1 = lpInstance->mUserTag;     // N +0x88 -> out+0x04
                lpOut->v2   = arInst2.mVolumeTag;       // 1 +0x84 -> out+0x08
                lpOut->tag2 = arInst2.mUserTag;         // 1 +0x88 -> out+0x0C
                lpOut->sepDir = lpIn->sepDir;           // in+0x4A0 -> out+0x4A0

                // bl memcpy x2: both 0x240-byte Feature blocks from the input
                // slot. When no pair has been culled the ranges coincide
                // exactly (dst == src), so memmove is used for well-defined
                // x64 behaviour; the console memcpy is a no-op there too.
                memmove(&lpOut->f1, &lpIn->f1, sizeof(Feature));
                memmove(&lpOut->f2, &lpIn->f2, sizeof(Feature));

                ++liNumIntersections;
            }
        }
    }

    return liNumIntersections;
}

// ===========================================================================
// rw::collision::GPInstanceBatchIntersect1xN @ 0x82BAB4A8
//
// The 1-vs-N sibling of the kernel above (same three-pass structure; the
// structural deltas the asm carries are kept: the output cursor advances as a
// pointer, the multi-contact average performs the 1/count divide once per
// average, the feature copies are memcpy-shaped, and the orientation/fatten
// helpers are the two inlined blocks below).
// (X360 __fastcall: r3=results, r4=maxResults, r5=inst1, r6=instsN, r7=num,
//  f1=padding; returns the surviving pair count in r3.)
// ===========================================================================

namespace
{
    // ---------------------------------------------------------------------
    // SelectContactNormal -- the interval sanity check the 1xN body inlines
    // twice (0x82BAB728 single-point path, 0x82BAB9E0 multi-point path).
    // Projects both instances onto the candidate normal and either keeps it,
    // flips it, or falls back to the coarse separating direction:
    //   sep1 = i1.min - i2.max            (vsubfp on the broadcast rows)
    //   sep2 = i2.min - i1.max
    //   best = max(sep1, sep2)            (vcmpgtfp mask + vsel)
    //   if (distance > best)   normal = sepDir     (vcmpgtfp. all-lanes CR6)
    //   else if (sep1 > sep2)  normal = -normal    (vcmpeqfp. mask==0 failed
    //                                               -> vxor sign flip)
    // (Lane-identical inputs: the interval rows are broadcasts, so the lane-0
    // scalar compare equals the all-lanes test here.)
    // ---------------------------------------------------------------------
    void SelectContactNormal(const GPInstance& arInst1,
                             const GPInstance& arInstN,
                             f32 afDistance,
                             const Vec4& arSepDir,
                             Vec4& rNormal)
    {
        Interval lInterval1;
        Interval lInterval2;
        arInst1.mMethods.mGetInterval(&arInst1, rNormal, lInterval1);
        arInstN.mMethods.mGetInterval(&arInstN, rNormal, lInterval2);

        const f32 lfSep1 = lInterval1.min.x - lInterval2.max.x;
        const f32 lfSep2 = lInterval2.min.x - lInterval1.max.x;
        const f32 lfBestSep = (lfSep1 > lfSep2) ? lfSep1 : lfSep2;

        if (afDistance > lfBestSep)
        {
            rNormal = arSepDir;
        }
        else if (lfSep1 > lfSep2)
        {
            rNormal = Negate(rNormal);
        }
    }

    // ---------------------------------------------------------------------
    // FattenContactPoints -- the contact-point push-out the 1xN body inlines
    // twice (0x82BAB7F0 single-point path, 0x82BABABC multi-point path). Both
    // reference points and every clipped point pair are pushed apart along
    // the contact normal by the instances' fatness (lvlx+vspltw fatness
    // splats feeding vmaddfp / vmulfp128+vsubfp), each pair's penetration
    // along the normal is stored (vmsum3fp128), and the reference-pair
    // penetration is returned.
    // ---------------------------------------------------------------------
    f32 FattenContactPoints(PrimitivePairIntersectResult* lpResult,
                            const GPInstance& arInst1,
                            const GPInstance& arInstN,
                            const Vec4& arNormal,
                            const Vec4& arSeedOn1,
                            const Vec4& arSeedOn2)
    {
        const f32 lfFatness1 = arInst1.mFatness;
        const f32 lfFatnessN = arInstN.mFatness;

        // vmaddfp(normal, fat1, seed1) / vsubfp(seed2, vmulfp128(normal, fatN))
        lpResult->pointOn1 = MaddScalar(arNormal, lfFatness1, arSeedOn1);
        lpResult->pointOn2 = Sub(arSeedOn2, Scale(arNormal, lfFatnessN));

        // The asm reloads numPoints as the loop bound every pass; it is not
        // modified inside the loop, so a plain bound read is equivalent.
        for (u32 luPoint = 0; luPoint < lpResult->numPoints; ++luPoint)
        {
            lpResult->pointsOn1[luPoint] =
                MaddScalar(arNormal, lfFatness1, lpResult->pointsOn1[luPoint]);
            lpResult->pointsOn2[luPoint] =
                Sub(lpResult->pointsOn2[luPoint], Scale(arNormal, lfFatnessN));
            lpResult->distances[luPoint] =
                Dot3(Sub(lpResult->pointsOn2[luPoint], lpResult->pointsOn1[luPoint]),
                     arNormal);
        }

        return Dot3(Sub(lpResult->pointOn2, lpResult->pointOn1), arNormal);
    }
}

s32 GPInstanceBatchIntersect1xN(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance& arInst1,
                                const GPInstance* lapInstsN, s32 aiNum,
                                f32 afPadding)
{
    s32 liNumCandidates = aiNum;
    if (liNumCandidates > aiResBufMaxSize)
    {
        liNumCandidates = aiResBufMaxSize;
    }

    s32 liNumResults = 0;

    // ---- pass 1 (0x82BAB504): coarse separating direction + distance -------
    // Dispatch through the [type1][typeN] table; the callee writes the
    // direction into the candidate slot (r3 = &slot.sepDir) and returns the
    // distance in v1, whose first lane lands in slot.distance
    // (stvx128 to a stack row + lfs/stfs of lane 0 -- +0x4F0, see the header).
    for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
    {
        PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
        const GPInstance* lpInstanceN = &lapInstsN[liPair];

        const FindBestSeparatingDirectionFn lpfnSeparate =
            gapFindBestSeparatingDirection[arInst1.mVolumeType][lpInstanceN->mVolumeType];
        lpCandidate->distance = lpfnSeparate(lpCandidate->sepDir, arInst1, *lpInstanceN);
    }

    // ---- pass 2 (0x82BAB564): fatness cull + support features --------------
    for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
    {
        PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
        const GPInstance* lpInstanceN = &lapInstsN[liPair];

        // fcmpu: separated further than both fatnesses plus the caller's pad.
        if (lpCandidate->distance > (lpInstanceN->mFatness + afPadding) + arInst1.mFatness)
        {
            lpCandidate->numPoints = 0;
            lpCandidate->vNindex   = -1;
        }
        else
        {
            lpCandidate->vNindex = liPair;
            // Support feature on instance 1 along +direction (r4 = 1) and on
            // instance N along -direction (r4 = 0; vspltisw(-1)+vslw+vxor
            // sign-flip of the direction register).
            arInst1.mMethods.mGetMaximumFeature(&arInst1, 1, lpCandidate->sepDir,
                                                lpCandidate->f1);
            const Vec4 lvNegDir = Negate(lpCandidate->sepDir);
            lpInstanceN->mMethods.mGetMaximumFeature(lpInstanceN, 0, lvNegDir,
                                                     lpCandidate->f2);
        }
    }

    // ---- pass 3 (0x82BAB620): prism clip, normal, push-out, compaction -----
    if (liNumCandidates > 0)
    {
        PrimitivePairIntersectResult* lpResult = lapResults;   // compacted output cursor

        for (s32 liPair = 0; liPair < liNumCandidates; ++liPair)
        {
            PrimitivePairIntersectResult* lpCandidate = &lapResults[liPair];
            const GPInstance* lpInstanceN = &lapInstsN[liPair];

            // The caller zeroes the "normal computed" flag every iteration
            // (stw r22, var_AC) before anything else runs.
            rwc_FeatureIntersectionPrism lPrism;
            lPrism.normalOverride = 0;

            if (lpCandidate->vNindex < 0)
            {
                continue;
            }
            if (!FindFeatureIntersectionPrism(lPrism, lpCandidate->f1, lpCandidate->f2,
                                              lpCandidate->sepDir))
            {
                continue;
            }
            if (liNumResults >= aiResBufMaxSize)
            {
                continue;   // capacity reached: keep scanning, keep nothing
            }

            // Stage the clipped points into the output slot (the r20=-0x100
            // twin-array copy loop).
            lpResult->numPoints = static_cast<u32>(lPrism.m_numpts);
            for (s32 liPoint = 0; liPoint < lPrism.m_numpts; ++liPoint)
            {
                lpResult->pointsOn2[liPoint] = lPrism.m_ptsOn2[liPoint];
                lpResult->pointsOn1[liPoint] = lPrism.m_ptsOn1[liPoint];
            }

            f32 lfDistance;
            if (lpResult->numPoints == 1)
            {
                // --- single contact (0x82BAB6A4): normal = normalize(p2-p1) -
                const Vec4 lvGap = Sub(lpResult->pointsOn2[0], lpResult->pointsOn1[0]); // vsubfp
                const f32 lfLenSq = Dot3(lvGap, lvGap);                 // vmsum3fp128
                lpResult->normal = lvGap;                               // raw gap staged first

                if (lfLenSq <= KF_DEGENERATE_GAP_EPSILON)               // flt_8218025C
                {
                    // Degenerate gap: keep the coarse separating direction.
                    lpResult->normal = lpCandidate->sepDir;
                }
                else
                {
                    // vrsqrtefp estimate + two vnmsubfp/vmaddfp Newton-Raphson
                    // refinements (est += 0.5*est*(1 - d*est*est), twice)
                    // == 1/sqrt(d) to full precision.
                    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
                    lpResult->normal = Scale(lvGap, lfInvLen);          // vmulfp128
                    SelectContactNormal(arInst1, *lpInstanceN, lpCandidate->distance,
                                        lpCandidate->sepDir, lpResult->normal);
                }

                lfDistance = FattenContactPoints(lpResult, arInst1, *lpInstanceN,
                                                 lpResult->normal,
                                                 lpResult->pointsOn1[0],
                                                 lpResult->pointsOn2[0]);
            }
            else
            {
                // --- multi contact (0x82BAB8DC): reference points = averages -
                Vec4 lvZero;                       // the asm stores four zero
                lvZero.x = 0.0f;                   // lanes (stfs flt_82001CC0
                lvZero.y = 0.0f;                   // x3 + stw 0)
                lvZero.z = 0.0f;
                lvZero.w = 0.0f;
                lpResult->pointOn1 = lvZero;
                lpResult->pointOn2 = lvZero;

                for (u32 luPoint = 0; luPoint < lpResult->numPoints; ++luPoint)  // vaddfp
                {
                    lpResult->pointOn1 = Add(lpResult->pointOn1, lpResult->pointsOn1[luPoint]);
                    lpResult->pointOn2 = Add(lpResult->pointOn2, lpResult->pointsOn2[luPoint]);
                }

                // 1/count via std+lfd+fcfid+frsp then fdivs (flt_82001C98 =
                // 1.0f numerator); this kernel reloads/reconverts the count
                // and re-divides once per average, exactly as the asm does.
                lpResult->pointOn1 = Scale(lpResult->pointOn1,
                    1.0f / static_cast<f32>(lpResult->numPoints));
                lpResult->pointOn2 = Scale(lpResult->pointOn2,
                    1.0f / static_cast<f32>(lpResult->numPoints));

                if (lPrism.normalOverride)
                {
                    lpResult->normal = lPrism.normal;
                    SelectContactNormal(arInst1, *lpInstanceN, lpCandidate->distance,
                                        lpCandidate->sepDir, lpResult->normal);
                }
                else
                {
                    lpResult->normal = lpCandidate->sepDir;
                }

                lfDistance = FattenContactPoints(lpResult, arInst1, *lpInstanceN,
                                                 lpResult->normal,
                                                 lpResult->pointOn1,
                                                 lpResult->pointOn2);
            }

            // --- LABEL_43 (0x82BABB9C): finalize / reject / compact ----------
            lpResult->distance = lfDistance;

            // One-sided triangle rejection (X360 sub_82BAA600): instance 1
            // against the normal, instance N against the flipped normal.
            if (arInst1.mVolumeType == GPInstance::TRIANGLE
                && !GPTriangleAcceptContactNormal(&arInst1, lpResult->normal))
            {
                lpResult->numPoints = 0;
            }
            if (lpInstanceN->mVolumeType == GPInstance::TRIANGLE
                && !GPTriangleAcceptContactNormal(lpInstanceN, Negate(lpResult->normal)))
            {
                lpResult->numPoints = 0;
            }

            if (lpResult->numPoints != 0)
            {
                lpResult->v1   = arInst1.mVolumeTag;
                lpResult->tag1 = arInst1.mUserTag;
                lpResult->v2   = lpInstanceN->mVolumeTag;
                lpResult->tag2 = lpInstanceN->mUserTag;
                lpResult->sepDir = lpCandidate->sepDir;
                // 0x240-byte block copies of the staged features into the
                // compacted slot (a self-copy when the slots coincide, exactly
                // as the console's memcpy performed; memmove keeps that
                // well-defined on x64).
                memmove(&lpResult->f1, &lpCandidate->f1, sizeof(Feature));
                memmove(&lpResult->f2, &lpCandidate->f2, sizeof(Feature));
                ++liNumResults;
                ++lpResult;
            }
        }
    }

    return liNumResults;
}

// ===========================================================================
// rw::collision::PrimitiveBatchIntersect @ 0x82BABC78
//
// Narrow-phase batch driver: walks the overlap-report buffer that
// VolumeVolumeQuery::GetPrimitiveBBoxOverlaps (0x82BB3AB0) staged, instances
// each report group's volumes into the caller's GPInstance scratch buffer via
// the collision-volume vtable, and hands every group to the matching batch
// kernel. Sole caller: VolumeVolumeQuery::GetPrimitiveIntersections
// (0x82BB3FF0, currently declaration-only in VolumeQuery.hpp).
//
// VMX NOTE (dedicated VMX pass): despite living in the hand-vectorised
// narrow-phase TU group, this driver itself contains NO vector instructions.
// The only FP ops are `fmr f31, f1` / `fmr f1, f31` -- a bit-preserving save/
// restore of the incoming padding value across the virtual instancing calls so
// it can be re-passed in f1 to the batch callees. That is rendered as an
// ordinary f32 pass-through parameter.
// ===========================================================================

namespace
{
    // Collision-volume dispatch table, reached through the pointer at
    // Volume+0x40 (`lwz r11, 0x40(r3)`), inside the opaque pad of the
    // committed 128-byte Volume image (CollisionVolume.hpp). Slot INDICES (word slots on
    // X360, pointer slots here) are the preserved layout:
    //   slot 1 (+0x04)  GetBBox            (VolumeVolumeQuery @ 0x82BB3AB0)
    //   slot 5 (+0x14)  CreateGPInstance   (this driver)
    // Unreferenced slots stay untyped. (TU-local view; the full Volume::VTable
    // shape -- DWARF volume.h:1507 -- lands with the Volume TU.)
    typedef void (*CreateGPInstanceFn)(const Volume* lpVolume,
                                       GPInstance*   lpInstance,
                                       const void*   lpTransform);

    struct VolumeVTable
    {
        void*              mpSlot00;           // slot 0 (unreferenced here)
        void*              mpGetBBox;          // slot 1
        void*              mpSlot08;           // slot 2
        void*              mpSlot0C;           // slot 3
        void*              mpSlot10;           // slot 4
        CreateGPInstanceFn mpCreateGPInstance; // slot 5
    };

    // FLAG (word-width): the committed VolRef keeps its X360 words as u32
    // (muVolumePtr / muTransformPtr). The 0x82BABC78 asm attests their meaning
    // at this consumer: +0x00 -> the Volume* the vtable dispatch runs on
    // (lwz r3, 0(rRef)); +0x04 -> the cached transform pointer passed in r5
    // (lwz r5, 4(rRef)). The u32 fields are widened through uintptr_t here; if
    // VolRef is later promoted to pointer-typed fields these collapse to
    // member reads.
    const Volume* VolRefVolume(const VolRef& lrRef)
    {
        return reinterpret_cast<const Volume*>(static_cast<uintptr_t>(lrRef.muVolumePtr));
    }

    const void* VolRefTransform(const VolRef& lrRef)
    {
        return reinterpret_cast<const void*>(static_cast<uintptr_t>(lrRef.muTransformPtr));
    }

    // The dispatch-table pointer lives at X360 byte +0x40 of the volume image;
    // read it by image offset until the full Volume layout lands (raw offset
    // documented per the serialised rw::collision data exception).
    const VolumeVTable* GetVolumeVTable(const Volume* lpVolume)
    {
        return *reinterpret_cast<const VolumeVTable* const*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x40);
    }
}

s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,
                            s32 aiResBufMaxSize,
                            GPInstance* lapInstancingBuffer,
                            VolRef1xN* lapPairs, s32 aiNumPairs,
                            f32 afPadding)
{
    s32 liNumIntersections = 0;                                     // r30

    if (aiNumPairs > 0)                                             // cmpwi/ble (signed)
    {
        VolRef1xN* lpGroup           = lapPairs;                    // r31
        s32        liGroupsRemaining = aiNumPairs;                  // r23
        do
        {
            if (liNumIntersections < aiResBufMaxSize)               // cmpw/bge (signed)
            {
                // ---- instance the "1"-side volume into slot 0 ----
                const VolRef* lpRef1    = lpGroup->vRef1;
                const Volume* lpVolume1 = VolRefVolume(*lpRef1);
                GetVolumeVTable(lpVolume1)->mpCreateGPInstance(
                    lpVolume1, &lapInstancingBuffer[0], VolRefTransform(*lpRef1));
                lapInstancingBuffer[0].mVolumeTag = lpRef1->muTag;  // stw r11, 0x88(r29)

                // ---- instance the N-side volumes into slots 1..N ----
                u32 luRefIndex = 0;                                 // r26
                if (lpGroup->vRefsNCount != 0)                      // cmplwi/ble (unsigned)
                {
                    GPInstance* lpInstanceN = &lapInstancingBuffer[1]; // r27 (0xC0 stride)
                    VolRef**    lppRefN     = &lpGroup->vRefsN[0];     // r28
                    do
                    {
                        const VolRef* lpRefN    = *lppRefN;
                        const Volume* lpVolumeN = VolRefVolume(*lpRefN);
                        GetVolumeVTable(lpVolumeN)->mpCreateGPInstance(
                            lpVolumeN, lpInstanceN, VolRefTransform(*lpRefN));
                        ++luRefIndex;                               // addi r26, r26, 1
                        lpInstanceN->mVolumeTag = lpRefN->muTag;    // stw r11, 0(r27)
                        ++lppRefN;                                  // addi r28, r28, 4
                        ++lpInstanceN;                              // addi r27, r27, 0xC0
                    }
                    while (luRefIndex < lpGroup->vRefsNCount);      // count reloaded per pass
                }

                // ---- narrow-phase batch on the staged instances ----
                const s32 liRemaining = aiResBufMaxSize - liNumIntersections; // subf
                PrimitivePairIntersectResult* lpOut =
                    lapResults + liNumIntersections;                // mulli 0x750 + add

                s32 liFound;
                if (lpGroup->volumesSwapped != 0)                   // cmplwi/beq on word 2
                {
                    liFound = GPInstanceBatchIntersectNx1(
                        lpOut, liRemaining,
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        lapInstancingBuffer[0], afPadding);
                }
                else
                {
                    liFound = GPInstanceBatchIntersect1xN(
                        lpOut, liRemaining,
                        lapInstancingBuffer[0],
                        &lapInstancingBuffer[1], static_cast<s32>(lpGroup->vRefsNCount),
                        afPadding);
                }
                liNumIntersections += liFound;                      // add r30, r3, r30
            }

            // Group advance runs even when the result buffer is full
            // (console: lpuGroup += lpuGroup[1] + 3).
            lpGroup = lpGroup->NextGroup();
            --liGroupsRemaining;                                    // addic.
        }
        while (liGroupsRemaining != 0);                             // bne
    }

    return liNumIntersections;                                      // mr r3, r30
}

// ===========================================================================
// rw::collision::ComputeContactPoints @ 0x82BABDA8
// Called by: ContactGeneratorJob::CollideGPInstances.
//
// Narrow-phase contact generation for one GP-instance pair:
//   1. dispatch off_82F91800[type1*6+type2] for the best separating direction
//      and separation distance;
//   2. early-out when distance > fatness1 + fatness2 + padding;
//   3. build each instance's maximal feature along +/-direction and intersect
//      the two feature prisms;
//   4. when the prisms meet in exactly one point pair, derive a fresh normal
//      from the (p2 - p1) delta if it is long enough;
//   5. with a valid derived normal, interval-test both instances along it and
//      adopt +/-normal as the contact normal when the pair is separated to at
//      least the dispatch distance along it;
//   6. re-validate one-sided-triangle instances against the final normal
//      (X360 sub_82BAA600);
//   7. emit tags, the NEGATED normal, the pair count, and the point pairs
//      pushed out along the normal by each instance's fatness.
//
// Returns the number of contact-point pairs written (0 on any rejection).
// The X360 body also parks a zero in a dead stack slot (stw r28, var_790,
// twice) that nothing reads back; no C++ equivalent is emitted.
// ===========================================================================
u32 ComputeContactPoints(const GPInstance& arGP1, const GPInstance& arGP2,
                         const f32& arPadding, GPInstance::ContactPoints& arResult)
{
    // --- 1. best separating direction via the 6x6 type dispatch -----------
    // (*(&off_82F91800[6 * gp1.type] + gp2.type))(&direction, gp1, gp2); the
    // distance comes back as a v1 lane broadcast (stvx128 to var_780, lfs
    // lane 0 -> f31).
    Vec4 lvDirection;
    const FindBestSeparatingDirectionFn lpfnFindSepDir =
        gapFindBestSeparatingDirection[arGP1.mVolumeType][arGP2.mVolumeType];
    const f32 lfDistance = lpfnFindSepDir(lvDirection, arGP1, arGP2);

    // --- 2. range early-out (fadds/fadds/fcmpu, bgt -> return 0) ----------
    // Scalar add ORDER preserved: (fatness2 + padding) + fatness1.
    if (lfDistance > ((arGP2.mFatness + arPadding) + arGP1.mFatness))
    {
        return 0;
    }

    // --- 3. per-instance maximal features, then the prism intersection ----
    // First instance: the +0xA4 callback with r4=1 and the direction in v1.
    // Second instance: r4=0 and the NEGATED direction (vxor sign-bit splat).
    Feature lFeature1;
    Feature lFeature2;
    arGP1.mMethods.mGetMaximumFeature(&arGP1, 1, lvDirection, lFeature1);
    arGP2.mMethods.mGetMaximumFeature(&arGP2, 0, Negate(lvDirection), lFeature2);

    rwc_FeatureIntersectionPrism lIntersection;
    lIntersection.normalOverride = 0;    // stw r28, var_4EC before the call
    if (!FindFeatureIntersectionPrism(lIntersection, lFeature1, lFeature2, lvDirection))
    {
        return 0;
    }

    // v127 = the direction, reloaded from var_770 only after the prism call.
    Vec4 lvNormal = lvDirection;

    // --- 4. single point pair: derive the normal from the point delta -----
    if (lIntersection.m_numpts == 1)
    {
        // delta = p2[0] - p1[0]                             (vsubfp, all lanes)
        const Vec4 lvDelta = Sub(lIntersection.m_ptsOn2[0], lIntersection.m_ptsOn1[0]);

        // lenSq = dot3(delta, delta)                        (vmsum3fp128)
        const f32 lfLenSq = Dot3(lvDelta, lvDelta);

        // length = lenSq * rsqrt(lenSq), i.e. sqrt(lenSq): vrsqrtefp estimate
        // + TWO Newton-Raphson refines (vmulfp128/vnmsubfp/vmaddfp x2, with
        // the vcfsx-built 1.0/0.5 splats), zero-guarded by vcmpeqfp/vsel when
        // lenSq == 0. Rendered as the mathematically equivalent scalar.
        const f32 lfInvLength = (lfLenSq == 0.0f) ? 0.0f : (1.0f / std::sqrt(lfLenSq));
        const f32 lfLength    = lfLenSq * lfInvLength;

        // fcmpu against flt_8218025C; ble skips the normal adoption.
        if (lfLength > KF_DEGENERATE_GAP_EPSILON)
        {
            // normal = delta * rsqrt(lenSq)                 (vmulfp128, all lanes)
            lIntersection.normalOverride = 1;
            lIntersection.normal = Scale(lvDelta, lfInvLength);
        }
    }

    // --- 5. interval test along the derived normal --------------------------
    if (lIntersection.normalOverride != 0)
    {
        // Both +0xA8 callbacks take the SAME (un-negated) derived normal in v1.
        Interval lInterval1;
        Interval lInterval2;
        arGP1.mMethods.mGetInterval(&arGP1, lIntersection.normal, lInterval1);
        arGP2.mMethods.mGetInterval(&arGP2, lIntersection.normal, lInterval2);

        // sepA = i1.min - i2.max / sepB = i2.min - i1.max   (vsubfp, all lanes)
        // mask = sepA > sepB per lane (vcmpgtfp); sel = max (vsel);
        // vcmpgefp. sel >= splat(distance) with the CR6 "all lanes" bit.
        const f32* laf1Min = &lInterval1.min.x;
        const f32* laf1Max = &lInterval1.max.x;
        const f32* laf2Min = &lInterval2.min.x;
        const f32* laf2Max = &lInterval2.max.x;

        bool lbSeparatedToDistance = true;
        bool lbNoneGreater         = true;
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const f32  lfSepA    = laf1Min[liLane] - laf2Max[liLane];
            const f32  lfSepB    = laf2Min[liLane] - laf1Max[liLane];
            const bool lbGreater = lfSepA > lfSepB;
            const f32  lfSel     = lbGreater ? lfSepA : lfSepB;
            lbSeparatedToDistance = lbSeparatedToDistance && (lfSel >= lfDistance);
            lbNoneGreater         = lbNoneGreater && !lbGreater;
        }

        if (lbSeparatedToDistance)
        {
            if (lbNoneGreater)
            {
                // vcmpeqfp. mask == 0 (all lanes): no lane had sepA > sepB.
                lvNormal = lIntersection.normal;             // lvx128 v127, var_500
            }
            else
            {
                lvNormal = Negate(lIntersection.normal);     // vxor128 sign flip
            }
        }
    }

    // --- 6. one-sided-triangle re-validation --------------------------------
    if (arGP1.mVolumeType == GPInstance::TRIANGLE)
    {
        // sub_82BAA600(gp1) with v1 = +normal.
        if (!GPTriangleAcceptContactNormal(&arGP1, lvNormal))
        {
            return 0;
        }
    }
    if (arGP2.mVolumeType == GPInstance::TRIANGLE)
    {
        // sub_82BAA600(gp2) with v1 = -normal (vxor sign flip).
        if (!GPTriangleAcceptContactNormal(&arGP2, Negate(lvNormal)))
        {
            return 0;
        }
    }

    // --- 7. emit the result --------------------------------------------------
    arResult.volumeTag1 = arGP1.mVolumeTag;      // stw 0x84(r31) -> 0x00(r29)
    arResult.volumeTag2 = arGP2.mVolumeTag;      // stw 0x84(r30) -> 0x04(r29)
    arResult.userTag1   = arGP1.mUserTag;        // stw 0x88(r31) -> 0x08(r29)
    arResult.userTag2   = arGP2.mUserTag;        // stw 0x88(r30) -> 0x0C(r29)
    arResult.normal     = Negate(lvNormal);      // stvx128 -normal -> 0x20(r29)
    arResult.numPoints  = static_cast<u32>(lIntersection.m_numpts);   // stw -> 0x10(r29)

    // Fatness push-out vectors: lvlx/vspltw broadcast each instance's fatness,
    // then vmulfp128 by the (un-negated) final normal -- all four lanes.
    const Vec4 lvOffset1 = Scale(lvNormal, arGP1.mFatness);
    const Vec4 lvOffset2 = Scale(lvNormal, arGP2.mFatness);

    // Pair loop (skipped entirely when the count is 0; the bound is re-read
    // from the result block each iteration, as the asm's lwz 0x10(r29) does).
    for (u32 luPoint = 0; luPoint < arResult.numPoints; ++luPoint)
    {
        GPInstance::ContactPoints::PointPair& lrPair = arResult.pointPairs[luPoint];

        // Store order per iteration: p2 (+0x40 + 0x20*i) first, then
        // p1 (+0x30 + 0x20*i).
        lrPair.p2 = Sub(lIntersection.m_ptsOn2[luPoint], lvOffset2);
        lrPair.p1 = Add(lIntersection.m_ptsOn1[luPoint], lvOffset1);
    }

    // lwz r3, 0x10(r29)
    return arResult.numPoints;
}

} // namespace collision
} // namespace rw
