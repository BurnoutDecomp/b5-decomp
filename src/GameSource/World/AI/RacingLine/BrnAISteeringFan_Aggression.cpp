#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_AggressionConstants.h"

#include "GameSource/World/AI/BrnAICar.h"                             // AICar getters
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles
#include "GameSource/World/AI/BrnAIUtils.h"                           // DistancePosVelToOrigin
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/Math/BrnMathUtils.h"                             // BrnMath::Flatten
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cfloat>   // FLT_MAX (flt_8204F664, FindNeabyAIInTraffic's lfBestProximity seed)
#include <cmath>    // std::sqrt / std::fabs (the vrsqrtefp and vandc sequences)

// BrnAI::SteeringFan -- the AGGRESSION contributors (aiwave 2026-09-05 lane F2b). Third partfile
// of the weighting half; BrnAISteeringFan_Weightings.cpp owns the racing-line contributors and
// BrnAISteeringFan_Target.cpp the target/accumulate half. Every body here was parked in
// _Weightings.cpp as a `(void)`-argument stub; those stubs are deleted at mount time.
//
//   BODIED
//     IncludeSmashIntoTarget        @0x82787968  (writes whichever row the caller names)
//     FindNeabyAIInTraffic          @0x82787BE8  (sic: DWARF spelling; NO IDA export -- image bytes)
//     IncludeSmashIntoPlayer        @0x82791230  (row eFan_SmashIntoPlayer)
//     IncludeSmashIntoNearbyAI      @0x82791338  (row eFan_SmashIntoRivals)
//     IncludeDriveCloseToPlayer     @0x82787E58  (row eFan_DriveCloseToPlayer)
//     IncludeDriftDirectionTracking @0x827881B0  (row eFan_DriftFinalDirection)
//     IncludeDriftLocationTracking  @0x82788738  (row eFan_DriftFinalLocation)
//
// SHARED REGISTER MAP. UpdateWeightings @0x82794600 calls these with r3 = this and a permutation
// of r26 = lpCar, r27 = lpRacingLine, r25 = lpNearbyVehicles, r24 = leAggressionVictim; each
// banner below names its own.
//
// .rdata constants this TU reads (image.bin, big-endian, file offset == VA - 0x82000000):
//   flt_82001C98 == 1.0     flt_82001CC0 == 0.0     flt_820C4168 == 0.5
//   flt_820037C8 == -1.0    flt_82004C78 == -0.5    flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON)
//   flt_820047C8 == 0.05    flt_820C4880 == 3.5     flt_820C488C == 5.0
//   flt_820C4844 == 4.5     flt_820C4890 == 20.0    flt_820C8074 == -4.5
//   flt_820C3FAC == 100.0   flt_8204F664 == 3.4028235e38 (FLT_MAX)
// The two .bss tunables live in BrnAISteeringFan_AggressionConstants.h with their initialisers.
//
// FAN GEOMETRY. mUnitDirection[i] (this+0x220, stride 0x10) is ray i's unit 2D direction and
// mfWeighting[row] (this+0x3B4, stride 0x44 == 17*4) is the row each contributor writes. No
// contributor here calls WriteWeightingValues (the console's debug overlay hook); the bodied
// contributors in _Weightings.cpp do not either.

namespace BrnAI
{
namespace
{
    // flt_820C3B70 == 1.1920929e-07 (FLT_EPSILON) -- the per-component epsilon every
    // rw::math::vpu::IsZero the console inlines here tests against (`vandc` to drop the sign bit,
    // then `vcmpgtfp` against the splatted epsilon). Same constant _Weightings.cpp calls
    // KF_FAN_TINY; kept TU-local (internal linkage) so the two partfiles cannot drift apart.
    const f32 KF_AGGRESSION_TINY = 1.1920928955078125e-07f;

    // The 2D lane sum of `vmulfp128 vD,vA,vB ; vspltw 1 ; vspltw 0 ; vaddfp` -- rw::math::vpu::Dot
    // on a Vector2 (lanes 0 and 1 only; .z/.w are never read).
    f32 Dot2DAggression(const Vector2& lA, const Vector2& lB)
    {
        return lA.x * lB.x + lA.y * lB.y;
    }

    // vrsqrtefp + two Newton-Raphson refinement steps over (x*x + y*y). rw::math::vpu::Normalize
    // does NOT guard a zero length -- neither does the console here (no vsel follows), so a zero
    // input produces the same non-finite answer it does on the X360. Callers gate it.
    Vector2 Normalize2DAggression(const Vector2& lVector)
    {
        const f32 lfScale = 1.0f / std::sqrt(lVector.x * lVector.x + lVector.y * lVector.y);
        Vector2 lOut;
        lOut.x = lVector.x * lfScale;
        lOut.y = lVector.y * lfScale;
        lOut.z = 0.0f;
        lOut.w = 0.0f;
        return lOut;
    }

