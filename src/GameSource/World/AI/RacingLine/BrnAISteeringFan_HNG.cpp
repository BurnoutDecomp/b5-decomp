#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine + SectionData
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"            // HardNoGoMap
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator + the gates

#include <cmath>    // std::sqrt (the vrsqrtefp + two-Newton-step length in FanIntersectsEdge)

// BrnAI::SteeringFan -- the HARD-NO-GO / ROUTE-EDGE contributors, partfile 3 of the weighting
// half. Three functions, all read off the X360 ARTIST image:
//
//   IncludeHardNoGo              @0x82779D98 (284 instr, IDA export)  rows eFan_AvoidHNG(1) /
//                                                                     eFan_ExitHNG(2) /
//                                                                     eFan_FavourHNGDanger(3)
//   FanIntersectsEdge            @0x8277A208 ( 92 instr, IDA export)  pure 2D geometry
//   IncludeRouteEdgeIntersection @0x8277A378 (136 instr, NO IDA EXPORT -- disassembled straight
//                                             out of the image, 0x8277A378..0x8277A590; the next
//                                             symbol is RouteRequestManager::GetFleeVector
//                                             @0x8277A598)            row eFan_AvoidEdges(6)
//
// The parked `(void)` stubs for these three in BrnAISteeringFan_Weightings.cpp are deleted at
// mount time; this file is their only body.
//
// REGISTER MAPS (from the asm, not the pseudocode -- Hex-Rays renders both entry points as
// `int __fastcall f(int, int, int, ..., double a9..a16)` and hides the VMX Vector2 arguments):
//   IncludeHardNoGo              0x82779DAC..DB8: r24 = this, r27/r4 = lpRacingLineGenerator,
//                                r29/r5 = lpRacingLine.
//   IncludeRouteEdgeIntersection 0x8277A38C..398: r24 = this, r25/r4 = lpRacingLineGenerator,
//                                r28/r5 = lpRacingLine.
//   FanIntersectsEdge            0x8277A208..228: r3 = this (NEVER READ), r4 = lpEdge,
//                                r5 = liIndex, v1 = lA, v2 = lB (both spilled to arg_30/arg_40).
// Both Include* take (RacingLineGenerator*, RacingLine*) with the GENERATOR first -- the console
// then calls the generator with r3 = generator, r4 = racingline, which is what pins the order.
//
// RECOVERED CONSTANTS (image bytes, big-endian, file offset = VA - 0x82000000):
//   flt_82F302B8 = 30.0f     the route-edge intersection range (IncludeRouteEdgeIntersection)
//   flt_82F302D0 =  0.0625f  the hard-no-go distance margin    (IncludeHardNoGo)
//   flt_82F302D4 =  0.125f   the hard-no-go danger threshold   (IncludeHardNoGo)
//   flt_8204F664 = +FLT_MAX (0x7F7FFFFF) / flt_82035570 = -FLT_MAX (0xFF7FFFFF)  min/max seeds
//   flt_820C4168 = 0.5f, flt_82001C98 = 1.0f, flt_82001CC0 = 0.0f, flt_820037C8 = -1.0f
//   flt_820C3B70 = +1.1920929e-07f, flt_82002514 = -1.1920929e-07f  (the parallel-edge epsilon)
// None of these lives in the zeroed 0x8300xxxx dynamic-initialiser bank; every one reads
// non-zero straight out of the static image.

namespace BrnAI
{
namespace
{
    // DWARF BrnAICar.cpp:49 `const uint16_t KU_INVALID_SECTION_INDEX = 32767`. Both Include*
    // bodies compare the section id against 0x7FFF as a signed 32-bit value (`cmpwi cr6, rN,
    // 0x7FFF`), so it is spelled s32 here; same value AICar.h already carries.
    const s32 KI_INVALID_SECTION_INDEX = 0x7FFF;

    // IncludeHardNoGo @0x82779F00 / @0x82779F3C -- flt_82F302D0 / flt_82F302D4.
    const f32 KF_HNG_EDGE_MARGIN   = 0.0625f;
    const f32 KF_HNG_DANGER_MARGIN = 0.125f;

    // IncludeHardNoGo @0x82779F98 / @0x82779FA4 -- flt_8204F664 / flt_82035570, the seeds of the
    // eFan_ExitHNG min/max scan. The +FLT_MAX seed is compared against again at 0x8277A0D8 to
    // detect "the row was entirely zero", so it must be this exact value.
    const f32 KF_FAN_FLOAT_MAX = 3.4028235e38f;

