#include <xtl.h>
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/BrnAICar.h"                             // AICar getters
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles
#include "GameSource/World/AI/BrnAIUtils.h"                           // FindSignedAngleBetween2DVectors
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator + the two gates
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_TrafficConstants.h" // KF_GUESSED_MAX_SPEED (CalculateFanAngle)


#include "SDKs/XboxMath/XMVectorSinCos.h"  // XboxMath::XMVectorSinCos (GenerateFanVectors' inlined SinCos)

#include <cmath>    // std::fmaf (the fmadds / vmaddfp sites), std::fabs, std::sqrt

// BrnAI::SteeringFan -- partfile 2 of 2 for the weighting half (aiwave R6 lane). This TU owns the
// per-round-robin weighting pass AIDriver::DoRoundRobinWork @0x82796340 drives:
//
//   BODIED
//     GenerateFanVectors            @0x827792C0
//     CachePointAhead               @0x827913C8
//     IncludeCentreLineTracking     @0x82786BC8   (row eFan_SteerToCentre)
//     IncludeRouteParallelTracking  @0x82786DB8   (row eFan_DriveParallel)
//     IncludePreferCurrentDirection @0x827694A8   (row eFan_PreferCurrentDirection)
//     FindPlayerInTraffic           @0x82769510
//     FindVictimInTraffic           @0x82769580
//     UpdateWeightings              @0x82794600
//     CalculateFanAngle             @0x82768CB0   (no IDA export -- decoded from image bytes, aiwave2)
//   BODIED ELSEWHERE (aiwave2 2026-09-05 partfiles, see the trailer note at the end of this file)
//     BrnAISteeringFan_HNG.cpp        IncludeHardNoGo / IncludeRouteEdgeIntersection / FanIntersectsEdge
//     BrnAISteeringFan_Traffic.cpp    IncludeConstantBearing
//     BrnAISteeringFan_Aggression.cpp IncludeSmashIntoTarget / IncludeSmashIntoPlayer / IncludeSmashIntoNearbyAI /
//                                     FindNeabyAIInTraffic / IncludeDriveCloseToPlayer / IncludeDrift*Tracking
//
// SHARED REGISTER MAP for UpdateWeightings @0x82794600 (the console's, read off 0x82794614..30):
//   r3/r31 = this, r4/r26 = lpCar, r5/r27 = lpRacingLine, r6/r28 = lpRacingLineGenerator,
//   r7/r25 = lpNearbyTraffic, r8/r24 = leAggressionVictim.
// Every Include* below is called with a permutation of those; the per-function banners name it.

namespace BrnAI
{
namespace
{
    // ARTIST 82779328..58: replace Y with Z while retaining the other lanes.
    Vector2 To2DFan(Vector3 lVector)
    {
        Vector2 lOut;
        lOut.mV = __vrlimi(lVector.mV, lVector.mV, 4, 1);
        return lOut;
    }

    // ARTIST 82779370..B0, 82786CF4..D58, 82786EF8..F58.
    // Keep the estimate, two VMX refinements and the complete register.
    Vector2 Normalize2DFan(Vector2 lVector)
    {
        const __vector4 lSquares = __vmulfp(lVector.mV, lVector.mV);
        const __vector4 lLength = __vaddfp(__vspltw(lSquares, 0), __vspltw(lSquares, 1));
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lLength);
        __vector4 lSquare = __vmulfp(lEstimate, lEstimate);
        __vector4 lHalfEstimate = __vmulfp(lEstimate, lHalf);
        __vector4 lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        lSquare = __vmulfp(lEstimate, lEstimate);
        lHalfEstimate = __vmulfp(lEstimate, lHalf);
        lCorrection = __vnmsubfp(lLength, lSquare, lOne);
        lEstimate = __vmaddfp(lHalfEstimate, lCorrection, lEstimate);
        Vector2 lOut;
        lOut.mV = __vmulfp(lVector.mV, lEstimate);
        return lOut;
    }

    f32 Dot2DFan(const Vector2& lA, const Vector2& lB)
    {
        const __vector4 lProduct = __vmulfp(lA.mV, lB.mV);
        const __vector4 lSum = __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1));
        return lSum.x;
    }

    // flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON) -- the "is this delta vector zero" epsilon both
    // centre-line contributors test each component of the delta against before normalising.
    const f32 KF_FAN_TINY = 1.1920929e-07f;

    // The console's own look-ahead distance for the drift point cache: flt_82004D0C == 40.0
    // (CachePointAhead @0x82791440 `lfs f1, flt_82004D0C`).
    const f32 KF_FAN_POINT_AHEAD_DISTANCE = 40.0f;
}