    // rw::math::vpu::Magnitude: the same refined rsqrt, but `vmulfp128 v0, lengthSq, rsqrt`
    // followed by `vcmpeqfp`/`vsel` against zero -- so a ZERO-length vector returns 0.0, not NaN.
    // That guard is the console's own (IncludeSmashIntoTarget @0x82787A74/9C,
    // FindNeabyAIInTraffic @0x82787CD0/F8 and @0x82787D98/DC0).
    f32 Magnitude2DAggression(const Vector2& lVector)
    {
        const f32 lfLengthSq = lVector.x * lVector.x + lVector.y * lVector.y;
        if (lfLengthSq == 0.0f)
            return 0.0f;                                   // vsel against the vcmpeqfp mask
        return std::sqrt(lfLengthSq);
    }

    // The `vandc` (drop the sign bit) + `vcmpgtfp` pair the console emits for
    // rw::math::vpu::IsZero on a Vector2: true when NEITHER component exceeds the epsilon.
    bool IsZero2DAggression(const Vector2& lVector)
    {
        return std::fabs(lVector.x) <= KF_AGGRESSION_TINY
            && std::fabs(lVector.y) <= KF_AGGRESSION_TINY;
    }

    // The `fsel f12,(lo-v),lo,v ; fsel f12,(hi-v),v,hi` ladder, i.e. rw::math::vpu::Clamp.
    f32 ClampAggression(f32 lfValue, f32 lfLow, f32 lfHigh)
    {
        if (lfValue < lfLow)  return lfLow;
        if (lfValue > lfHigh) return lfHigh;
        return lfValue;
    }

    // The 2D cross product rw::math::vpu::Cross reduces to on a Vector2
    // (IncludeDriveCloseToPlayer @0x8278807C..84).
    f32 Cross2DAggression(const Vector2& lA, const Vector2& lB)
    {
        return lA.x * lB.y - lA.y * lB.x;
    }

    // Zero one contributor row. Every Include* below has an arm that does exactly this
    // (`li r10,0x11 ; mtctr ; stw 0,0(r11) ; addi r11,r11,4 ; bdnz`).
    void ZeroContributorRow(f32* lpRow)
    {
        for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
            lpRow[liFanIndex] = 0.0f;
    }
}

// ========================================================================================
// IncludeSmashIntoTarget @0x82787968   (159 instructions)
//   r31 = this, r30 = lpCar, r28 = lpTarget, r29 = leContributor,
//   f31 = lfMinAheadness (f1), f30 = lfMaxAheadness (f2).
// DWARF BrnAISteeringFan.cpp:2005; locals :2007..:2015.
//
//   0x827879A0  lCarPosition  = BrnMath::Flatten(lpCar->GetPosition())        -> v127
//   0x827879B8  lCarDirection = BrnMath::Flatten(lpCar->GetDirection())       -> v1
//   0x827879D0  lvx128 v0, r28, 0x10  == lpTarget->mCentre
//   0x827879D8  lRelativeVector = lpTarget->mCentre - lCarPosition            -> v12
//   0x827879C8..A4C  two refined vrsqrtefp chains: v10 = 1/|lCarDirection|, v9 = 1/|lRelativeVector|
//   0x82787A48  v8  = lCarDirection * v10                  == Normalize(lCarDirection)
//   0x82787A54  v10 = lRelativeVector * v9                 == lRelativeDirection
//   0x82787A5C..AA8  lfAheadness = Dot(Normalize(lCarDirection), lRelativeVector)  (a DISTANCE
//               along the car's forward axis, NOT a cosine -- the separation is NOT normalised
//               on this side of the dot, which is why the callers pass metres: -4.5 .. 20.0)
//   0x82787A70..AA0  lfSeparation = Magnitude(lRelativeVector), with the vcmpeqfp/vsel zero guard,
//               stored to var_70 and read back at 0x82787AAC BEFORE 0x82787ABC overwrites var_70
//               with lfAheadness. (Both scalars share the slot; the read order is load-bearing.)
//   0x82787AB0..AE0  lfProximity = 1.0 - Clamp(lfSeparation * flt_820047C8, 0, 1)
//               flt_820047C8 == 0.05 == 1/20 == 1/KF_MAX_SEPERATION_FOR_SLAM (DWARF :2015); the
//               console compiler folded the division into the reciprocal.
//   0x82787AE4  lfProximity == 0.0 -> zero the row
//   0x82787AF0  lfAheadness < lfMinAheadness -> zero the row
//   0x82787AF8  lfAheadness > lfMaxAheadness -> zero the row
//   0x82787B00  row base = this + leContributor*0x44 + 0x3B4 == mfWeighting[leContributor]
//   0x82787B20..BA0  per ray: ((Dot(lRelativeDirection, mUnitDirection[i]) + 1.0) * 0.5) * lfProximity
//               (the 1.0 and the 0.5 are built as one-lane Vector2 scalars on the stack --
//                flt_82001C98 into var_80 and flt_820C4168 into var_70 -- then vspltw'd; that is
//                the VecFloat idiom, not two more constants.)
// The row is written in [0,1] scaled by lfProximity: rays pointing AT the target score highest,
// rays pointing away score 0. kfBias supplies the sign/strength (AccumulateWeightings @0x82779088).
// ========================================================================================
void SteeringFan::IncludeSmashIntoTarget(AICar* lpCar, const NearbyVehicle* lpTarget,
                                         EFan_Contributors leContributor,
                                         f32 lfMinAheadness, f32 lfMaxAheadness)
{
    // DWARF BrnAISteeringFan.cpp:2015 -- declared inside the body, not at file scope.
    const f32 KF_MAX_SEPERATION_FOR_SLAM = 20.0f;

    const Vector2 lCarPosition  = BrnMath::Flatten(lpCar->GetPosition());    // @0x827879A0
    const Vector2 lCarDirection = BrnMath::Flatten(lpCar->GetDirection());   // @0x827879B8

    Vector2 lRelativeVector;
    lRelativeVector.x = lpTarget->mCentre.x - lCarPosition.x;
    lRelativeVector.y = lpTarget->mCentre.y - lCarPosition.y;
    lRelativeVector.z = 0.0f;
    lRelativeVector.w = 0.0f;

    const Vector2 lRelativeDirection = Normalize2DAggression(lRelativeVector);
    const f32     lfSeparation       = Magnitude2DAggression(lRelativeVector);
    const f32     lfAheadness        = Dot2DAggression(Normalize2DAggression(lCarDirection),
                                                       lRelativeVector);

    const f32 lfProximity =
        1.0f - ClampAggression(lfSeparation / KF_MAX_SEPERATION_FOR_SLAM, 0.0f, 1.0f);

    f32* lpRow = mfWeighting[leContributor];

    if (lfProximity == 0.0f || lfAheadness < lfMinAheadness || lfAheadness > lfMaxAheadness)
    {
        ZeroContributorRow(lpRow);                                          // @0x82787BA8
        return;
    }

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)       // @0x82787B20
    {
        const f32 lfAlignment = Dot2DAggression(lRelativeDirection, mUnitDirection[liFanIndex]);
        lpRow[liFanIndex] = ((lfAlignment + 1.0f) * 0.5f) * lfProximity;
    }
}

