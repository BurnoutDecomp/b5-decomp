#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"

#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_TrafficConstants.h" // the recovered tunables
#include "GameSource/World/AI/BrnAICar.h"                             // AICar::GetSpeed/GetPosition/GetDirection
#include "GameSource/World/AI/BrnAIDriver.h"                          // NearbyVehicle / NearbyVehicles / ENearbyType
#include "GameSource/World/AI/BrnAIUtils.h"                           // BrnAI::DistancePosVelToOrigin
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine
#include "GameSource/Math/BrnMathUtils.h"                             // BrnMath::Flatten (XZ ground plane)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cmath>    // std::sqrt (the vrsqrtefp + Newton chains), std::fabs

// BrnAI::SteeringFan -- the traffic-avoidance contributor, split out of the weighting partfile.
//
//   IncludeConstantBearing @0x827873A0   rows eFan_AvoidTraffic(4) / eFan_AvoidOncomingTraffic(5)
//
// The parked stub for this function in BrnAISteeringFan_Weightings.cpp is superseded by the body
// below; it must be deleted when this TU is mounted (one body per symbol).
//
// "Constant bearing" is the sailor's collision test: for each of the 17 fan rays, pretend the car
// drives along that ray at its current speed, then ask, per nearby vehicle, whether the RELATIVE
// motion closes on the origin inside KF_TRAFFIC_IMPACT_TIME and passes closer than the allowed
// passing space. Rays that fail get a positive penalty in row 4 (same-direction traffic) or row 5
// (oncoming); AccumulateWeightings then multiplies those rows by kfBias, which is NEGATIVE for
// both (-100 / -400 in eBiasMode_Race), so a penalised ray loses.
//
// EVIDENCE. Every operand below was read from tools/re/vmx128.py --func 0x827873A0 (the raw VMX128
// register fields), not from IDA's listing: this body is full of vA operands IDA prints in the
// v86..v95 band. The Hex-Rays pseudocode is a hint only, and where it lies the lie is named.
//
// X360 REGISTER MAP (prologue 0x827873BC..0x827873D0)
//   r3/r19 = this          r4/r17 = lpRacingLine    r5/r27 = lpCar    r6/r24 = lpNearbyTraffic
//   f21 = lfPlayerSpeed    f31 = 1.0                f29 = 0.0         f28 = lfAllowedPassingSpace
//   f27 = lfReciprocalAllowedPassingSpace           f24 = lfClosestCollision
//   f26 = 80.0             f25 = 0.0125             f20 = 0.2         f23/f22 = the two old rows
//   r16 = &KF_CLOSENESS_TO_BRAKE (0x82F302CC; the three .data tunables are all reached off it)
//   r18 = lpRacingLine + 0xB1C = &mfImmediateDistanceToTrafficImpact
//   r30 = &mfWeighting[5][i]  (so r30 - 0x44 == &mfWeighting[4][i])
//   r27 (REUSED after GetSpeed) = &mUnitDirection[i]        r29 = &lpNearbyTraffic->mVehicle[j]
//   v121 = lPlayerPosition   v119 = lPlayerDirection   v122 = lPlayerMotion
//   v123 = lRelativeVelocity v1   = lRelativePosition  v120 = lImmmediateApproachSpeedOfTrafficAhead
//   v124 = lRelativePosition componentwise squared     v125 = 0
//
// LOCAL NAMES are the DecFIGS DWARF's (dwarfdump/.../BrnAISteeringFan.cpp:979, source lines
// :1505..:1660); the source-line numbers are quoted per declaration below.

namespace BrnAI
{
namespace
{
    // ---- the inlined VMX idioms, spelled once ------------------------------------------------
    // Named with a "Traffic" suffix so they stay distinct from the "...Fan" set in
    // BrnAISteeringFan_Weightings.cpp; both live in anonymous namespaces (internal linkage).

    // vmulfp128 + vspltw 0 + vspltw 1 + vaddfp: the 2-lane dot the console spells inline eight
    // times in this body. Lanes z/w are never summed here (that is vmsum3fp128, below).
    f32 Dot2DTraffic(const Vector2& lA, const Vector2& lB)
    {
        return lA.x * lB.x + lA.y * lB.y;
    }

