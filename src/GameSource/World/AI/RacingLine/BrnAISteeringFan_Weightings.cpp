#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/BrnAICar.h"                             // AICar getters
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles
#include "GameSource/World/AI/BrnAIUtils.h"                           // FindSignedAngleBetween2DVectors
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator + the two gates
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cmath>    // std::sin / std::cos (the XMVectorSinCos polynomial), std::fabs, std::sqrt

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
//   PARKED (each with its own [FLAG PC bring-up] banner and DELETE-WHEN, below)
//     CalculateFanAngle             @0x82768CB0   -- no IDA export
//     IncludeRouteEdgeIntersection  @0x8277A378   -- no IDA export
//     IncludeHardNoGo               @0x82779D98   -- needs the RacingLineGenerator section stack
//     IncludeConstantBearing        @0x827873A0   -- needs .bss-resident tuning constants
//     IncludeDriftDirectionTracking @0x827881B0   -- kfBias column is 0 in all 10 modes
//     IncludeDriftLocationTracking  @0x82788738   -- kfBias column is 0 in all 10 modes
//     IncludeSmashIntoPlayer        @0x82791230 / IncludeSmashIntoNearbyAI @0x82791338 /
//     IncludeSmashIntoTarget        @0x82787968 / FindNeabyAIInTraffic /
//     IncludeDriveCloseToPlayer     @0x82787E58 / FanIntersectsEdge @0x8277A208
//
// SHARED REGISTER MAP for UpdateWeightings @0x82794600 (the console's, read off 0x82794614..30):
//   r3/r31 = this, r4/r26 = lpCar, r5/r27 = lpRacingLine, r6/r28 = lpRacingLineGenerator,
//   r7/r25 = lpNearbyTraffic, r8/r24 = leAggressionVictim.
// Every Include* below is called with a permutation of those; the per-function banners name it.

namespace BrnAI
{
namespace
{
    // The X360 spells these as inline vrsqrtefp/vrlimi/vsubfp sequences; the TU-local helpers keep
    // the bodies readable. Same pattern (and same names + "Fan" suffix to stay ODR-clean) as the
    // BrnAIDriver.cpp / BrnAIDriver_Update.cpp partfiles.
    Vector2 To2DFan(Vector3 lVector)
    {
        Vector2 lOut;
        lOut.x = lVector.x;
        lOut.y = lVector.z;   // XZ ground plane (Flatten mask @0x82CDA450 = x,z,x,x -- lane R7); was .y
        lOut.z = 0.0f;
        lOut.w = 0.0f;
        return lOut;
    }

    // vmulfp/vspltw/vaddfp (x*x + y*y) then the two-step vrsqrtefp Newton refinement, i.e. a
    // plain 2D normalise. The console does NOT guard a zero length here -- callers do (see the
    // KF_TINY epsilon tests in IncludeCentreLineTracking / IncludeRouteParallelTracking).
    Vector2 Normalize2DFan(Vector2 lVector)
    {
        const f32 lfLengthSq = lVector.x * lVector.x + lVector.y * lVector.y;
        const f32 lfScale    = 1.0f / std::sqrt(lfLengthSq);
        Vector2 lOut;
        lOut.x = lVector.x * lfScale;
        lOut.y = lVector.y * lfScale;
        lOut.z = 0.0f;
        lOut.w = 0.0f;
        return lOut;
    }

    f32 Dot2DFan(const Vector2& lA, const Vector2& lB)
    {
        return lA.x * lB.x + lA.y * lB.y;
    }

    // flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON) -- the "is this delta vector zero" epsilon both
    // centre-line contributors test each component of the delta against before normalising.
    const f32 KF_FAN_TINY = 1.1920929e-07f;