// ========================================================================================
// FindNeabyAIInTraffic @0x82787BE8   (sic: the DWARF's spelling, BrnAISteeringFan.cpp:2048)
//   r31 = lpCar, r30 = the walking NearbyVehicle*, r29 = lpBestRival, f30 = lfOurSpeed,
//   f31 = lfBestProximity, v127 = lOurPosition, v126 = lOurMotion.
//
// NO IDA EXPORT EXISTS for 0x82787BE8 -- .ida-exports/BURNOUT_X360_ARTIST.XEX has no
// 0x82787BE8.json and scratch/aiwave2/names.tsv has no row for it, but IDA knows the name (it is
// in IncludeSmashIntoNearbyAI's xrefs_from). The whole body below was disassembled from the image
// bytes 0x82787BE8..0x82787E50 with tools/re/ppcdis.py; it is NOT inlined, IncludeSmashIntoNearbyAI
// really does `bl` it.
//
//   0x82787C20  f31 = flt_8204F664 == FLT_MAX                     (lfBestProximity seed)
//   0x82787C24  f30 = lpCar->GetSpeed()                           (lfOurSpeed)
//   0x82787C34  v127 = BrnMath::Flatten(lpCar->GetPosition())     (lOurPosition)
//   0x82787C4C  v126 = BrnMath::Flatten(lpCar->GetUsefulDirection()) (lOurMotion)
//   0x82787C60  GetCount() == 0 -> return NULL
//   constants hoisted out of the loop: f13 = flt_82001CC0 == 0.0, f10 = flt_820C4890 == 20.0,
//                                      f11 = 0x8300D6F8 (KF_SLAM_AHEADNESS),
//                                      f12 = 0x8300D75C (KF_SLAM_FROM_BEHIND_RELATIVE_SPEED)
//   loop, stride 0x70 from lpNearbyTraffic+0 (== mVehicle[i]):
//     0x82787C8C  mType (entry+0x20) != E_NEARBY_AI (1) -> next
//     0x82787CA8  lRelativePosition = mCentre - lOurPosition
//     0x82787CD0..D00  lfSeparation = Magnitude(lRelativePosition) (zero-guarded)
//     0x82787D04  lfSeparation > KF_AI_SMASH_SEPERATION (flt_820C4890 == 20.0) -> next
//     0x82787D0C  lfSeparation == 0.0 -> next
//     0x82787D50  lRelativePosition = Normalize(lRelativePosition)   <- REASSIGNED IN PLACE
//     0x82787D54..68  lfAheadness = Dot(lRelativePosition, lOurMotion)
//     0x82787D6C  lfAheadness < 0.0 -> next
//     0x82787D74..DCC  lfRelativeSpeed = lfOurSpeed - Magnitude(entry->mVelocity)
//     0x82787DE8  lfRelativeSpeed >  KF_SLAM_FROM_BEHIND_RELATIVE_SPEED -> keep the candidate
//     0x82787DF0  else lfAheadness > KF_SLAM_AHEADNESS -> next
//     0x82787DF8  lfProximity = MagnitudeSquared(lRelativePosition)
//     0x82787E14  lfProximity >= lfBestProximity -> next; else it is the new best
//
// CONSOLE QUIRK, REPRODUCED AS-IS. lfProximity is MagnitudeSquared of the ALREADY-NORMALISED
// lRelativePosition (`vmulfp128 v0, v10, v10` with v10 the unit vector written at 0x82787D50), so
// it is 1.0 for every candidate to within the rsqrt refinement error. With the FLT_MAX seed the
// FIRST qualifying rival therefore wins and no later one can displace it (1.0 >= 1.0 takes the
// `bge` skip). The DWARF confirms the shape -- MagnitudeSquared is called exactly once, and
// lRelativePosition is one variable that Normalize reassigns -- so this is the original source's
// own bug, not a transcription slip. Taking MagnitudeSquared before the Normalize would change
// rival selection; do NOT "fix" it.
// ========================================================================================
const NearbyVehicle* SteeringFan::FindNeabyAIInTraffic(const NearbyVehicles* lpNearbyTraffic,
                                                       AICar* lpCar)
{
    // DWARF BrnAISteeringFan.cpp:2076 -- declared inside the loop body. flt_820C4890 == 20.0.
    const f32 KF_AI_SMASH_SEPERATION = 20.0f;

    const NearbyVehicle* lpBestRival     = 0;                               // r29
    f32                  lfBestProximity = FLT_MAX;                         // flt_8204F664
    const f32            lfOurSpeed      = lpCar->GetSpeed();               // @0x82787C24
    const Vector2        lOurPosition    = BrnMath::Flatten(lpCar->GetPosition());
    const Vector2        lOurMotion      = BrnMath::Flatten(lpCar->GetUsefulDirection());

    const s32 liCount = lpNearbyTraffic->GetCount();                        // @0x82787C60
    for (s32 liTrafficIndex = 0; liTrafficIndex < liCount; ++liTrafficIndex)
    {
        const NearbyVehicle* lpRival = &lpNearbyTraffic->mVehicle[liTrafficIndex];
        if (lpRival->mType != E_NEARBY_AI)                                  // @0x82787C98
            continue;

        Vector2 lRelativePosition;
        lRelativePosition.x = lpRival->mCentre.x - lOurPosition.x;
        lRelativePosition.y = lpRival->mCentre.y - lOurPosition.y;
        lRelativePosition.z = 0.0f;
        lRelativePosition.w = 0.0f;

        const f32 lfSeparation = Magnitude2DAggression(lRelativePosition);
        if (lfSeparation > KF_AI_SMASH_SEPERATION)                          // @0x82787D04
            continue;
        if (lfSeparation == 0.0f)                                           // @0x82787D0C
            continue;

        lRelativePosition = Normalize2DAggression(lRelativePosition);       // @0x82787D50

        const f32 lfAheadness = Dot2DAggression(lRelativePosition, lOurMotion);
        if (lfAheadness < 0.0f)                                             // @0x82787D6C
            continue;

        const f32 lfRelativeSpeed = lfOurSpeed - Magnitude2DAggression(lpRival->mVelocity);
        if (lfRelativeSpeed <= KF_SLAM_FROM_BEHIND_RELATIVE_SPEED           // @0x82787DE8
            && lfAheadness > KF_SLAM_AHEADNESS)                             // @0x82787DF0
        {
            continue;
        }

        // See the CONSOLE QUIRK note above: lRelativePosition is the UNIT vector here.
        const f32 lfProximity = lRelativePosition.x * lRelativePosition.x
                              + lRelativePosition.y * lRelativePosition.y;
        if (lfProximity < lfBestProximity)                                  // @0x82787E14
        {
            lfBestProximity = lfProximity;
            lpBestRival     = lpRival;
        }
    }

    return lpBestRival;
}