    // IncludeRouteEdgeIntersection @0x8277A4C4 -- flt_82F302B8. An edge hit at this distance or
    // beyond contributes nothing.
    const f32 KF_ROUTE_EDGE_RANGE = 30.0f;

    // IncludeRouteEdgeIntersection: `cmpwi cr6, r30, 0x30` @0x8277A480 caps the gather loop at
    // three 16-byte entries; the tail append at 0x8277A490 can add a fourth, which is why both
    // console stack arrays (r1+0x60 and r1+0xA0) are 0x40 bytes.
    const s32 KI_ROUTE_EDGE_SECTIONS = 3;
    const s32 KI_ROUTE_EDGE_POINTS   = KI_ROUTE_EDGE_SECTIONS + 1;

    // FanIntersectsEdge @0x8277A258 / @0x8277A270 -- flt_820C3B70 / flt_82002514 (FLT_EPSILON).
    const f32 KF_FAN_EDGE_PARALLEL_EPSILON = 1.1920929e-07f;

    // FanIntersectsEdge @0x8277A368 -- flt_820037C8. The "this ray does not cross this edge in
    // front of its target" answer; IncludeRouteEdgeIntersection tests results against it.
    const f32 KF_FAN_EDGE_NO_INTERSECTION = -1.0f;
}

// ============================================================================================
// [FLAG PC bring-up] THE TWO CONTRIBUTORS THAT NEED THE GENERATOR QUERY STACK ARE GATED.
// IncludeHardNoGo and the gather half of IncludeRouteEdgeIntersection call
// RacingLineGenerator::GetLocalSectionID @0x82776280, GetNearSectionID @0x827765A8 and
// GetSectionPointer @0x827655D0. As this file was written the first two are NOT DECLARED in this
// tree's BrnRacingLineGenerator.h at all and the third is declared `private`, so this TU cannot
// name them and still compile; that header belongs to another lane, and the three exact
// declarations needed are listed in this lane's report under `header_requests`.
// (The HardNoGoMap half -- IsReady, DistanceToHardNoGoEdge @0x82777D60, GetPreviousLeft /
// GetPreviousRight / GetCurrentLeft / GetCurrentRight, all four of them console-inlined and so
// exportless -- IS already present in BrnHardNoGoMap.h and needs nothing.)
// The gate defaults to the existing BRN_AI_RACINGLINE_STACK_PRESENT, so the moment that flips to
// 1 -- i.e. the moment the generator query half lands -- these bodies come alive with it.
// The #else arms are NOT invented: they are the console's own "GetLocalSectionID returned
// KI_INVALID_SECTION_INDEX" arms (0x82779DF4 zeroes the three HNG rows and returns; 0x8277A3D8
// zeroes the route-edge row and returns), which is exactly the answer that holds while no
// section can be resolved at all.
// FanIntersectsEdge is NOT gated -- it is self-contained 2D geometry and is live now.
// DELETE-WHEN the header_requests land and the gate goes to 1.
// ============================================================================================
#ifndef BRN_AI_STEERINGFAN_HNG_PRESENT
#define BRN_AI_STEERINGFAN_HNG_PRESENT BRN_AI_RACINGLINE_STACK_PRESENT
#endif

// ============================================================================================
// IncludeHardNoGo @0x82779D98   (this r3/r24, lpRacingLineGenerator r4/r27, lpRacingLine r5/r29)
// Rows eFan_AvoidHNG (this+0x3F8), eFan_ExitHNG (this+0x43C), eFan_FavourHNGDanger (this+0x480);
// mfWeighting is at this+0x3B4 with a 0x44-byte row stride, so those are rows 1, 2 and 3.
//
//   0x82779DB8  !lpRacingLine->mbIsInitialised (racingline+0xBD0) -> return, rows UNTOUCHED.
//   0x82779DC4  liSection = lpRacingLine->miLastKnownSectionID (racingline+0xC00).
//   0x82779DCC  only when that is KI_INVALID_SECTION_INDEX:
//   0x82779DE4     liSection = GetLocalSectionID(lpRacingLine, mFanOrigin2D (this+0x350, loaded
//                              by `lvx128 v1, r24, 0x350`), KI_INVALID_SECTION_INDEX)
//   0x82779DF4     still invalid -> zero ALL THREE rows (three unrolled 17-word `stw 0` loops at
//                              +0x3F8 / +0x43C / +0x480) and return.
//   0x82779E58     otherwise write it back to miLastKnownSectionID.
//   0x82779E80  per fan step i (r26 = 17, r28 = mHNGTarget[i] = this+0x110+16i, r31 = &row3[i]):
//     0x82779E90  liSection = GetNearSectionID(lpRacingLine, mHNGTarget[i], liSection)
//     0x82779EA0  invalid -> row1[i] = 0, row2[i] = 1 (f30 == flt_82001C98), row3[i] = 0, and
//                 liSection is RE-SEEDED from miLastKnownSectionID for the next step.
//     0x82779EC0  else lpSection = GetSectionPointer(lpRacingLine, liSection)
//     0x82779EC4  !lpSection->mHardNoGoMap.mbReady (section+0x98 == map+0x48) -> same 0/1/0 row.
//     0x82779EEC  else lbInside = mHardNoGoMap (section+0x50)
//                                 .DistanceToHardNoGoEdge(mHNGTarget[i], lfDistance)
//     0x82779EFC    !lbInside: lfDistance -= 0.0625; if it went negative, clamp it to 0 AND flip
//                              lbInside to TRUE (`li r3, 1` @0x82779F18 -- the console really does
//                              reclassify the point as inside here).
//     0x82779F28    row3 (FavourHNGDanger) = lbInside ? 0 : ((lfDistance - 0.125 < 0) ? 0
//                                                          : 1.0 - (lfDistance - 0.125))
//                   -- NOT clamped on the far side: 1.125 - d goes negative for d > 1.125 and the
//                   console keeps it.
//     0x82779F5C    lbInside ? (row2 = lfDistance, row1 = 0) : (row1 = lfDistance, row2 = 0)
//                   i.e. row eFan_AvoidHNG carries the distance while OUTSIDE the map and row
//                   eFan_ExitHNG carries it while INSIDE.
//   0x82779F8C  min/max over the NON-ZERO entries of row eFan_ExitHNG ONLY (unrolled 8+8+1;
//               f0 seeded +FLT_MAX, f13 seeded -FLT_MAX, zeros skipped by `fcmpu ... beq`).
//   0x8277A0D0  min == max, or min still +FLT_MAX (nothing non-zero) -> leave the row alone.
//   0x8277A0E0  else rescale every non-zero entry of that row into [0.5, 1.0]:
//                    v = (v - min) * ((1.0 / (max - min)) * 0.5) + 0.5
//               (`fdivs f12, f30(1.0), f13` then `fmuls f12, f12, f13(0.5)` then
//                `fmadds f11, f11, f12, f13(0.5)`). Rows 1 and 3 are NOT rescaled.
// ============================================================================================
void SteeringFan::IncludeHardNoGo(RacingLineGenerator* lpRacingLineGenerator,
                                  RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return;                                    // 0x82779DC0 -- rows keep their old values

#if BRN_AI_STEERINGFAN_HNG_PRESENT
    s32 liSection = lpRacingLine->miLastKnownSectionID;
    if (liSection == KI_INVALID_SECTION_INDEX)
    {
        liSection = lpRacingLineGenerator->GetLocalSectionID(lpRacingLine, mFanOrigin2D,
                                                             KI_INVALID_SECTION_INDEX);
        if (liSection == KI_INVALID_SECTION_INDEX)
        {
            for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            {
                mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
                mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
                mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            }
            return;
        }
        lpRacingLine->miLastKnownSectionID = liSection;
    }

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        liSection = lpRacingLineGenerator->GetNearSectionID(lpRacingLine, mHNGTarget[liStep],
                                                            liSection);
        if (liSection == KI_INVALID_SECTION_INDEX)
        {
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
            mfWeighting[eFan_ExitHNG][liStep]         = 1.0f;
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            liSection = lpRacingLine->miLastKnownSectionID;      // 0x82779EAC
            continue;
        }

        SectionData* lpSection = lpRacingLineGenerator->GetSectionPointer(lpRacingLine, liSection);
        if (!lpSection->mHardNoGoMap.IsReady())
        {
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
            mfWeighting[eFan_ExitHNG][liStep]         = 1.0f;
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
            continue;
        }

        f32  lfDistance       = 0.0f;
        bool lbInsideHardNoGo = lpSection->mHardNoGoMap.DistanceToHardNoGoEdge(mHNGTarget[liStep],
                                                                               lfDistance);
        if (!lbInsideHardNoGo)
        {
            lfDistance -= KF_HNG_EDGE_MARGIN;                    // 0x82779F04
            if (lfDistance < 0.0f)
            {
                lfDistance       = 0.0f;                         // 0x82779F14
                lbInsideHardNoGo = true;                         // 0x82779F18 `li r3, 1`
            }
        }

        if (lbInsideHardNoGo)
        {
            mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;    // 0x82779F34
            mfWeighting[eFan_ExitHNG][liStep]         = lfDistance;
            mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
        }
        else
        {
            const f32 lfOverDanger = lfDistance - KF_HNG_DANGER_MARGIN;
            mfWeighting[eFan_FavourHNGDanger][liStep] =
                (lfOverDanger < 0.0f) ? 0.0f : (1.0f - lfOverDanger);
            mfWeighting[eFan_AvoidHNG][liStep]        = lfDistance;
            mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
        }
    }