    // The console's own look-ahead distance for the drift point cache: flt_82004D0C == 40.0
    // (CachePointAhead @0x82791440 `lfs f1, flt_82004D0C`).
    const f32 KF_FAN_POINT_AHEAD_DISTANCE = 40.0f;
}

// ========================================================================================
// [FLAG PC bring-up] CalculateFanAngle @0x82768CB0 -- UNRECOVERABLE IN THIS TREE.
// UpdateWeightings calls it first (bl @0x82794640, r3 = this, r4 = lpCar) and it is the only
// writer of mfFanAngle besides Prepare. There is NO IDA export for 0x82768CB0
// (.ida-exports/BURNOUT_X360_ARTIST.XEX has no 0x82768CB0.json and names.tsv has no row for it),
// so no pseudocode or assembly is available to reconstruct from. The DWARF names the three
// file-scope constants it must consume (BrnAISteeringFan.cpp:24..26 KF_STEER_AT_LOW_SPEED /
// KF_STEER_AT_HIGH_SPEED / KF_GUESSED_MAX_SPEED), i.e. it narrows the fan as the car speeds up --
// but the ramp itself is not attested, and inventing it would be inventing behaviour.
// EFFECT OF THE PARK: mfFanAngle keeps SteeringFan::Prepare's value, flt_82F30428 == 1.3962634 rad
// (80 degrees), which is the console's own value for a stationary car, so the fan is at its widest
// at all speeds instead of narrowing. GenerateFanVectors is otherwise faithful.
// DELETE-WHEN 0x82768CB0 is disassembled from the image bytes (the recipe BrnAIUtils_Angles.cpp
// used for FindSignedAngleBetween2DVectors @0x827716A8) and this body is written.
// ========================================================================================
void SteeringFan::CalculateFanAngle(AICar* lpCar)
{
    (void)lpCar;
}

// ========================================================================================
// GenerateFanVectors @0x827792C0
//
// Rebuild the 17 fan rays around the car for this tick.
//   r29 = this, r31 = lpCar.
//   0x827792F0 GetPosition(lpCar)         -> v127 ; stvx128 v127, r29, 0x340 -> mFanOrigin
//   0x82779304 GetUsefulDirection(lpCar)  -> v13  (only x/y are used, via the vrlimi128 pair)
//   0x8277933C mFanOrigin2D.xy = carPos.xy (vrlimi128 into the loaded this+0x350)
//   0x827793B4 f28 = FindSignedAngleBetween2DVectors(normalize2D(useful.xy), (0,1))
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
// THE SIN/COS LANE ORDER is not read off the vperm constants (unk_82CDA3C0 / unk_82CDA400 are not
// exported); it is DERIVED, and the derivation is exact: FindSignedAngleBetween2DVectors(A,(0,1))
// returns acos(A.y) signed by -sign(A.x) (see BrnAIUtils_Angles.cpp @0x827716A8), so the car's own
// heading maps to the angle t at ray 8 (where the cubic term is 0) and the inverse that must
// reproduce it is ( -sin(t), cos(t) ): at t = 0 that is (0,1); at t = -pi/2 it is (1,0), which is
// exactly the direction whose signed angle to (0,1) is -pi/2. Ray 0 is therefore mfFanAngle to the
// car's RIGHT and ray 16 mfFanAngle to its LEFT, with ray 8 dead ahead.
// ========================================================================================
void SteeringFan::GenerateFanVectors(AICar* lpCar)
{
    const Vector3 lCarPosition = lpCar->GetPosition();            // @0x8276B1F0
    const Vector3 lUseful      = lpCar->GetUsefulDirection();     // @0x82770028

    mFanOrigin      = lCarPosition;                               // stvx128 v127, r29, 0x340
    mFanOrigin2D.x  = lCarPosition.x;                             // vrlimi128 8,0 / 4,1
    mFanOrigin2D.y  = lCarPosition.y;

    Vector2 lReference;                                           // the (0,1) the angle is from
    lReference.x = 0.0f;
    lReference.y = 1.0f;
    lReference.z = 0.0f;
    lReference.w = 0.0f;

    const Vector2 lCarDirection2D = Normalize2DFan(To2DFan(lUseful));
    const f32 lfBaseAngle = FindSignedAngleBetween2DVectors(lCarDirection2D, lReference);

    f32 lfT = 0.0f;
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        const f32 lfInterp = (lfT - 0.5f) * 2.0f;
        const f32 lfAngle  = (lfInterp * lfInterp * lfInterp) * mfFanAngle + lfBaseAngle;

        CGS_ASSERT(liStep < KI_FAN_STEPS, "Fan index out of range at ");

        Vector2 lUnit;
        lUnit.x = -std::sin(lfAngle);
        lUnit.y =  std::cos(lfAngle);
        lUnit.z = 0.0f;
        lUnit.w = 0.0f;
        mUnitDirection[liStep] = lUnit;

        mTarget[liStep].x = mFanOrigin2D.x + lUnit.x * mfLookAheadRadius;
        mTarget[liStep].y = mFanOrigin2D.y + lUnit.y * mfLookAheadRadius;
        mTarget[liStep].z = 0.0f;
        mTarget[liStep].w = 0.0f;

        mHNGTarget[liStep].x = mFanOrigin2D.x + lUnit.x * mfLookAheadHNGRadius;
        mHNGTarget[liStep].y = mFanOrigin2D.y + lUnit.y * mfLookAheadHNGRadius;
        mHNGTarget[liStep].z = 0.0f;
        mHNGTarget[liStep].w = 0.0f;

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
        // [FLAG PC bring-up] RacingLineGenerator::GetPointFarAhead @0x827900A0 has no body in this
        // tree (it needs GetLocalSectionID / GetSectionPointer / GetSectionInterpPosition). The
        // console's own "no point found" answer is false, which is what every consumer of
        // mbPointAheadKnown treats as "use the fallback". Same park as
        // AIDriver::FindFinalDriftDirection's. DELETE-WHEN the generator query stack lands.
        mbPointAheadKnown = false;
#endif
    }

#if BRN_AI_RACINGLINE_STACK_PRESENT
    mbCentreHereKnown = lpRacingLineGenerator->GetCentreCentreLineHere(lpRacingLine,
                                                                       mCentreTarget,
                                                                       mCentreTarget,
                                                                       mCentreHere,
                                                                       mCentreAhead);
#else
    // [FLAG PC bring-up] RacingLineGenerator::GetCentreCentreLineHere @0x8278E930 has no body in
    // this tree. false is the console's own "centre line not known" answer, and it is exactly what
    // AIDriver::Update's twin park at BrnAIDriver.cpp already stores into
    // RacingLine::mbCentreLineHereKnown. Consequence: IncludeCentreLineTracking and
    // IncludeRouteParallelTracking below both take their "zero the row" arm, so rows
    // eFan_SteerToCentre and eFan_DriveParallel contribute nothing until the generator lands.
    // DELETE-WHEN the generator query stack lands.
    (void)lpRacingLineGenerator;
    mbCentreHereKnown = false;
#endif
}