// ========================================================================================
// IncludeSmashIntoPlayer @0x82791230   (65 instructions)
//   r28 = this, r25 = lpCar, r26 = lpNearbyTraffic, r27 = leAggressionVictim.
// DWARF BrnAISteeringFan.cpp:1949.
//
//   0x8279124C  leAggressionVictim == -1 -> the console's CGS_ASSERT
//               ("Invalid global race car ID used in slam", BrnAISteeringFan.cpp:1952). It is a
//               StrStream-formatted assert the retail build DID emit, and it is NOT a guard:
//               control falls straight through into FindVictimInTraffic afterwards.
//   0x827912D8  lpPlayer = FindVictimInTraffic(lpNearbyTraffic, leAggressionVictim)
//   0x827912E8  no victim -> zero this+0x6A0 (== mfWeighting[11], eFan_SmashIntoPlayer), return
//   0x8279130C  f1 = flt_820C8074 == -4.5, f2 = flt_820C4890 == 20.0, r6 = 0xB
//               -> IncludeSmashIntoTarget(lpCar, lpPlayer, eFan_SmashIntoPlayer, -4.5, 20.0)
// The aheadness window is asymmetric because IncludeSmashIntoTarget's lfAheadness is in METRES:
// the victim may be up to 20 m ahead along our forward axis but only 4.5 m behind it.
// ========================================================================================
void SteeringFan::IncludeSmashIntoPlayer(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles,
                                         EGlobalRaceCarIndex leAggressionVictim)
{
    CGS_ASSERT(leAggressionVictim != -1, "Invalid global race car ID used in slam\n");

    const NearbyVehicle* lpPlayer = FindVictimInTraffic(lpNearbyVehicles, leAggressionVictim);
    if (lpPlayer == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_SmashIntoPlayer]);              // @0x827912E8
        return;
    }

    IncludeSmashIntoTarget(lpCar, lpPlayer, eFan_SmashIntoPlayer,
                           -4.5f,          // f1 = flt_820C8074
                           20.0f);         // f2 = flt_820C4890
}