    // vrsqrtefp + Newton refinement + `vsel v0, v0, 0, (x == 0)`, i.e. sqrt with a zero guard.
    // The console spells it with ONE Newton step where the DWARF says SqrtFast and TWO where the
    // DWARF says Magnitude/Sqrt; on the host both lower to the same exact std::sqrt.
    // [FLAG VMX->portable] estimate + Newton -> exact std::sqrt. Numerically TIGHTER than the
    // console, not a placeholder -- the same substitution BrnMathUtils.cpp's Magnitude/IsNormal
    // already made. DELETE-WHEN a bit-exact VMX emulation layer exists.
    f32 SqrtTraffic(f32 lfValue)
    {
        return (lfValue == 0.0f) ? 0.0f : std::sqrt(lfValue);
    }

    // fsel f0, f13, f29, f0 / fsubs+fsel f0, f13, f0, f31 -- the console's branchless clamp of a
    // ratio into [0,1] (0x827873F0..0x8278740C and again at 0x827877E0..0x82787804). Written as
    // fsel's own predicate (>= 0 picks the second operand) so the -0.0 and NaN behaviour matches.
    f32 ClampUnitTraffic(f32 lfValue)
    {
        const f32 lfFloored = ((-lfValue) >= 0.0f) ? 0.0f : lfValue;
        return ((1.0f - lfFloored) >= 0.0f) ? lfFloored : 1.0f;
    }

    // fsubs f11, f12, f0 / fsel f0, f11, f12, f0 == max(lfA, lfB) with fsel's tie/NaN rule.
    f32 MaxTraffic(f32 lfA, f32 lfB)
    {
        return ((lfA - lfB) >= 0.0f) ? lfA : lfB;
    }

    // fsubs + fmadds at 0x82787888..0x827878A8: from + (to - from) * time.
    // This is SteeringFan::Interpolate (DWARF BrnAISteeringFan.cpp:276), which has no IDA export
    // because the compiler inlined it at every call site. It is spelled TU-locally here because
    // BrnAISteeringFan.h belongs to another lane -- see the header_request in this lane's report.
    f32 InterpolateTraffic(f32 lfFrom, f32 lfTo, f32 lfTime)
    {
        return (lfTo - lfFrom) * lfTime + lfFrom;
    }