// ========================================================================================
// CalculateFanAngle @0x82768CB0 -- NO IDA export; decoded from image bytes (tools/re/ppcdis.py,
// 38 instructions, 0x82768CB0..0x82768D44) by the conductor, aiwave2 2026-09-05.
//
//   bl AICar::GetSpeed                     -> f1
//   f0  = flt_82F30428  (1.3962634 = 80 deg)          KF_STEER_AT_LOW_SPEED   (DWARF :24)
//   f13 = flt_820C4168  (0.5)                          KF_STEER_AT_HIGH_SPEED  (DWARF :25)
//   f10 = f13 - f0
//   f11 = flt_820C4238  (15.0)                         look-ahead growth over the speed range
//   stfs flt_82F302C0 (15.0) -> +0x7FC                  mfLookAheadHNGRadius (every tick)
//   f13 = f1 / [0x8300D78C]                            KF_GUESSED_MAX_SPEED (DWARF :26; .bss,
//                                                       recovered by lane F2a = 100 mph)
//   fsel x2 -> saturate(f13) into [0, 1]
//   stfs (f10 * ratio + f0)        -> +0x800            mfFanAngle: 80 deg at rest -> 0.5 rad flat out
//   stfs (ratio * 15.0 + 10.0)     -> +0x7F8            mfLookAheadRadius: 10 m -> 25 m
// ========================================================================================
namespace
{
    const f32 KF_STEER_AT_LOW_SPEED    = 1.3962633609771729f;   // flt_82F30428 (80 deg)
    const f32 KF_STEER_AT_HIGH_SPEED   = 0.5f;                  // flt_820C4168
    const f32 KF_FAN_LOOK_AHEAD_BASE   = 10.0f;                 // flt_820C4150
    const f32 KF_FAN_LOOK_AHEAD_GROWTH = 15.0f;                 // flt_820C4238
    const f32 KF_FAN_LOOK_AHEAD_HNG    = 15.0f;                 // flt_82F302C0
}

void SteeringFan::CalculateFanAngle(AICar* lpCar)
{
    const f32 lfSpeed = lpCar->GetSpeed();

    mfLookAheadHNGRadius = KF_FAN_LOOK_AHEAD_HNG;                                   // stfs 0x7FC

    // The saturate is `fneg t,r ; fsel r,t,0.0,r` 0x82768D0C/0x82768D10 then `fsubs u,1.0,r ;
    // fsel r,u,r,1.0` 0x82768D18/0x82768D1C: fsel takes its THIRD operand on an unordered test, so
    // a NaN speed ratio is 1.0 (flat-out fan) -- spelt as the fsels (FX-AINAN2; the if/if kept NaN).
    f32 lfRatio = lfSpeed / KF_GUESSED_MAX_SPEED;                                    // fdivs
    lfRatio = (-lfRatio >= 0.0f) ? 0.0f : lfRatio;                                   // fsel vs 0.0
    lfRatio = ((1.0f - lfRatio) >= 0.0f) ? lfRatio : 1.0f;                           // fsel vs 1.0

    // Both lerps are ONE rounding (ROUNDING_RULE 3): 0x82768D20 `fmadds f0, f10, f13, f0` (f10 = high - low, the
    // fsubs at 0x82768CE4, rule 4; f13 = the ratio; f0 = low) and 0x82768D2C `fmadds f0, f13, f11, f0` (the
    // ratio, 15.0, 10.0). fmadds frD = frA*frC + frB == std::fmaf(frA, frC, frB).
    volatile f32 lfFanRange = KF_STEER_AT_HIGH_SPEED - KF_STEER_AT_LOW_SPEED;
    mfFanAngle        = lfFanRange * lfRatio + KF_STEER_AT_LOW_SPEED;                            // stfs 0x800
    mfLookAheadRadius = lfRatio * KF_FAN_LOOK_AHEAD_GROWTH + KF_FAN_LOOK_AHEAD_BASE; // stfs 0x7F8
}