    // 0x82779F8C..0x8277A0CC -- min/max over the non-zero entries of eFan_ExitHNG only. The
    // console unrolls the 17 steps as 8 + 8 + 1; `fsel f0, (v - min), f0, v` keeps the running
    // minimum and `fsel f13, (v - max), v, f13` the running maximum.
    f32 lfMinimum =  KF_FAN_FLOAT_MAX;
    f32 lfMaximum = -KF_FAN_FLOAT_MAX;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfValue = mfWeighting[eFan_ExitHNG][liStep];
        if (lfValue == 0.0f)
            continue;
        if (lfValue < lfMinimum) lfMinimum = lfValue;
        if (lfValue > lfMaximum) lfMaximum = lfValue;
    }

    // 0x8277A0D0 / 0x8277A0D8 -- a flat row (min == max) and an all-zero row (min still the
    // +FLT_MAX seed) both leave the row exactly as it is.
    if (lfMinimum == lfMaximum || lfMinimum == KF_FAN_FLOAT_MAX)
        return;

    const f32 lfScale = (1.0f / (lfMaximum - lfMinimum)) * 0.5f;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        if (mfWeighting[eFan_ExitHNG][liStep] == 0.0f)
            continue;
        mfWeighting[eFan_ExitHNG][liStep] =
            (mfWeighting[eFan_ExitHNG][liStep] - lfMinimum) * lfScale + 0.5f;
    }