// ========================================================================================
// IncludeCentreLineTracking @0x82786BC8   (this r3, lpRacingLineGenerator r4 UNUSED, lpRacingLine r5)
// Row eFan_SteerToCentre (this+0x3B4 == mfWeighting[0]).
//
//   0x82786BC8  racing line not initialised (racingline+0xBD0) -> zero the row, return.
//   0x82786BF4  !mbCentreHereKnown (this+0x809)                -> zero the row, return.
//   0x82786C20  delta = mCentreHere (this+0x390) - Vector2(mFanOrigin.xy) (this+0x340)
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
    lDelta.x = mCentreHere.x - mFanOrigin.x;
    lDelta.y = mCentreHere.y - mFanOrigin.y;
    lDelta.z = 0.0f;
    lDelta.w = 0.0f;

    if (std::fabs(lDelta.x) <= KF_FAN_TINY && std::fabs(lDelta.y) <= KF_FAN_TINY)
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
    lDelta.x = mCentreHere.x - mCentreAhead.x;
    lDelta.y = mCentreHere.y - mCentreAhead.y;
    lDelta.z = 0.0f;
    lDelta.w = 0.0f;

    if (std::fabs(lDelta.x) <= KF_FAN_TINY && std::fabs(lDelta.y) <= KF_FAN_TINY)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            mfWeighting[eFan_DriveParallel][liStep] = 0.0f;
        return;
    }

    const Vector2 lBackAlongRoad = Normalize2DFan(lDelta);
    for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
    {
        f32 lfDot = Dot2DFan(mUnitDirection[liStep], lBackAlongRoad);
        if (lfDot < -1.0f) lfDot = -1.0f;
        if (lfDot >  1.0f) lfDot =  1.0f;
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
// [FLAG PC bring-up] THE SIX CONTRIBUTORS BELOW HAVE NO RECONSTRUCTED BODY.
// Each one keeps its console dispatch guard in UpdateWeightings above (the kfBias column test), so
// none of them is silently skipped -- the call is made and the row is simply left at whatever
// SteeringFan::Prepare / the previous tick left in it (Prepare zeroes all 14 rows).
// A zero row contributes nothing to AccumulateWeightings, which is why the fan can come out FLAT;
// see the [FLAG] on BRN_AI_STEERINGFAN_TARGET_PRESENT in BrnRacingLineGenerator.h for what a flat
// fan does to GetDrivingTarget and GetSpeedRatio.
//
//   IncludeHardNoGo @0x82779D98 (rows eFan_AvoidHNG / eFan_ExitHNG / eFan_FavourHNGDanger, and the
//     ONLY positive contributor in eBiasMode_Race: kfBias == +50 / -300). Needs
//     RacingLineGenerator::GetLocalSectionID @0x82776280, GetNearSectionID @0x827765A8,
//     GetSectionPointer, and HardNoGoMap::DistanceToHardNoGoEdge -- none of which has a body in
//     this tree (BrnHardNoGoMap.cpp bodies only MapSquareOccupiedFast / SetMapSquare, and
//     BrnRacingLineGenerator.cpp is not even mounted because MakeMap / FindMaximalEdges are
//     missing). DELETE-WHEN the RacingLineGenerator section-cache stack + HardNoGoMap land.
//
//   IncludeConstantBearing @0x827873A0 (rows eFan_AvoidTraffic / eFan_AvoidOncomingTraffic; the
//     traffic-avoidance half of eBiasMode_Race, kfBias == -100 / -400). 370 instructions of VMX
//     whose tuning constants (flt_8300D78C, flt_8300DB00, and the KF_SLOW_PASSING_SPACE /
//     KF_TRAFFIC_IMPACT_TIME group) live in the 0x8300xxxx region, which reads as ALL ZERO in the
//     static image -- they are written by dynamic initialisers, exactly like kfBias itself was
//     (see BrnAISteeringFan_Target.cpp's table banner for the recovery recipe: find the
//     lis/addi pair that names the address, decode the initialiser). Reconstructing the body
//     against zeroed constants would invent behaviour. DELETE-WHEN those constants are decoded.
//
//   IncludeRouteEdgeIntersection @0x8277A378 (row eFan_AvoidEdges, kfBias == -200 in Race). NO IDA
//     export exists for 0x8277A378 (it is the function immediately after FanIntersectsEdge
//     @0x8277A208, and .ida-exports has no 0x8277A378.json). DELETE-WHEN it is disassembled from
//     the image bytes.
//
//   IncludeDriftDirectionTracking @0x827881B0 / IncludeDriftLocationTracking @0x82788738 (rows
//     eFan_DriftFinalDirection / eFan_DriftFinalLocation). PROVABLY UNREACHABLE in this build:
//     both kfBias columns are 0.0 in all ten bias modes (verified against the static image AND the
//     dynamic initialiser at 0x82C69380, which never writes columns 9 or 10), so UpdateWeightings'
//     guard above never fires. Both are self-contained (no external calls) and are the cheapest
//     remaining recon if a later build re-enables the columns.
//     DELETE-WHEN a mode with a non-zero column 9/10 exists, or on a completeness pass.
//
//   IncludeDriveCloseToPlayer @0x82787E58 (row eFan_DriveCloseToPlayer; kfBias == 60 in
//     eBiasMode_CloseToPlayer -- which is the mode AIDriver::SetDrivingFanBiases selects for
//     behaviour E_AI_BEHAVIOUR_ROLLING_START -- and 50 in eBiasMode_VeerAwayFromPlayer). Needs
//     BrnAI::DistancePosVelToOrigin + BrnMath::Flatten (both present) but also the
//     KF_CLOSE_PASSING_RANGE / KF_DESIRED_CLOSE_PASSING_SEPERATION constants, which are in the
//     same zeroed 0x8300xxxx region as IncludeConstantBearing's.
//     DELETE-WHEN those constants are decoded.
//
//   IncludeSmashIntoNearbyAI @0x82791338 / IncludeSmashIntoPlayer @0x82791230 /
//   IncludeSmashIntoTarget @0x82787968 / FindNeabyAIInTraffic / FanIntersectsEdge @0x8277A208
//     (rows eFan_SmashIntoRivals / eFan_SmashIntoPlayer). The two entry points are 35 and 65
//     instructions and are pure dispatch into IncludeSmashIntoTarget, whose 159-instruction body
//     needs KF_SLAM_AHEADNESS / KF_MAX_SEPERATION_FOR_SLAM / KF_SLAM_FROM_BEHIND_RELATIVE_SPEED --
//     again the zeroed 0x8300xxxx constant region. FanIntersectsEdge is only reached from
//     IncludeRouteEdgeIntersection, which has no export.
//     DELETE-WHEN those constants are decoded.
// ========================================================================================
void SteeringFan::IncludeHardNoGo(RacingLineGenerator* lpRacingLineGenerator,
                                  RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;
    (void)lpRacingLine;
}

void SteeringFan::IncludeConstantBearing(RacingLine* lpRacingLine, AICar* lpCar,
                                         const NearbyVehicles* lpNearbyVehicles)
{
    (void)lpRacingLine;
    (void)lpCar;
    (void)lpNearbyVehicles;
}

void SteeringFan::IncludeRouteEdgeIntersection(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;
    (void)lpRacingLine;
}

void SteeringFan::IncludeDriftDirectionTracking(RacingLineGenerator* lpRacingLineGenerator,
                                                RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;
    (void)lpRacingLine;
}

void SteeringFan::IncludeDriftLocationTracking(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;
    (void)lpRacingLine;
}

void SteeringFan::IncludeDriveCloseToPlayer(RacingLine* lpRacingLine, AICar* lpCar,
                                            const NearbyVehicles* lpNearbyVehicles)
{
    (void)lpRacingLine;
    (void)lpCar;
    (void)lpNearbyVehicles;
}

const NearbyVehicle* SteeringFan::FindNeabyAIInTraffic(const NearbyVehicles* lpNearbyTraffic,
                                                       AICar* lpCar)
{
    (void)lpNearbyTraffic;
    (void)lpCar;
    return 0;
}

void SteeringFan::IncludeSmashIntoTarget(AICar* lpCar, const NearbyVehicle* lpTarget,
                                         EFan_Contributors leContributor,
                                         f32 lfMinAheadness, f32 lfMaxAheadness)
{
    (void)lpCar;
    (void)lpTarget;
    (void)leContributor;
    (void)lfMinAheadness;
    (void)lfMaxAheadness;
}

void SteeringFan::IncludeSmashIntoPlayer(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles,
                                         EGlobalRaceCarIndex leAggressionVictim)
{
    (void)lpCar;
    (void)lpNearbyVehicles;
    (void)leAggressionVictim;
}

void SteeringFan::IncludeSmashIntoNearbyAI(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles)
{
    (void)lpCar;
    (void)lpNearbyVehicles;
}

f32 SteeringFan::FanIntersectsEdge(Vector2* lpEdge, s32 liIndex, Vector2 lA, Vector2 lB)
{
    (void)lpEdge;
    (void)liIndex;
    (void)lA;
    (void)lB;
    return 0.0f;
}

}