// ========================================================================================
// GenerateFanVectors @0x827792C0
//
// Rebuild the 17 fan rays around the car for this tick.
//   r29 = this, r31 = lpCar.
//   0x827792F0 GetPosition(lpCar)         -> v127 ; stvx128 v127, r29, 0x340 -> mFanOrigin
//   0x82779304 GetUsefulDirection(lpCar)  -> v13  (XZ is flattened via the vrlimi128 pair)
//   0x8277933C mFanOrigin2D.xy = carPos.xz (vrlimi128 into the loaded this+0x350)
//   0x827793B4 f28 = FindSignedAngleBetween2DVectors(normalize2D(useful.xz), (0,1))
//              -- var_110/var_10C are stored 0.0 (flt_82001CC0) / 1.0 (flt_82001C98) to build
//              the (0,1) reference vector the angle is measured from.
//   loop i = 0..16 with lfT += mfReciprocalSteps (this+0x7F4, == 1/16):
//     0x82779474..90  angle = ((lfT - 0.5) * 2)^3 * mfFanAngle(this+0x800) + f28
//                     (f30 = flt_820C4168 == 0.5, f29 = flt_820C41F4 == 2.0)
//     0x82779498      CGS_ASSERT(i < 17, "Fan index out of range at ")
//     ...             XMVectorSinCos polynomial + vperm/vsldoi -> mUnitDirection[i] (this+0x220)
//     0x827796F4      mTarget[i]    = mFanOrigin2D + mUnitDirection[i] * mfLookAheadRadius
//                     (vmaddfp vD,vA,vB,vC == vA*vC + vB: vA = unit dir, vB = origin, vC = radius)
//     0x82779724      mHNGTarget[i] = mFanOrigin2D + mUnitDirection[i] * mfLookAheadHNGRadius
//   0x8277973C      mCentreTarget (this+0x330) = mTarget[8] (lvx this+0x80)
//
// ARTIST 827796C0..C8: vperm with bytes at 82CDA3C0
// {00010203,00010203,00010203,14151617} selects {sin,sin,sin,cos}.
// vsldoi by 8 bytes then stores {sin,cos,0,0}; there is no sine negation.
// This agrees with FindSignedAngleBetween2DVectors(A,(0,1)): sign(cross) = sign(A.x).
// Ray 8 follows the useful heading; ray 0 is left and ray 16 is right.
// ========================================================================================
void SteeringFan::GenerateFanVectors(AICar* lpCar)
{
    const Vector3 lCarPosition = lpCar->GetPosition();            // @0x8276B1F0
    const Vector3 lUseful      = lpCar->GetUsefulDirection();     // @0x82770028

    mFanOrigin      = lCarPosition;                               // stvx128 v127, r29, 0x340
    // ARTIST 82779340/54 updates only X and Y, retaining Z and W.
    mFanOrigin2D.mV = __vrlimi(mFanOrigin2D.mV, lCarPosition.mV, 8, 0);
    mFanOrigin2D.mV = __vrlimi(mFanOrigin2D.mV, lCarPosition.mV, 4, 1);
    const Vector2 lReference(0.0f, 1.0f);

    const Vector2 lCarDirection2DRaw = To2DFan(lUseful);

    const Vector2 lCarDirection2D = Normalize2DFan(lCarDirection2DRaw);
    const f32 lfBaseAngle = FindSignedAngleBetween2DVectors(lCarDirection2D, lReference);

    f32 lfT = 0.0f;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        // 0x82779474 fsubs, 0x82779480 fmuls 2.0, the cube as (x * x) * x (0x82779484 / 0x82779488), each rounded
        // (ROUNDING_RULE 4); then 0x8277948C `fmadds f0, f0, f13, f28` = cube * mfFanAngle + base, ONE rounding
        // (rule 3).
        const f32 lfInterp = (lfT - 0.5f) * 2.0f;
        volatile f32 lfSquared = lfInterp * lfInterp;
        volatile f32 lfCubed = lfSquared * lfInterp;
        const f32 lfAngle = lfCubed * mfFanAngle + lfBaseAngle;

        CGS_ASSERT(liStep < KI_FAN_STEPS, "Fan index out of range at ");

        // 0x8277955C..0x827796BC: the XDK's XMVectorSinCos, inlined (tables 0x82000BD0..0x82000C6F; sine in v13,
        // cosine in v0 at 0x827796C0). It is XboxMath::XMVectorSinCos bit for bit: the real words run on emu64
        // agree with it on 1500 angles (FX-GATE, crash parity 2026-09-25). std::sin / std::cos stood in before.
        f32 lfSin;
        f32 lfCos;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngle);

        const Vector2 lUnit(lfSin, lfCos);
        mUnitDirection[liStep] = lUnit;

        // 0x827796F4 / 0x82779724 `vmaddfp` (raw D,A,B,C = v13, v0, v13, v12 / v0, v0, v13, v12) = unit * radius +
        // origin, ONE rounding per lane (ROUNDING_RULE 3).
        const XMVECTOR lDirection = __lvx(&lUnit, 0);
        const XMVECTOR lOrigin = __lvx(&mFanOrigin2D, 0);
        __stvx(__vmaddfp(lDirection, XMVectorReplicate(mfLookAheadRadius), lOrigin), &mTarget[liStep], 0);
        __stvx(__vmaddfp(lDirection, XMVectorReplicate(mfLookAheadHNGRadius), lOrigin), &mHNGTarget[liStep], 0);

        lfT += mfReciprocalSteps;
    }

    mCentreTarget = mTarget[KI_FAN_STEPS / 2];                    // lvx this+0x80 -> stvx this+0x330
}