#else
    // The console's 0x82779DF4 arm: no section can be resolved, so all three rows read zero.
    (void)lpRacingLineGenerator;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        mfWeighting[eFan_AvoidHNG][liStep]        = 0.0f;
        mfWeighting[eFan_ExitHNG][liStep]         = 0.0f;
        mfWeighting[eFan_FavourHNGDanger][liStep] = 0.0f;
    }
#endif
}

// ============================================================================================
// FanIntersectsEdge @0x8277A208   (this r3 -- NEVER READ, lpEdge r4, liIndex r5, lA v1, lB v2)
//
// The 2D segment/segment test the route-edge contributor runs per fan ray. lpEdge is a polyline
// of 16-byte Vector2s; the edge tested is lpEdge[liIndex] -> lpEdge[liIndex + 1] (`slwi r11,
// r5, 4` @0x8277A224 then the 0x00/0x04 and 0x10/0x14 loads). The ray is lA -> lB.
//
//   0x8277A230  D  = lB - lA            (f11 = D.x, f12 = D.y)
//   0x8277A244  E  = e1 - e0            (f6  = E.x, f5  = E.y)
//   0x8277A25C  denominator = D.y*E.x - D.x*E.y      (`fmuls f0, f11, f5` then
//                                                     `fmsubs f0, f12, f6, f0`)
//   0x8277A264  |denominator| <= FLT_EPSILON -> parallel -> return -1.0. The console spells this
//               as `bgt` on +eps then `bge` on -eps, so a NaN denominator ALSO returns -1.0; the
//               !(>) / !(<) form below reproduces that exactly.
//   0x8277A290  R  = e0 - lA            (f9 = R.x, f10 = R.y)
//   0x8277A2A8  lfEdgeParam = (R.y*D.x - R.x*D.y) / denominator   -- the position along the edge
//   0x8277A2B8  outside [0, 1] -> return -1.0
//   0x8277A2C8  lfRayParam  = (R.y*E.x - R.x*E.y) / denominator   -- the position along the ray
//   0x8277A2D4  < 1.0 -> return -1.0, i.e. the crossing must be AT OR BEYOND lB (the fan target),
//               never between the car and its target.
//   0x8277A2E4..0x8277A360  return lfRayParam * |D|. The console builds |D| with vrsqrtefp plus
//               two Newton-Raphson refinements and a `vcmpeqfp` / `vsel` that forces the length
//               to 0 when D.x*D.x + D.y*D.y is exactly 0; std::sqrt plus that same guard is the
//               semantic equal. The result is therefore a DISTANCE in metres from lA.
// ============================================================================================
f32 SteeringFan::FanIntersectsEdge(Vector2* lpEdge, s32 liIndex, Vector2 lA, Vector2 lB)
{
    const f32 lfDirectionX = lB.x - lA.x;
    const f32 lfDirectionY = lB.y - lA.y;

    const f32 lfEdgeX = lpEdge[liIndex + 1].x - lpEdge[liIndex].x;
    const f32 lfEdgeY = lpEdge[liIndex + 1].y - lpEdge[liIndex].y;

    const f32 lfDenominator = lfDirectionY * lfEdgeX - lfDirectionX * lfEdgeY;
    if (!(lfDenominator >  KF_FAN_EDGE_PARALLEL_EPSILON) &&
        !(lfDenominator < -KF_FAN_EDGE_PARALLEL_EPSILON))
        return KF_FAN_EDGE_NO_INTERSECTION;                 // parallel (or NaN) -- no crossing

    const f32 lfToEdgeX    = lpEdge[liIndex].x - lA.x;
    const f32 lfToEdgeY    = lpEdge[liIndex].y - lA.y;
    const f32 lfReciprocal = 1.0f / lfDenominator;

    const f32 lfEdgeParam = (lfToEdgeY * lfDirectionX - lfToEdgeX * lfDirectionY) * lfReciprocal;
    if (lfEdgeParam < 0.0f || lfEdgeParam > 1.0f)
        return KF_FAN_EDGE_NO_INTERSECTION;                 // the crossing misses the segment

    const f32 lfRayParam = (lfToEdgeY * lfEdgeX - lfToEdgeX * lfEdgeY) * lfReciprocal;
    if (lfRayParam < 1.0f)
        return KF_FAN_EDGE_NO_INTERSECTION;                 // the crossing is short of lB

    const f32 lfLengthSq = lfDirectionX * lfDirectionX + lfDirectionY * lfDirectionY;
    const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : std::sqrt(lfLengthSq);
    return lfRayParam * lfLength;
}