// ========================================================================================
// IncludeSmashIntoNearbyAI @0x82791338   (35 instructions)
//   r31 = this, r30 = lpCar, r5 = lpNearbyTraffic.
// DWARF BrnAISteeringFan.cpp:1976.
//
//   0x8279135C  lpNearbyAI = FindNeabyAIInTraffic(lpNearbyTraffic, lpCar)
//               (the console swaps its own arguments into r4 = r5, r5 = r30 first)
//   0x8279136C  none -> zero this+0x728 (== mfWeighting[13], eFan_SmashIntoRivals), return
//   0x8279138C  f1 = flt_820C8074 == -4.5, f2 = flt_820C4844 == 4.5, r6 = 0xD
//               -> IncludeSmashIntoTarget(lpCar, lpNearbyAI, eFan_SmashIntoRivals, -4.5, 4.5)
// The rival window is SYMMETRIC (+/-4.5 m) where the player window reaches 20 m ahead: a rival is
// only slammed when it is roughly alongside.
// ========================================================================================
void SteeringFan::IncludeSmashIntoNearbyAI(AICar* lpCar, const NearbyVehicles* lpNearbyVehicles)
{
    const NearbyVehicle* lpNearbyAI = FindNeabyAIInTraffic(lpNearbyVehicles, lpCar);
    if (lpNearbyAI == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_SmashIntoRivals]);              // @0x8279136C
        return;
    }

    IncludeSmashIntoTarget(lpCar, lpNearbyAI, eFan_SmashIntoRivals,
                           -4.5f,          // f1 = flt_820C8074
                           4.5f);          // f2 = flt_820C4844
}