// ========================================================================================
// CachePointAhead @0x827913C8   (this r3, lpRacingLineGenerator r4, lpRacingLine r5)
//
//   0x827913E0  if (!lpRacingLine->mbIsInitialised (racingline+0xBD0))
//                   { mbPointAheadKnown = false; mbCentreHereKnown = false; return; }
//   0x827913EC  if (kfBias[mode][eFan_DriftFinalLocation] != 0 ||
//                   kfBias[mode][eFan_DriftFinalDirection] != 0)
//                   mbPointAheadKnown = GetPointFarAhead(lpRacingLine, 40.0 (flt_82004D0C),
//                                                        mFanOrigin2D, mCentreFarAhead,
//                                                        mCentreAheadFarAhead);
//   0x8279144C  mbCentreHereKnown = GetCentreCentreLineHere(lpRacingLine, mCentreTarget,
//                                                           mCentreTarget, mCentreHere,
//                                                           mCentreAhead);
//               (the console loads v2 from this+0x330 and then `vmr v1, v2` -- BOTH vector
//                arguments are mCentreTarget; that is not a transcription slip.)
// ========================================================================================
void SteeringFan::CachePointAhead(RacingLineGenerator* lpRacingLineGenerator,
                                  RacingLine* lpRacingLine)
{
    if (!lpRacingLine->mbIsInitialised)
    {
        mbPointAheadKnown = false;      // stb 0, 0x808
        mbCentreHereKnown = false;      // stb 0, 0x809
        return;
    }

    if (kfBias[meBiasMode][eFan_DriftFinalLocation]  != 0.0f ||
        kfBias[meBiasMode][eFan_DriftFinalDirection] != 0.0f)
    {
#if BRN_AI_RACINGLINE_STACK_PRESENT
        mbPointAheadKnown = lpRacingLineGenerator->GetPointFarAhead(lpRacingLine,
                                                                    KF_FAN_POINT_AHEAD_DISTANCE,
                                                                    mFanOrigin2D,
                                                                    mCentreFarAhead,
                                                                    mCentreAheadFarAhead);
#else
#endif
    }

#if BRN_AI_RACINGLINE_STACK_PRESENT
    mbCentreHereKnown = lpRacingLineGenerator->GetCentreCentreLineHere(lpRacingLine,
                                                                       mCentreTarget,
                                                                       mCentreTarget,
                                                                       mCentreHere,
                                                                       mCentreAhead);
#else
    mbCentreHereKnown = false;
#endif
}