// ============================================================================================
// IncludeRouteEdgeIntersection @0x8277A378  (this r3/r24, lpRacingLineGenerator r4/r25,
//                                            lpRacingLine r5/r28).  Row eFan_AvoidEdges(6) at
//                                            this+0x54C. NO IDA EXPORT -- the listing this body
//                                            was written from was decoded out of the image with
//                                            tools/re/ppcdis.py + tools/re/vmx128.py; the
//                                            function occupies 0x8277A378..0x8277A590 with a
//                                            zero pad word at 0x8277A594.
//
//   0x8277A398  !lpRacingLine->mbIsInitialised -> return, row UNTOUCHED.
//   0x8277A3B8  lCarPosition2D: `lvx128 v1, r0, r1+0x50` (an UNINITIALISED stack slot -- only
//               x/y are ever read downstream) then `lvx128 v0, r28, 0xB30` (lpRacingLine->mCarPos)
//               then `vrlimi128 v1, v0, 8, 0` (mask 8 == x lane, rotate 0 -> .x = mCarPos.x) and
//               `vrlimi128 v1, v0, 4, 1` (mask 4 == y lane, rotate 1 -> .y = mCarPos.z). This is
//               the same XZ flatten GenerateFanVectors @0x82779330/0x82779350 uses.
//   0x8277A3C8  liSection = GetLocalSectionID(lpRacingLine, lCarPosition2D,
//                                             lpRacingLine->miLastKnownSectionID)
//               -- unlike IncludeHardNoGo this call is UNCONDITIONAL, and the last known id is
//               the hint rather than KI_INVALID_SECTION_INDEX.
//   0x8277A3D8  invalid -> zero the 17 words at this+0x54C and return.
//   0x8277A408  lpRacingLine->miLastKnownSectionID = liSection.
//   0x8277A41C  GATHER, at most KI_ROUTE_EDGE_SECTIONS entries (`cmpwi cr6, r30, 0x30`):
//     0x8277A428    lpRacingLine->maSectionCache[liSection & 15].mCachedSectionIndex (the s16 at
//                   racingline + (id & 15)*0xB0 + 0xB8) != liSection -> stop. This inlined test is
//                   the DWARF's RacingLineGenerator::CacheUpToDate (BrnRacingLineGenerator.h:226)
//                   and it is exactly the condition GetSectionPointer @0x827655D0 would otherwise
//                   fire its StrStream assert on.
//     0x8277A444    lpSection = GetSectionPointer(lpRacingLine, liSection)
//     0x8277A448    !lpSection->mHardNoGoMap.mbReady -> stop (lpSection STAYS SET -- see below).
//     0x8277A454    `lvx128 v0, r3, 0x50` / `lvx128 v13, r3, 0x60` load the map's
//                   mCurrentAndPreviousLeft / mCurrentAndPreviousRight, and BOTH are then run
//                   through `vpermwi128 vX, vX, 0xBF`. 0xBF == (2,3,3,3), i.e. z/w -> x/y, which
//                   moves the PREVIOUS point into the Vector2 lanes: GetPreviousLeft() /
//                   GetPreviousRight() (DWARF BrnHardNoGoMap.h:139 / :142).
//                   (The permute immediate is split across the word: uimm = (b23<<7)|(b24<<6)|
//                   (b25<<5)|bits(b11..b15); calibrated against XMMatrixRotationX @0x8220352C and
//                   CreateLookAt @0x8220C72C, whose 0x63 is the yzxw cross-product swizzle.)
//   0x8277A488  AFTER the gather, if lpSection is non-null -- which it is whenever at least one
//               GetSectionPointer ran, INCLUDING the "map not ready" exit -- append that same
//               section's mCurrentAndPreviousLeft/Right with NO permute, i.e. GetCurrentLeft() /
//               GetCurrentRight(). That is what closes the polyline: entry(s0), entry(s1),
//               entry(s2), exit(s_last).
//   0x8277A4E4  per fan step i (r30 = 17, r7 = &row[i], r31 = &mTarget[i]):
//     0x8277A4E4    row[i] = 0.0 unconditionally, then walk the (liEdgeCount - 1) segments:
//     0x8277A504    lfLeft  = FanIntersectsEdge(laLeftEdgePoints,  liSegment, mFanOrigin2D,
//                                               mTarget[i])
//     0x8277A510    lfRight = FanIntersectsEdge(laRightEdgePoints, liSegment, mFanOrigin2D,
//                                               mTarget[i])  -- the second call reuses r3/r5/v1/v2
//                                               untouched, only r4 changes.
//     0x8277A514    both -1 -> next segment; otherwise take the nearer of the two hits and STOP
//                   scanning (the console falls straight through to the next fan step).
//     0x8277A548    lfRange = clamp(distance / 30.0, 0, 1)  (`fneg`+`fsel` low clamp,
//                   `fsubs`+`fsel` high clamp), then
//                   row[i] = (1 - lfRange)^2 * mTravelDirectionBias[i]   (this+0x54C + 0x264 ==
//                   this+0x7B0, mTravelDirectionBias).
//               Because FanIntersectsEdge only reports crossings at or beyond mTarget[i], the
//               distance is never below mfLookAheadRadius (10.0), so the row peaks at
//               (1 - 10/30)^2 == 0.444 and reaches 0 at 30 m. kfBias[eBiasMode_Race]
//               [eFan_AvoidEdges] == -200, so it is a strong penalty for rays that run into the
//               road edge close ahead.
// ============================================================================================
void SteeringFan::IncludeRouteEdgeIntersection(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
        return;                                    // 0x8277A3A0 -- row keeps its old values

#if BRN_AI_STEERINGFAN_HNG_PRESENT
    // 0x8277A3B8 -- the XZ flatten of lpRacingLine->mCarPos. The console leaves z/w as whatever
    // the stack slot held; nothing downstream reads them, so they are zeroed here.
    Vector2 lCarPosition2D;
    lCarPosition2D.x = lpRacingLine->mCarPos.x;
    lCarPosition2D.y = lpRacingLine->mCarPos.z;
    lCarPosition2D.z = 0.0f;
    lCarPosition2D.w = 0.0f;

    s32 liSection = lpRacingLineGenerator->GetLocalSectionID(lpRacingLine, lCarPosition2D,
                                                             lpRacingLine->miLastKnownSectionID);
    if (liSection == KI_INVALID_SECTION_INDEX)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;
        return;
    }
    lpRacingLine->miLastKnownSectionID = liSection;

    Vector2      laLeftEdgePoints[KI_ROUTE_EDGE_POINTS];
    Vector2      laRightEdgePoints[KI_ROUTE_EDGE_POINTS];
    s32          liEdgeCount = 0;
    SectionData* lpSection   = 0;

    for (s32 liSlot = 0; liSlot < KI_ROUTE_EDGE_SECTIONS; ++liSlot)
    {
        // The inlined RacingLineGenerator::CacheUpToDate (DWARF BrnRacingLineGenerator.h:226).
        const s32 liCacheSlot = liSection & (RacingLine::KI_SECTION_CACHE_COUNT - 1);
        if (static_cast<s32>(lpRacingLine->maSectionCache[liCacheSlot].mCachedSectionIndex)
                != liSection)
            break;

        lpSection = lpRacingLineGenerator->GetSectionPointer(lpRacingLine, liSection);
        if (!lpSection->mHardNoGoMap.IsReady())
            break;

        laLeftEdgePoints[liEdgeCount]  = lpSection->mHardNoGoMap.GetPreviousLeft();
        laRightEdgePoints[liEdgeCount] = lpSection->mHardNoGoMap.GetPreviousRight();
        ++liEdgeCount;
        ++liSection;
    }

    // 0x8277A488 -- close the polyline with the last resolved section's CURRENT edge points.
    // lpSection is only null when the very first cache test failed.
    if (lpSection != 0)
    {
        laLeftEdgePoints[liEdgeCount]  = lpSection->mHardNoGoMap.GetCurrentLeft();
        laRightEdgePoints[liEdgeCount] = lpSection->mHardNoGoMap.GetCurrentRight();
        ++liEdgeCount;
    }

    const s32 liSegmentCount = liEdgeCount - 1;

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;

        for (s32 liSegment = 0; liSegment < liSegmentCount; ++liSegment)
        {
            const f32 lfLeftHit  = FanIntersectsEdge(laLeftEdgePoints,  liSegment,
                                                     mFanOrigin2D, mTarget[liStep]);
            const f32 lfRightHit = FanIntersectsEdge(laRightEdgePoints, liSegment,
                                                     mFanOrigin2D, mTarget[liStep]);

            f32 lfDistance;
            if (lfLeftHit == KF_FAN_EDGE_NO_INTERSECTION)
            {
                if (lfRightHit == KF_FAN_EDGE_NO_INTERSECTION)
                    continue;                                    // 0x8277A524 -- next segment
                lfDistance = lfRightHit;
            }
            else if (lfRightHit == KF_FAN_EDGE_NO_INTERSECTION)
            {
                lfDistance = lfLeftHit;                          // 0x8277A538
            }
            else
            {
                lfDistance = (lfLeftHit < lfRightHit) ? lfLeftHit : lfRightHit;
            }

            f32 lfRange = lfDistance / KF_ROUTE_EDGE_RANGE;
            if (lfRange <= 0.0f) lfRange = 0.0f;                 // 0x8277A550 fneg + fsel
            if (lfRange >  1.0f) lfRange = 1.0f;                 // 0x8277A558 fsubs + fsel
            const f32 lfCloseness = 1.0f - lfRange;
            mfWeighting[eFan_AvoidEdges][liStep] =
                lfCloseness * lfCloseness * mTravelDirectionBias[liStep];
            break;                                               // the first hit wins
        }
    }
#else
    // The console's 0x8277A3D8 arm: no section can be resolved, so the row reads zero.
    (void)lpRacingLineGenerator;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        mfWeighting[eFan_AvoidEdges][liStep] = 0.0f;
#endif
}

}