// ========================================================================================
// IncludeDriveCloseToPlayer @0x82787E58   (214 instructions)
//   r30 = this, r31 = lpRacingLine, r28 = lpCar, r29 = lpNearbyTraffic then lpPlayer,
//   f26 = lfOurCarSpeed, v126 = lOurCarPosition, v127 = lPlayerUnitVelocity, f27 = lfCarSide.
// DWARF BrnAISteeringFan.cpp:2186; locals :2188..:2264.
//
//   0x82787E90  lfOurCarSpeed = lpCar->GetSpeed()
//   0x82787EA0  lpPlayer = FindPlayerInTraffic(lpNearbyTraffic)
//   0x82787EB0  no player -> zero this+0x6E4 (== mfWeighting[12]), return
//   0x82787ED8  lOurCarPosition: GetPosition(lpCar) then `vrlimi128 v10,v12,8,0` (lane 0 <- x)
//               and `vrlimi128 v126,v12,4,1` (lane 1 <- source lane 2 == z) -- the SAME XZ flatten
//               BrnMath::Flatten performs, written inline instead of called.
//   0x82787F10  lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead (+0xB20) = flt_82001CC0 == 0.0
//   0x82787F14  lpRacingLine->mfImmediateDistanceToTrafficImpact     (+0xB1C) = flt_820C3FAC == 100.0
//               (the proximity-speed inputs AIDriver::ProximitySpeed @0x82770800 reads; this
//                contributor resets them to "nothing ahead" before doing its own work.)
//   0x82787F1C..FDC  IsZero(lpPlayer->mVelocity) -> zero the row, return
//   0x82787FE0..8030  lPlayerUnitVelocity = Normalize(lpPlayer->mVelocity)
//   0x82788034  lPosA = BrnMath::Flatten(lpCar->GetPosition())   (a SECOND GetPosition call; the
//               DWARF lists AICar::GetPosition twice and BrnMath::Flatten once)
//   0x82788058..88  lPosB = lpPlayer->mCentre, lPosC = lPosB + lPlayerUnitVelocity,
//               lfCarSide = Cross(lPosC - lPosA, lPosB - lPosA)
//   0x827880A4  f27 = (lfCarSide >= 0) ? flt_82001C98 (1.0) : flt_820037C8 (-1.0)
//   loop i = 0..16 from 0x827880AC (r30 walks this+0x220, r25 walks this+0x6E4):
//     0x827880BC  lRelativePosition = lOurCarPosition - lpPlayer->mCentre
//     0x827880E8  lOurCarMotion     = mUnitDirection[i] * lfOurCarSpeed
//     0x827880F0  lRelativeVelocity = lOurCarMotion - lpPlayer->mVelocity            (-> v2)
//     0x827880EC..108  lfAheadness = Dot(lRelativePosition, mUnitDirection[i]) + KF_PROJECT_AHEAD
//                  (flt_820C488C == 5.0, DWARF :2260)
//     0x82788128  v1 = lRelativePosition - lPlayerUnitVelocity * that sum
//     0x82788130  BrnAI::DistancePosVelToOrigin(v1, v2)
//     0x82788138  lfPassingSpace = result * lfCarSide - KF_DESIRED_CLOSE_PASSING_SEPERATION (3.5)
//     0x82788140  lfPassingSpace >= 0 -> row[i] =  1.0 - Clamp(lfPassingSpace * 0.5, 0, 1)
//                 else                   row[i] = -Clamp(lfPassingSpace * -0.5, 0, 1)
//                 (0.5 == 1/KF_CLOSE_PASSING_RANGE; see the constants header.)
// The row is therefore +1 when the ray would put us exactly at the desired separation on the
// correct side and ramps to 0 as we drift KF_CLOSE_PASSING_RANGE beyond it; rays that would put
// us on the WRONG side (or inside the separation) score NEGATIVE.
// ========================================================================================
void SteeringFan::IncludeDriveCloseToPlayer(RacingLine* lpRacingLine, AICar* lpCar,
                                            const NearbyVehicles* lpNearbyVehicles)
{
    // DWARF BrnAISteeringFan.cpp:2260 -- declared inside the loop body. flt_820C488C == 5.0.
    const f32 KF_PROJECT_AHEAD = 5.0f;

    const f32 lfOurCarSpeed = lpCar->GetSpeed();                            // @0x82787E90

    const NearbyVehicle* lpPlayer = FindPlayerInTraffic(lpNearbyVehicles);  // @0x82787EA0
    if (lpPlayer == 0)
    {
        ZeroContributorRow(mfWeighting[eFan_DriveCloseToPlayer]);           // @0x82787EB0
        return;
    }

    // The inline vrlimi128 XZ flatten (@0x82787EFC / @0x82787F2C), not a BrnMath::Flatten call.
    const Vector3 lOurCarPosition3D = lpCar->GetPosition();
    Vector2 lOurCarPosition;
    lOurCarPosition.x = lOurCarPosition3D.x;
    lOurCarPosition.y = lOurCarPosition3D.z;
    lOurCarPosition.z = 0.0f;
    lOurCarPosition.w = 0.0f;

    lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead = 0.0f;           // @0x82787F10
    lpRacingLine->mfImmediateDistanceToTrafficImpact      = 100.0f;         // @0x82787F14

    if (IsZero2DAggression(lpPlayer->mVelocity))                            // @0x82787FC4
    {
        ZeroContributorRow(mfWeighting[eFan_DriveCloseToPlayer]);
        return;
    }

    const Vector2 lPlayerUnitVelocity = Normalize2DAggression(lpPlayer->mVelocity);  // DWARF :2207

    // DWARF :2220 -- loop-invariant, so the console recomputes it at 0x827880BC every step from
    // the same two loads; hoisted here to where the source declares it.
    Vector2 lRelativePosition;
    lRelativePosition.x = lOurCarPosition.x - lpPlayer->mCentre.x;
    lRelativePosition.y = lOurCarPosition.y - lpPlayer->mCentre.y;
    lRelativePosition.z = 0.0f;
    lRelativePosition.w = 0.0f;

    const Vector2 lPosA = BrnMath::Flatten(lpCar->GetPosition());           // DWARF :2224, @0x82788034
    const Vector2 lPosB = lpPlayer->mCentre;                                // DWARF :2225

    Vector2 lPosC;                                                          // DWARF :2226
    lPosC.x = lPosB.x + lPlayerUnitVelocity.x;
    lPosC.y = lPosB.y + lPlayerUnitVelocity.y;
    lPosC.z = 0.0f;
    lPosC.w = 0.0f;

    Vector2 lToPosB;
    lToPosB.x = lPosB.x - lPosA.x;
    lToPosB.y = lPosB.y - lPosA.y;
    lToPosB.z = 0.0f;
    lToPosB.w = 0.0f;

    Vector2 lToPosC;
    lToPosC.x = lPosC.x - lPosA.x;
    lToPosC.y = lPosC.y - lPosA.y;
    lToPosC.z = 0.0f;
    lToPosC.w = 0.0f;

    // DWARF :2232, @0x827880A4 (`fsel f27, lfCarSide, 1.0, -1.0`): +1 on one side of the player's
    // line of travel, -1 on the other, which is what flips the sign of lfPassingSpace below.
    const f32 lfCarSide = (Cross2DAggression(lToPosC, lToPosB) >= 0.0f) ? 1.0f : -1.0f;

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)       // @0x827880AC
    {
        const Vector2& lUnitDirection = mUnitDirection[liFanIndex];

        Vector2 lOurCarMotion;
        lOurCarMotion.x = lUnitDirection.x * lfOurCarSpeed;
        lOurCarMotion.y = lUnitDirection.y * lfOurCarSpeed;
        lOurCarMotion.z = 0.0f;
        lOurCarMotion.w = 0.0f;

        Vector2 lRelativeVelocity;
        lRelativeVelocity.x = lOurCarMotion.x - lpPlayer->mVelocity.x;
        lRelativeVelocity.y = lOurCarMotion.y - lpPlayer->mVelocity.y;
        lRelativeVelocity.z = 0.0f;
        lRelativeVelocity.w = 0.0f;

        // DWARF :2256 lfAheadness (the raw dot) then :2260 KF_PROJECT_AHEAD; the console adds them
        // at 0x82788108 (`fadds f0, f13, flt_820C488C`) and splats the sum at 0x82788124.
        const f32 lfAheadness   = Dot2DAggression(lRelativePosition, lUnitDirection);
        const f32 lfProjectHere = lfAheadness + KF_PROJECT_AHEAD;

        Vector2 lProjectedPosition;
        lProjectedPosition.x = lRelativePosition.x - lPlayerUnitVelocity.x * lfProjectHere;
        lProjectedPosition.y = lRelativePosition.y - lPlayerUnitVelocity.y * lfProjectHere;
        lProjectedPosition.z = 0.0f;
        lProjectedPosition.w = 0.0f;

        const f32 lfPassingSpace =
            DistancePosVelToOrigin(lProjectedPosition, lRelativeVelocity) * lfCarSide
            - KF_DESIRED_CLOSE_PASSING_SEPERATION;                          // @0x82788138

        if (lfPassingSpace >= 0.0f)                                         // @0x82788140
        {
            mfWeighting[eFan_DriveCloseToPlayer][liFanIndex] =
                1.0f - ClampAggression(lfPassingSpace / KF_CLOSE_PASSING_RANGE, 0.0f, 1.0f);
        }
        else
        {
            mfWeighting[eFan_DriveCloseToPlayer][liFanIndex] =
                -ClampAggression(-lfPassingSpace / KF_CLOSE_PASSING_RANGE, 0.0f, 1.0f);
        }
    }
}