// ========================================================================================
// IncludeCentreLineTracking @0x82786BC8   (this r3, lpRacingLineGenerator r4 UNUSED, lpRacingLine r5)
// Row eFan_SteerToCentre (this+0x3B4 == mfWeighting[0]).
//
//   0x82786BC8  racing line not initialised (racingline+0xBD0) -> zero the row, return.
//   0x82786BF4  !mbCentreHereKnown (this+0x809)                -> zero the row, return.
//   0x82786C20  delta = mCentreHere (this+0x390) - Flatten(mFanOrigin) (this+0x340)
//   0x82786C64..CF0  if |delta.x| <= FLT_EPSILON && |delta.y| <= FLT_EPSILON -> return with the
//               row LEFT AS IT WAS (`bnelr`, not a zero-then-return -- unlike the two arms above).
//   0x82786D00  f13 = lpRacingLine->mfCentreLineAhead (racingline+0xC04)
//               f12 = lpRacingLine->mfCentreLineAheadRecip (racingline+0xC08)
//   loop i: dot = dot2D(mUnitDirection[i], normalize2D(delta));
//           row[i] = (dot > mfCentreLineAhead) ? -((dot - mfCentreLineAhead) * recip) : 0.0
//           (the `fneg` at 0x82786D8C is the console's -- the weight is NEGATIVE, so with
//            kfBias[eBiasMode_Race][eFan_SteerToCentre] == +20 the fan PENALISES rays that point
//            more directly at the centre line than the mfCentreLineAhead threshold.)
// ========================================================================================
void SteeringFan::IncludeCentreLineTracking(RacingLineGenerator* lpRacingLineGenerator,
                                            RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised || !mbCentreHereKnown)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_SteerToCentre][liStep] = 0.0f;
        return;
    }

    Vector2 lDelta;
    lDelta.mV.x = mCentreHere.mV.x - mFanOrigin.mV.x;
    lDelta.mV.y = mCentreHere.mV.y - mFanOrigin.mV.z; // ARTIST 82786C4C: world Z into planar Y
    lDelta.mV.z = 0.0f;
    lDelta.mV.w = 0.0f;

    // IsZero is a per-lane non-dot vcmpgtfp |d| > eps gathered by vperm + `cmpwi ; bne/beq`
    // (0x82786C88..0x82786CF0): a NaN lane is never "greater", so a NaN delta counts as zero and
    // returns here (FX-AINAN2; `fabs <= tiny` called it non-zero and wrote a 0.0 row).
    if (!(std::fabs(lDelta.mV.x) > KF_FAN_TINY) && !(std::fabs(lDelta.mV.y) > KF_FAN_TINY))
        return;                                            // bnelr: the row keeps its old values

    const f32 lfAhead      = lpRacingLine->mfCentreLineAhead;
    const f32 lfAheadRecip = lpRacingLine->mfCentreLineAheadRecip;
    const Vector2 lToCentre = Normalize2DFan(lDelta);

    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfDot = Dot2DFan(mUnitDirection[liStep], lToCentre);
        if (lfDot > lfAhead)
            mfWeighting[eFan_SteerToCentre][liStep] = -((lfDot - lfAhead) * lfAheadRecip);
        else
            mfWeighting[eFan_SteerToCentre][liStep] = 0.0f;
    }


}

// ========================================================================================
// IncludeRouteParallelTracking @0x82786DB8  (this r3, lpRacingLineGenerator r4 UNUSED, lpRacingLine r5)
// Row eFan_DriveParallel (this+0x590 == mfWeighting[7]). NO external calls at all.
//
//   0x82786DB8  racing line not initialised -> zero the row, return.
//   0x82786DE4  !mbCentreHereKnown          -> zero the row, return.
//   0x82786E10  delta = mCentreHere (this+0x390) - mCentreAhead (this+0x3A0)
//               (vsubfp v13, v12, v13 with v12 = +0x390 and v13 = +0x3A0), i.e. it points BACK
//               along the road.
//   0x82786E58..ED4  if |delta.x| <= FLT_EPSILON && |delta.y| <= FLT_EPSILON -> zero the row, return.
//   loop i:   d = dot2D(mUnitDirection[i], normalize2D(delta));
//             d = max(d, -1.0)  (flt_820037C8 == -1.0, fsubs+fsel at 0x82786F80/84)
//             d = min(d,  1.0)  (flt_82001C98 ==  1.0, fsubs+fsel at 0x82786F90/94)
//             row[i] = (d + 1.0) * 0.5      (flt_820C4168 == 0.5)
//   So the row is 1 for a ray pointing straight BACK down the road and 0 for one pointing along
//   it; with kfBias[eBiasMode_Race][eFan_DriveParallel] == -200 that is a large penalty for
//   turning round.
// ========================================================================================
void SteeringFan::IncludeRouteParallelTracking(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised || !mbCentreHereKnown)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_DriveParallel][liStep] = 0.0f;
        return;
    }

    Vector2 lDelta;
    lDelta.mV = __vsubfp(mCentreHere.mV, mCentreAhead.mV); // ARTIST 82786E38

    // IsZero: the same vperm-gathered vcmpgtfp (0x82786E6C / 0x82786EA4) -- a NaN delta counts as
    // zero and zeroes the row (FX-AINAN2; `fabs <= tiny` called it non-zero).
    if (!(std::fabs(lDelta.mV.x) > KF_FAN_TINY) && !(std::fabs(lDelta.mV.y) > KF_FAN_TINY))
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_DriveParallel][liStep] = 0.0f;
        return;
    }

    const Vector2 lBackAlongRoad = Normalize2DFan(lDelta);
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        // Clamp(d, -1, 1): `fsubs t,-1.0,d ; fsel d,t,-1.0,d` 0x82786F80/0x82786F84 then
        // `fsubs u,1.0,d ; fsel d,u,d,1.0` 0x82786F90/0x82786F94 -- a NaN dot is 1.0 (FX-AINAN2).
        f32 lfDot = Dot2DFan(mUnitDirection[liStep], lBackAlongRoad);
        lfDot = ((-1.0f - lfDot) >= 0.0f) ? -1.0f : lfDot;
        lfDot = ((1.0f - lfDot) >= 0.0f) ? lfDot : 1.0f;
        mfWeighting[eFan_DriveParallel][liStep] = (lfDot + 1.0f) * 0.5f;
    }
}