    // vmsum3fp128 v0, v120, v120 then the two-step rsqrt chain at 0x827878B0..0x827878FC.
    // THREE lanes, not two: the console really does include z, and lImmmediateApproachSpeedOfTrafficAhead
    // is a Vector3 in the DWARF (:1519). With mUnitDirection[i].z == 0 (GenerateFanVectors) the z
    // term reduces to the traffic entity's own velocity z lane.
    f32 Magnitude3Traffic(const Vector3& lVector)
    {
        return SqrtTraffic(lVector.x * lVector.x + lVector.y * lVector.y + lVector.z * lVector.z);
    }
}

// ================================================================================================
// IncludeConstantBearing @0x827873A0
//   (this r3, lpRacingLine r4, lpCar r5, lpNearbyTraffic r6 -- UpdateWeightings' call @0x8279476C)
//
// 0x827873D0  f21 = AICar::GetSpeed(lpCar)
// 0x827873E8  f0  = KF_GUESSED_MAX_SPEED (.bss flt_8300D78C, recovered -- see the constants header)
// 0x827873F0  f0  = f21 / f0, then the fsel pair clamps it into [0,1]        -> lfSpeedRatio
// 0x82787414  f28 = lfSpeedRatio * 0.0 + KF_SLOW_PASSING_SPACE               -> lfAllowedPassingSpace
//             (the multiplier is f29, the SHARED 0.0 literal flt_82001CC0 -- the same register the
//              clamp floor and the assert compare use -- i.e. a folded compile-time zero, NOT an
//              unrecovered .bss slot. Kept so the shape of the console expression survives.)
// 0x82787418  CGS_ASSERT(f28 != 0.0)  "Passing Space is zero"  (BrnAISteeringFan.cpp:1512)
// 0x82787498  f27 = 1.0 / f28
// 0x827874A0  GetPosition(lpCar); the vrlimi128 pair at 0x827874C0/CC packs (pos.x, pos.z) into
//             v121 -- lane 0 from mask 8 shift 0, lane 1 from mask 4 shift 1 (rotate-left-one
//             puts .z in .y). That is the same XZ flatten BrnMath::Flatten performs; lanes z/w of
//             v121 keep the raw position and are DEAD (every consumer of v1 splats lane 0 or 1).
// 0x827874D0  v119 = BrnMath::Flatten(GetDirection(lpCar))
// 0x8278751C  lpRacingLine->mfImmediateDistanceToTrafficImpact = 10000.0
// 0x8278753C  the fan loop. r26 counts 17 iterations while r27/r30 walk DOWNWARDS, so the console
//             visits liFanIndex 16..0. THAT ORDER IS LOAD-BEARING and is preserved below: the
//             "closest collision" test at 0x8278786C is a strict `<`, and |lRelativePosition|^2
//             does not depend on the fan index, so when one vehicle qualifies on several rays the
//             FIRST ray visited keeps the record -- and the recorded value
//             (lImmmediateApproachSpeedOfTrafficAhead = lRelativeVelocity) DOES depend on the ray.
//             Running the loop forwards would silently pick a different ray's relative velocity.
// 0x827878B0  mfImmmediateApproachSpeedOfTrafficAhead = |lImmmediateApproachSpeedOfTrafficAhead|
// 0x8278790C  mfImmediateDistanceToTrafficImpact = sqrt(itself)  -- UNCONDITIONAL, which is why
//             the 10000.0 seed above reports as 100.0 m when no traffic is found.
//             +0xB18 (mfImmediateTimeToTrafficImpact) is NOT written by this function.
//
// PSEUDOCODE LIES CORRECTED (all resolved from the asm / vmx128 fields):
//   * Hex-Rays renders the prototype as `int IncludeConstantBearing()` with no parameters at all;
//     the four arguments are the mr chain at 0x827873BC..0x827873D0.
//   * `v13 = ((_FP13 * 0.0) + flt_82F302C4)` hides that flt_82F302C4 is KF_SLOW_PASSING_SPACE and
//     that the 0.0 is a register, not an immediate.
//   * it prints `BrnAI::DistancePosVelToOrigin(v30)` -- an integer argument. The real arguments are
//     the two VECTOR registers live across the call: v1 = lRelativePosition, v2 = lRelativeVelocity
//     (v2 is set by the otherwise-dead `vmr128 v2, v123` at 0x827876EC, which is what identifies it).
//   * it shows the vcmpgtfp. CR6 extraction as an opaque bit shuffle; `extrwi r11, r11, 1, 24`
//     takes CR6 bit 0, the "all lanes true" bit, and the compared lanes are uniform splats, so the
//     tests are the plain scalar `< 0.0f` comparisons written below.
// ================================================================================================
void SteeringFan::IncludeConstantBearing(RacingLine* lpRacingLine, AICar* lpCar,
                                         const NearbyVehicles* lpNearbyTraffic)
{
    const f32 lfPlayerSpeed = lpCar->GetSpeed();                                      // :1506

    // :1509 -- rw::math::vpu::Clamp in the DWARF call list; the fsel pair in the asm.
    const f32 lfSpeedRatio = ClampUnitTraffic(lfPlayerSpeed / KF_GUESSED_MAX_SPEED);

    // :1511 -- see the banner: KF_PASSING_SPACE_SPEED_TERM is the console's folded 0.0.
    const f32 lfAllowedPassingSpace = lfSpeedRatio * KF_PASSING_SPACE_SPEED_TERM + KF_SLOW_PASSING_SPACE;
    CGS_ASSERT(lfAllowedPassingSpace != 0.0f, "Passing Space is zero\n");             // :1512
    const f32 lfReciprocalAllowedPassingSpace = 1.0f / lfAllowedPassingSpace;         // :1513

    // :1507 -- the inline XZ flatten of the car position (lanes z/w are dead, so they are zeroed
    // here rather than carrying the console's leftover lanes).
    const Vector3 lCarPosition = lpCar->GetPosition();
    Vector2 lPlayerPosition;
    lPlayerPosition.x = lCarPosition.x;
    lPlayerPosition.y = lCarPosition.z;
    lPlayerPosition.z = 0.0f;
    lPlayerPosition.w = 0.0f;

    const Vector2 lPlayerDirection = BrnMath::Flatten(lpCar->GetDirection());         // :1521

    f32 lfClosestCollision = KF_NO_CLOSEST_COLLISION;                                 // :1517
    Vector3 lImmmediateApproachSpeedOfTrafficAhead;                                   // :1519 (sic)
    lImmmediateApproachSpeedOfTrafficAhead.SetZero();

    lpRacingLine->mfImmediateDistanceToTrafficImpact = KF_NO_TRAFFIC_IMPACT_DISTANCE_SQUARED;

    // 16 -> 0; see the banner for why the direction is preserved.                    // :1505
    for (s32 liFanIndex = KI_FAN_STEPS - 1; liFanIndex >= 0; --liFanIndex)
    {
        // :1529 -- the velocity the car would have if it drove along this ray.
        // vmulfp128 v122, v0, v13 @0x82787588 (all four lanes; z feeds the Vector3 magnitude at
        // the very end via lRelativeVelocity).
        const Vector2& lUnitDirection = mUnitDirection[liFanIndex];
        Vector2 lPlayerMotion;
        lPlayerMotion.x = lUnitDirection.x * lfPlayerSpeed;
        lPlayerMotion.y = lUnitDirection.y * lfPlayerSpeed;
        lPlayerMotion.z = lUnitDirection.z * lfPlayerSpeed;
        lPlayerMotion.w = lUnitDirection.w * lfPlayerSpeed;

        f32* lfAvoidTraffic         = &mfWeighting[eFan_AvoidTraffic][liFanIndex];         // :1537
        f32* lfAvoidOncomingTraffic = &mfWeighting[eFan_AvoidOncomingTraffic][liFanIndex]; // :1538

        // 0x8278755C..0x82787574: both rows are read out, then zeroed, then rebuilt from scratch
        // by the max-accumulate below, and finally interpolated back toward the saved values.
        const f32 lfOldTraffic  = *lfAvoidTraffic;                                    // :1540
        const f32 lfOldOncoming = *lfAvoidOncomingTraffic;                            // :1541
        *lfAvoidTraffic         = 0.0f;
        *lfAvoidOncomingTraffic = 0.0f;

        // 0x8278757C..0x827875CC: the console re-reads miCount three times here because
        // NearbyVehicles::GetCount @0x82766748 is inlined WITH its two asserts
        // (BrnAIDriver.cpp:2912 "miCount >= 0" and :2913 "miCount <= KI_MAX_NEARBY_TRAFFIC").
        // Those asserts belong to GetCount, not to this function -- see this lane's
        // header_request for restoring them there rather than duplicating them here.
        const s32 liTrafficCount = lpNearbyTraffic->GetCount();

        for (s32 liTrafficIndex = 0; liTrafficIndex < liTrafficCount; ++liTrafficIndex)  // :1533
        {
            const NearbyVehicle* lpTraffic = &lpNearbyTraffic->mVehicle[liTrafficIndex]; // :1535

            // vsubfp128 v123, v0, v122 @0x827875E0 -- all four lanes (see Magnitude3Traffic).
            Vector2 lRelativeVelocity;                                                // :1550
            lRelativeVelocity.x = lpTraffic->mVelocity.x - lPlayerMotion.x;
            lRelativeVelocity.y = lpTraffic->mVelocity.y - lPlayerMotion.y;
            lRelativeVelocity.z = lpTraffic->mVelocity.z - lPlayerMotion.z;
            lRelativeVelocity.w = lpTraffic->mVelocity.w - lPlayerMotion.w;

            // vsubfp128 v1, v0, v121 @0x827875F0 (mCentre is entry+0x10, reached as r11 - 0x10
            // with r11 == entry+0x20). Lanes z/w are dead in every consumer.
            Vector2 lRelativePosition;                                                // :1551
            lRelativePosition.x = lpTraffic->mCentre.x - lPlayerPosition.x;
            lRelativePosition.y = lpTraffic->mCentre.y - lPlayerPosition.y;
            lRelativePosition.z = 0.0f;
            lRelativePosition.w = 0.0f;

            // 0x827875F4..0x82787610: a positive dot means the gap is opening. Only closing
            // vehicles are considered.
            const f32 lfAheadness = Dot2DTraffic(lRelativeVelocity, lRelativePosition); // :1553
            if (lfAheadness > 0.0f)
                continue;

            // 0x82787614..0x82787670 -- the player gets two extra outs.
            if (lpTraffic->mType == E_NEARBY_PLAYER)
            {
                // 0x82787620: while slamming, the player is a target, not an obstacle.
                if (meBiasMode == eBiasMode_Slam)
                    continue;
                // 0x8278762C: and a player who is BEHIND the car is not avoided either.
                if (Dot2DTraffic(lPlayerDirection, lRelativePosition) < 0.0f)
                    continue;
            }

            // 0x82787674..0x827876B4. Both magnitudes are 2-lane (vspltw 0 + vspltw 1).
            const f32 lfRelativeSpeedSquared = lRelativeVelocity.x * lRelativeVelocity.x
                                             + lRelativeVelocity.y * lRelativeVelocity.y;
            const f32 lfDistanceSquared      = lRelativePosition.x * lRelativePosition.x   // :1578
                                             + lRelativePosition.y * lRelativePosition.y;
            const f32 lfTimeToCollisionSquared = lfDistanceSquared / lfRelativeSpeedSquared;

            // 0x827876C4: the .bss constant this lane recovered. Note the console tests the
            // SQUARED time before taking the root -- that is what KF_TRAFFIC_IMPACT_TIME_SQUARED
            // exists for, and it is why the fdivs above is not guarded (see this lane's risks).
            if (lfTimeToCollisionSquared > KF_TRAFFIC_IMPACT_TIME_SQUARED)
                continue;

            const f32 lfTimeToCollision = SqrtTraffic(lfTimeToCollisionSquared);      // :1579

            // 0x82787730..0x82787738: 1 at the moment of impact, 0 at KF_TRAFFIC_IMPACT_TIME out.
            const f32 lfRisk = 1.0f - (lfTimeToCollision / KF_TRAFFIC_IMPACT_TIME);

            // 0x8278773C: v1 = lRelativePosition, v2 = lRelativeVelocity. The perpendicular
            // distance from the car to the line the relative motion sweeps, i.e. how close this
            // vehicle passes if neither party changes course.
            const f32 lfClosestApproach = DistancePosVelToOrigin(lRelativePosition, lRelativeVelocity);

            // 0x82787740..0x827877A0 (two Newton steps -- rw::math::vpu::Magnitude in the DWARF).
            const f32 lfSeparation = SqrtTraffic(lfDistanceSquared);                  // :1615
            if (lfSeparation > KF_MAX_TRAFFIC_CONSIDERATION_RANGE)
                continue;

            // 0x827877A4..0x827877B4: the required clearance grows with range, so a vehicle 80 m
            // away must miss by a full passing space while one at touching distance need only
            // miss at all.
            const f32 lfPassingSpace = lfSeparation * KF_RECIPROCAL_MAX_TRAFFIC_CONSIDERATION_RANGE
                                           * lfAllowedPassingSpace
                                     + std::fabs(lfClosestApproach);                  // :1608
            if (lfPassingSpace > lfAllowedPassingSpace)
                continue;

            // 0x827877B8..0x8278780C.
            const f32 lfCloseness = 1.0f - ClampUnitTraffic(lfPassingSpace * lfReciprocalAllowedPassingSpace);
            const f32 lfNewBias   = lfCloseness * lfRisk;                             // :1643

            // 0x82787800/0x8278781C: the vehicle's own velocity opposing the ray we are testing
            // is what makes it "oncoming"; anything else lands in the same-direction row.
            if (Dot2DTraffic(lpTraffic->mVelocity, lPlayerMotion) < 0.0f)
                *lfAvoidOncomingTraffic = MaxTraffic(*lfAvoidOncomingTraffic, lfNewBias);
            else
                *lfAvoidTraffic         = MaxTraffic(*lfAvoidTraffic, lfNewBias);

            // 0x82787844..0x82787878: the braking feed. Note the test is on lfCloseness, BEFORE
            // it is scaled by lfRisk.
            if (lfCloseness > KF_CLOSENESS_TO_BRAKE)
            {
                // :1660 -- MagnitudeSquared, and the member keeps the squared value until the
                // single sqrt at the end of the function.
                const f32 lfDistance = lfDistanceSquared;
                if (lfDistance < lfClosestCollision)
                {
                    lpRacingLine->mfImmediateDistanceToTrafficImpact = lfDistance;
                    lfClosestCollision = lfDistance;
                    lImmmediateApproachSpeedOfTrafficAhead.x = lRelativeVelocity.x;
                    lImmmediateApproachSpeedOfTrafficAhead.y = lRelativeVelocity.y;
                    lImmmediateApproachSpeedOfTrafficAhead.z = lRelativeVelocity.z;
                    lImmmediateApproachSpeedOfTrafficAhead.w = lRelativeVelocity.w;
                }
            }
        }

        // 0x82787888..0x827878A8 -- SteeringFan::Interpolate, inlined twice. Unconditional: with
        // no traffic at all both rows decay toward 0 by KF_TRAFFIC_BIAS_LERP per tick.
        *lfAvoidTraffic         = InterpolateTraffic(lfOldTraffic,  *lfAvoidTraffic,  KF_TRAFFIC_BIAS_LERP);
        *lfAvoidOncomingTraffic = InterpolateTraffic(lfOldOncoming, *lfAvoidOncomingTraffic, KF_TRAFFIC_BIAS_LERP);
    }

    // 0x827878B0..0x82787908 -- vmsum3fp128, i.e. all THREE lanes.
    lpRacingLine->mfImmmediateApproachSpeedOfTrafficAhead =
        Magnitude3Traffic(lImmmediateApproachSpeedOfTrafficAhead);

    // 0x8278790C..0x8278794C -- the squared distance accumulated above becomes a real distance.
    lpRacingLine->mfImmediateDistanceToTrafficImpact =
        SqrtTraffic(lpRacingLine->mfImmediateDistanceToTrafficImpact);
}

}