// ========================================================================================
// IncludeDriftDirectionTracking @0x827881B0   (354 instructions -- a fully unrolled 17-step loop)
//   r3 = this, r4 = lpRacingLineGenerator (NEVER READ), r5 = lpRacingLine. Leaf: no prologue,
//   no calls, three `beqlr`/`blr` exits.
// DWARF BrnAISteeringFan.cpp:2327; locals :2339 lVectorToCentre, :2341 lLineVector, :2350 liFanIndex.
//
//   0x827881B0  !lpRacingLine->mbIsInitialised (+0xBD0)  -> return, row LEFT AS IT WAS
//   0x827881BC  !mbPointAheadKnown (this+0x808)          -> return, row LEFT AS IT WAS
//               (neither arm zeroes the row -- `beqlr`, unlike every other contributor here.)
//   0x827881E0  lVectorToCentre = mCentreAheadFarAhead (this+0x380) - mCentreFarAhead (this+0x370)
//   0x827881F8..828C  IsZero(lVectorToCentre): if it IS zero the normalise is SKIPPED
//               (`bne cr6, loc_827882DC`) and the raw zero vector is used for every dot -- the
//               console does not bail out here, so the row is written with 0.5 everywhere.
//   0x82788290..D8  lLineVector = Normalize(lVectorToCentre)
//   0x827882F0..8730  17 unrolled steps over this+0x220 .. +0x320 (mUnitDirection[0..16]) writing
//               this+0x618 .. +0x658 == mfWeighting[9] (eFan_DriftFinalDirection):
//                 lfAheadness = Dot(mUnitDirection[i], lLineVector)
//                 row[i] = (Clamp(lfAheadness, flt_820037C8 == -1.0, flt_82001C98 == 1.0) + 1.0)
//                          * flt_820C4168 (0.5)
//               i.e. the alignment remapped from [-1,1] to [0,1].
//
// PROVABLY UNREACHABLE IN THIS BUILD: kfBias[mode][eFan_DriftFinalDirection] is 0.0 in all ten
// bias modes, so UpdateWeightings @0x827947EC never calls this. Bodied for completeness; if a
// later build re-enables column 9 it is already correct.
// ========================================================================================
void SteeringFan::IncludeDriftDirectionTracking(RacingLineGenerator* lpRacingLineGenerator,
                                                RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised)                                     // @0x827881B0
        return;
    if (!mbPointAheadKnown)                                                 // @0x827881BC
        return;

    Vector2 lVectorToCentre;                                                // @0x827881E0
    lVectorToCentre.x = mCentreAheadFarAhead.x - mCentreFarAhead.x;
    lVectorToCentre.y = mCentreAheadFarAhead.y - mCentreFarAhead.y;
    lVectorToCentre.z = 0.0f;
    lVectorToCentre.w = 0.0f;

    Vector2 lLineVector = lVectorToCentre;
    if (!IsZero2DAggression(lVectorToCentre))                               // @0x8278828C
        lLineVector = Normalize2DAggression(lVectorToCentre);

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
    {
        const f32 lfAheadness = Dot2DAggression(mUnitDirection[liFanIndex], lLineVector);
        mfWeighting[eFan_DriftFinalDirection][liFanIndex] =
            (ClampAggression(lfAheadness, -1.0f, 1.0f) + 1.0f) * 0.5f;
    }
}