// ========================================================================================
// IncludePreferCurrentDirection @0x827694A8   (this r3 only -- a leaf, no calls, no branches)
// Row eFan_PreferCurrentDirection (this+0x5D4 == mfWeighting[8]).
//
//   flt_82046E00 == 0.0625, flt_820C4168 == 0.5, flt_820C41F4 == 2.0, flt_82001C98 == 1.0.
//   row[i] = (1 - |((i * 0.0625) - 0.5) * 2|) ^ 2
// i.e. a squared tent peaking at 1 on the centre ray and falling to 0 at both fan edges. Note the
// console uses the literal 0.0625 here, NOT mfReciprocalSteps.
// (kfBias's eFan_PreferCurrentDirection column is 0.0 in all 10 bias modes in this build, so
// UpdateWeightings never actually calls it -- the body is here because the guard is the console's,
// not ours, and a later build/mode could re-enable the column.)
// ========================================================================================
void SteeringFan::IncludePreferCurrentDirection()
{
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfV      = std::fabs(((static_cast<f32>(liStep) * 0.0625f) - 0.5f) * 2.0f);
        const f32 lfWeight = 1.0f - lfV;
        mfWeighting[eFan_PreferCurrentDirection][liStep] = lfWeight * lfWeight;
    }
}

// ========================================================================================
// FindPlayerInTraffic @0x82769510   (this r3 UNUSED, lpNearbyTraffic r4)
// First entry of the avoidance list whose mType (entry+0x20) is E_NEARBY_PLAYER (2); NULL when the
// list is empty or holds no player. The console walks the NearbyVehicles object itself with stride
// 0x70 from +0x00, which is mVehicle[i] -- named here, never offset-hacked.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindPlayerInTraffic(const NearbyVehicles* lpNearbyTraffic)
{
    const s32 liCount = lpNearbyTraffic->GetCount();       // bl NearbyVehicles::GetCount
    for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
    {
        if (lpNearbyTraffic->mVehicle[liEntry].mType == E_NEARBY_PLAYER)
            return &lpNearbyTraffic->mVehicle[liEntry];
    }
    return 0;
}

// ========================================================================================
// FindVictimInTraffic @0x82769580   (this r3 UNUSED, lpNearbyTraffic r4, leAggressionVictim r5)
// First entry whose meGlobalRaceCarIndex (entry+0x24) equals the aggression victim; NULL if none.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindVictimInTraffic(const NearbyVehicles* lpNearbyTraffic,
                                                      EGlobalRaceCarIndex leAggressionVictim)
{
    const s32 liCount = lpNearbyTraffic->GetCount();
    for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
    {
        if (lpNearbyTraffic->mVehicle[liEntry].meGlobalRaceCarIndex == leAggressionVictim)
            return &lpNearbyTraffic->mVehicle[liEntry];
    }
    return 0;
}

// ========================================================================================
// UpdateWeightings @0x82794600 -- the whole round-robin weighting pass.
//
// Refresh the fan geometry, cache the racing-line samples, then run every contributor whose
// kfBias column is non-zero for the current bias mode. The kfBias tests ARE the console's own
// dispatch (`lwz r11,0x3B0(r31)` / `mulli r11,r11,0x38` / `lfsx` / `fcmpu` against
// flt_82001CC0 == 0.0 before each `bl`), not a host optimisation -- reproducing them is what keeps
// the parked contributors below unreachable in the modes whose column is zero.
//
// The six CgsDev::PerfMonCpu::StartMonitor/StopMonitor pairs around the blocks (ids dword_82F302D8
// .. 0x82F302F0, the miSteeringFanPM[7] the DWARF declares) are the same presentation-only
// profiling SteeringFan::Prepare already declines to register on this host; not reproduced.
// ========================================================================================
void SteeringFan::UpdateWeightings(AICar* lpCar, RacingLine* lpRacingLine,
                                   RacingLineGenerator* lpRacingLineGenerator,
                                   const NearbyVehicles* lpNearbyVehicles,
                                   EGlobalRaceCarIndex leVictim)
{
    const f32* lpfBias = kfBias[meBiasMode];

    CalculateFanAngle(lpCar);                                        // @0x82794640
    GenerateFanVectors(lpCar);                                       // @0x8279464C
    CachePointAhead(lpRacingLineGenerator, lpRacingLine);            // @0x82794664

    if (lpfBias[eFan_SteerToCentre] != 0.0f)
        IncludeCentreLineTracking(lpRacingLineGenerator, lpRacingLine);          // @0x827946A0

    if (lpfBias[eFan_DriveParallel] != 0.0f)
        IncludeRouteParallelTracking(lpRacingLineGenerator, lpRacingLine);       // @0x827946D8

    if (lpfBias[eFan_AvoidHNG] != 0.0f || lpfBias[eFan_ExitHNG] != 0.0f)
        IncludeHardNoGo(lpRacingLineGenerator, lpRacingLine);                    // @0x82794720

    if (lpfBias[eFan_AvoidTraffic] != 0.0f || lpfBias[eFan_AvoidOncomingTraffic] != 0.0f)
        IncludeConstantBearing(lpRacingLine, lpCar, lpNearbyVehicles);           // @0x8279476C

    if (lpfBias[eFan_AvoidEdges] != 0.0f)
        IncludeRouteEdgeIntersection(lpRacingLineGenerator, lpRacingLine);       // @0x8279479C

    if (lpfBias[eFan_PreferCurrentDirection] != 0.0f)
        IncludePreferCurrentDirection();                                         // @0x827947C4

    if (lpfBias[eFan_DriftFinalDirection] != 0.0f)
        IncludeDriftDirectionTracking(lpRacingLineGenerator, lpRacingLine);      // @0x827947EC

    if (lpfBias[eFan_DriftFinalLocation] != 0.0f)
        IncludeDriftLocationTracking(lpRacingLineGenerator, lpRacingLine);       // @0x82794814

    if (lpfBias[eFan_SmashIntoPlayer] != 0.0f)
        IncludeSmashIntoPlayer(lpCar, lpNearbyVehicles, leVictim);               // @0x82794840

    if (lpfBias[eFan_SmashIntoRivals] != 0.0f)
        IncludeSmashIntoNearbyAI(lpCar, lpNearbyVehicles);                       // @0x82794868

    if (lpfBias[eFan_DriveCloseToPlayer] != 0.0f)
        IncludeDriveCloseToPlayer(lpRacingLine, lpCar, lpNearbyVehicles);        // @0x82794894
}

// ========================================================================================
// The eleven contributors that used to be parked here (IncludeHardNoGo, IncludeConstantBearing,
// IncludeRouteEdgeIntersection, IncludeDriftDirectionTracking, IncludeDriftLocationTracking,
// IncludeDriveCloseToPlayer, FindNeabyAIInTraffic, IncludeSmashIntoTarget, IncludeSmashIntoPlayer,
// IncludeSmashIntoNearbyAI, FanIntersectsEdge) were bodied by aiwave2 (2026-09-05) in their own
// partfiles: BrnAISteeringFan_HNG.cpp (HNG / route-edge / FanIntersectsEdge),
// BrnAISteeringFan_Traffic.cpp (constant bearing + BrnAISteeringFan_TrafficConstants.h) and
// BrnAISteeringFan_Aggression.cpp (smash / drive-close / drift + BrnAISteeringFan_AggressionConstants.h).
// ========================================================================================

}