// ========================================================================================
// IncludeDriftLocationTracking @0x82788738   (354 instructions -- the same unrolled shape)
//   r3 = this, r4 = lpRacingLineGenerator (NEVER READ), r5 = lpRacingLine. Leaf.
// DWARF BrnAISteeringFan.cpp:2372; locals :2380 lVectorToCentre, :2387 lLineVector,
// :2405 liFanIndex, :2409 lVectorToFanTarget, :2413 lfAheadness.
//
//   0x82788738  !lpRacingLine->mbIsInitialised (+0xBD0)  -> return, row LEFT AS IT WAS
//   0x82788744  !mbPointAheadKnown (this+0x808)          -> return, row LEFT AS IT WAS
//   0x82788768  lVectorToCentre = mCentreFarAhead (this+0x370) - mFanOrigin2D (this+0x350)
//   0x82788814  IsZero(lVectorToCentre) -> `bnelr`, RETURN (this is where the two drift
//               contributors differ: the direction one merely skips the normalise)
//   0x82788818..74  lLineVector = Normalize(lVectorToCentre)
//   0x82788834..8CB8  17 unrolled steps over mUnitDirection[0..16] writing this+0x65C .. +0x69C
//               == mfWeighting[10] (eFan_DriftFinalLocation), identical arithmetic to the
//               direction row: (Clamp(Dot(unit, lLineVector), -1, 1) + 1) * 0.5.
//
// PROVABLY UNREACHABLE IN THIS BUILD for the same reason as the direction row: the
// kfBias[mode][eFan_DriftFinalLocation] column is 0.0 in all ten modes.
// ========================================================================================
void SteeringFan::IncludeDriftLocationTracking(RacingLineGenerator* lpRacingLineGenerator,
                                               RacingLine* lpRacingLine)
{
    (void)lpRacingLineGenerator;   // r4 -- never read by the console body

    if (!lpRacingLine->mbIsInitialised)                                     // @0x82788738
        return;
    if (!mbPointAheadKnown)                                                 // @0x82788744
        return;

    Vector2 lVectorToCentre;                                                // @0x82788768
    lVectorToCentre.x = mCentreFarAhead.x - mFanOrigin2D.x;
    lVectorToCentre.y = mCentreFarAhead.y - mFanOrigin2D.y;
    lVectorToCentre.z = 0.0f;
    lVectorToCentre.w = 0.0f;

    if (IsZero2DAggression(lVectorToCentre))                                // @0x82788814
        return;

    const Vector2 lLineVector = Normalize2DAggression(lVectorToCentre);

    for (s32 liFanIndex = 0; liFanIndex < KI_FAN_STEPS; ++liFanIndex)
    {
        const f32 lfAheadness = Dot2DAggression(mUnitDirection[liFanIndex], lLineVector);
        mfWeighting[eFan_DriftFinalLocation][liFanIndex] =
            (ClampAggression(lfAheadness, -1.0f, 1.0f) + 1.0f) * 0.5f;
    }
}

}
