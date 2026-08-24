#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics (mfSlamSteering, mSlamEffect, mbContactingWall, mass/transform accessors)
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                     // vpu::{Dot, Subtract, Normalize, Mult}
#include "rw/math/vpu/vector4_operation.h"                                     // vpu::Splat

#include <cmath>   // std::fabs

// BrnPhysics::Vehicle::VehicleManager -- the car-vs-car impact HELPER family, reconstructed from
// BURNOUT_X360_ARTIST.XEX (physics mount wave B3b, 2026-08-24). These are the declared-only
// callees the takedown chain in BrnVehicleManager.cpp names at its call sites:
//
//   IsPointBetweenTwoParallelPlanes   @0x825C5660  (120B)
//   CheckForVerticalTakedownSituation @0x825C56D8  (216B)
//   CheckForGrindingAndRubbing        @0x825B5450  (208B)
//   GenerateContactSituation          @0x825B5520  (364B)
//   SendImpactMessage                 @0x825EACF0  (184B)
//   CalculateShuntData                @0x825C7880  (824B)
//   CalculateSlamData                 @0x825C7568  (792B)
//   ApplyShunt                        @0x8261A5B0  (392B)
//   ApplySlam                         @0x8261A738  (404B)
//
// Deep-member offset truth (all reached BY NAME here; console bytes for the record):
//   (idx+0x2B28)*4 == this+44192+idx*4        maeRaceCarTypes[idx]
//   this+idx*4+171684                          mafNoImpactTimeSeconds[idx]      (addis/addi -0x5857)
//   this+idx*4+171744 / +171808                mafVulnerableTimeSeconds / mafTotalVulnerableTime
//   this+idx+171716                            maiPhysicsSlamIndex[idx]         (0x29EC4)
//   this+idx+171936/171944/171952              mau8FramesSincePlayerGrindingOther /
//                                              mau8FramesSinceOtherGrindingPlayer / mabRubbingThisUpdate
//   this+172204 / +172315 / +172380            mePlayerActiveRaceCarIndex / mbIsOnlineGameMode /
//                                              meCurrentGameModeType (0x2A0AC / 0x2A11B / 0x2A15C)
//   this+161968+0x420..0x43C                   mDebugComponent.{meLastSlam*/meLastShunt*} stamps
//   car+0x10/0x40/0x50/0xE0                    mTransform(row0=Right,+0x30=Pos)/mLinearVelocity/mfMass
//   car+0x6D0 / +0x1060.z / +0x1362 / +0x13B0  mDeformableAABB / TimeWithoutTraction lane /
//                                              mbContactingWall / mLastLinearVelocity
//   car+0x1128 / +0x1404                       mSlamEffect.mi8SlamNumber / mfSlamSteering
//
// BSS splat constants, recovered from the 0x82C5Bxxx dynamic-init blob (each writer is a
// lfs + vspltw + stvx128 of a scalar literal):
//   0x82FB8280 <- splat(1.0)  [0x82C5BAA0]     0x82FB8050 <- splat(10.0)  [0x82C5B928]
//   0x82FB9F70 <- splat(0.8)  [0x82C5BAC8]     0x82FB9D10 <- splat(0.9)   [0x82C5BC50]
//   0x82FB9D20 <- splat(1.1)  [0x82C5BC78]     0x82FB9D80 <- splat(0.5)   [0x82C5B830]
//   0x82FB83A0 <- splat(5.0)  [0x82C5B858]
//
// FLAG (VMX modelling, stated once): the vrsqrtefp/vrefp + Newton-refinement cascades
// (Normalize, the mass-ratio reciprocal) are reproduced through the named vpu:: scalar/vector
// helpers, not register-for-register; every load-bearing RESULT (dot products, clamps, signs,
// table reads, store targets) is asm-pinned.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // Per-situation tuning tables (static image data, values read from the image):
        //   0x82F2A1A8 (0.75, 0.25, 1.0, 2.0)  slam situation scale
        //   0x82F2A1B8 (5.0, 3.0, 5.0, 3.0)    slam recovery time      (mode != 3/8)
        //   0x82F2A1C8 (5.0, 3.0, 5.0, 3.0)    slam recovery time      (mode == 3 or 8)
        //   0x82F2A1D8 (2.0, 2.0, 0.0, 2.0)    slam vulnerable-time base
        //   0x82F2A1E8 (1.0, 1.0, 1.5, 2.0)    shunt situation scale
        //   0x82F2A1F8 (2.0, 2.0, 2.0, 2.0)    shunt vulnerable time
        const f32 KAF_SLAM_SITUATION_SCALE[4]   = { 0.75f, 0.25f, 1.0f, 2.0f };   // 0x82F2A1A8
        const f32 KAF_SLAM_RECOVERY_TIME[4]     = { 5.0f, 3.0f, 5.0f, 3.0f };     // 0x82F2A1B8
        const f32 KAF_SLAM_RECOVERY_TIME_ALT[4] = { 5.0f, 3.0f, 5.0f, 3.0f };     // 0x82F2A1C8
        const f32 KAF_SLAM_VULNERABLE_BASE[4]   = { 2.0f, 2.0f, 0.0f, 2.0f };     // 0x82F2A1D8
        const f32 KAF_SHUNT_SITUATION_SCALE[4]  = { 1.0f, 1.0f, 1.5f, 2.0f };     // 0x82F2A1E8
        const f32 KAF_SHUNT_VULNERABLE_TIME[4]  = { 2.0f, 2.0f, 2.0f, 2.0f };     // 0x82F2A1F8

        // sign with zero preserved: the X360 `fcmpu ==0 ? 0 : fsel(x, 1, -1)` idiom
        // (flt_82001C98 = 1.0 / flt_820037C8 = -1.0).
        inline f32 SignOrZero(f32 lfValue)
        {
            if (lfValue == 0.0f)
                return 0.0f;
            return (lfValue >= 0.0f) ? 1.0f : -1.0f;
        }
    }

    // ==============================================================================================
    // IsPointBetweenTwoParallelPlanes @0x825C5660. Pure geometry, `this` unread (the X360 call
    // sites leave r3 holding scratch). Signs of the two plane-side tests: the point is between
    // the planes exactly when (planeA - point)*n and (planeB - point)*n fall on OPPOSITE sides
    // (the zero-dot case contributes sign 0, which can only "differ").
    //   NB the old declared signature (point, planeA, planeB) was FLAGGED inferred and was WRONG:
    //   the asm takes FOUR vector args -- the shared plane normal arrives in v4.
    // ==============================================================================================
    bool VehicleManager::IsPointBetweenTwoParallelPlanes(Vector3 lvPoint, Vector3 lvPlaneA,
                                                         Vector3 lvPlaneB, Vector3 lvPlaneNormal)
    {
        const f32 lfSideA = vpu::Dot(lvPlaneA - lvPoint, lvPlaneNormal);   // vmsum3fp128
        const f32 lfSideB = vpu::Dot(lvPlaneB - lvPoint, lvPlaneNormal);

        // fcmpu ==0 -> 0.0, else fsel(x, 1.0, -1.0); return signA != signB.
        return SignOrZero(lfSideA) != SignOrZero(lfSideB);
    }

    // ==============================================================================================
    // CheckForVerticalTakedownSituation @0x825C56D8. Is the contact point inside 80% of the
    // victim's deformable-AABB FOOTPRINT (fore-aft z and lateral x, in the victim's own frame)?
    // 0x82FB9F70 = splat(0.8). Four vcmpgtfp gates, each early-outs false.
    //   NB the old declared signature (lpVictim, lpOther) was FLAGGED inferred and was WRONG:
    //   the second argument is the CONTACT POINT (v1), not the other car.
    // ==============================================================================================
    bool VehicleManager::CheckForVerticalTakedownSituation(RaceCarPhysics* lpVictim,
                                                           Vector3 lvContactPoint)
    {
        const f32 KF_FOOTPRINT_SCALE = 0.8f;   // 0x82FB9F70 <- splat(flt_8208F9C8)

        const Matrix44Affine& lrTransform = lpVictim->mTransform;                    // +0x10
        const CgsGeometric::AxisAlignedBox& lrAABB = lpVictim->GetDeformableAABB();  // +0x6D0

        const Vector3 lvOffset = lvContactPoint - lrTransform.wAxis;   // vsubfp v1 - row3

        const f32 lfForward = vpu::Dot(lvOffset, lrTransform.zAxis);   // vmsum3fp (row2)
        const f32 lfLateral = vpu::Dot(lvOffset, lrTransform.xAxis);   // vmsum3fp (row0)

        if (!(lrAABB.mMax.z * KF_FOOTPRINT_SCALE > lfForward))   // vcmpgtfp (max.z splat)
            return false;
        if (!(lfForward > lrAABB.mMin.z * KF_FOOTPRINT_SCALE))   // vcmpgtfp (min.z splat)
            return false;
        if (!(lrAABB.mMax.x * KF_FOOTPRINT_SCALE > lfLateral))   // vcmpgtfp (max.x splat)
            return false;
        if (!(lfLateral > lrAABB.mMin.x * KF_FOOTPRINT_SCALE))   // vcmpgtfp (min.x splat)
            return false;

        return true;
    }

    // ==============================================================================================
    // CheckForGrindingAndRubbing @0x825B5450. The player-vs-other grind/rub pre-pass: picks the
    // player side out of the response info, tests both cars' mbContactingWall latches and the
    // two struct speeds, then resets the per-car frames-since counters and raises the rubbing
    // latch on the OTHER car's slot. Returns true when a grind/rub registered this frame.
    // ==============================================================================================
    bool VehicleManager::CheckForGrindingAndRubbing(RaceCarResponseInfo* lpInfo)
    {
        const f32 KF_MIN_GRIND_SPEED = 4.0f;   // flt_8208FA0C

        RaceCarPhysics* lpPlayerCar;
        RaceCarPhysics* lpOtherCar;
        s32             liOtherIndex;
        if (lpInfo->mbRaceCarAIsPlayer)              // +0x52
        {
            lpPlayerCar  = lpInfo->mpRaceCarA;               // +0x24
            lpOtherCar   = lpInfo->mpRaceCarB;               // +0x28
            liOtherIndex = static_cast<s32>(lpInfo->meActiveRaceCarIndexB);   // +0x20
        }
        else if (lpInfo->mbRaceCarBIsPlayer)         // +0x53
        {
            lpPlayerCar  = lpInfo->mpRaceCarB;
            lpOtherCar   = lpInfo->mpRaceCarA;
            liOtherIndex = static_cast<s32>(lpInfo->meActiveRaceCarIndexA);   // +0x1C
        }
        else
        {
            return false;   // neither car is the player
        }

        const bool lbPlayerOnWall = lpPlayerCar->mbContactingWall;   // lbz +0x1362
        const bool lbOtherOnWall  = lpOtherCar->mbContactingWall;

        if (lbPlayerOnWall && lbOtherOnWall)
            return false;   // both against the wall -- no grind classification

        // Both struct speeds must clear the 4.0 floor (asm reads the fixed A/B slots).
        if (lpInfo->mfRaceCarASpeed < KF_MIN_GRIND_SPEED)    // +0x5C
            return false;
        if (lpInfo->mfRaceCarBSpeed < KF_MIN_GRIND_SPEED)    // +0x60
            return false;

        if (lbOtherOnWall)
            mau8FramesSincePlayerGrindingOther[liOtherIndex] = 0;   // stbx 0 @ +171936
        if (lbPlayerOnWall)
            mau8FramesSinceOtherGrindingPlayer[liOtherIndex] = 0;   // stbx 0 @ +171944

        mabRubbingThisUpdate[liOtherIndex] = true;                  // stbx 1 @ +171952
        return true;
    }

    // ==============================================================================================
    // GenerateContactSituation @0x825B5520. Maps the (aggressor type, victim type) pair onto the
    // EImpactSituation the slam/shunt tuning tables index. Four independent `if` ladders exactly
    // as the console emits them (each re-reads the type array), then the two range tripwires.
    // ==============================================================================================
    void VehicleManager::GenerateContactSituation(RaceCarResponseInfo* lpInfo)
    {
        lpInfo->meImpactSitutation = E_IMPACT_SITUATION_INVALID;   // stw -1 @+0x108

        const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);   // +0xF8
        const s32 liVictim    = static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex);      // +0xFC

        if (maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_PLAYER
            && maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_AI)
        {
            lpInfo->meImpactSitutation = E_IMPACT_SITUATION_PLAYER_ON_AI;
        }
        if (maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_AI
            && maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_PLAYER)
        {
            lpInfo->meImpactSitutation = E_IMPACT_SITUATION_AI_ON_PLAYER;
        }
        if (maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_AI
            && maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_AI)
        {
            lpInfo->meImpactSitutation = E_IMPACT_SITUATION_AI_ON_AI;
        }
        if ((maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_PLAYER
             && maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            || (maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_NETWORK
                && maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_PLAYER))
        {
            lpInfo->meImpactSitutation = E_IMPACT_SITUATION_NETWORK;
        }

        CGS_ASSERT(lpInfo->meImpactSitutation > E_IMPACT_SITUATION_INVALID,
                   "lpInfo->meImpactSitutation > E_IMPACT_SITUATION_INVALID");   // :6668
        CGS_ASSERT(lpInfo->meImpactSitutation < E_IMPACT_SITUATION_COUNT,
                   "lpInfo->meImpactSitutation < E_IMPACT_SITUATION_COUNT");     // :6669
    }

    // HasRaceCarHadRecentImpact @0x825B4EB8 is NOT here: the link flagged the duplicate --
    // its real body already lives in the mounted BrnVehicleManagerPlayerStats.cpp (identical
    // mafNoImpactTimeSeconds[idx] > 0 read; the "body unrecovered" note in the header was the
    // TENTH stale banner this campaign).

    // ==============================================================================================
    // ShouldRaceCarCrashOnCarImpact @0x825C6FF8 (168B) -- FOUND during this wave (the header's
    // "no standalone export in this dossier" was an EXPORT-SET HOLE, not a missing body), but
    // NOT reconstructed here: the asm takes TWO ADDITIONAL VECTOR ARGS (v1/v2) the PC signature
    // never modelled, and the real predicate is
    //     v1 * min(otherCar.mfMass, splat@0x82FB8350) / victimCar.mfMass
    //        * mafVulnerabilityFactor[victim]                        >  victim.mpAttribs[+0x280] * v2
    // where BOTH caller sites (@0x8263E1B4.., @0x8263DC30..) build v1/v2 through a boost-flag
    // vsel over the BSS pair @0x8327F240 whose writers are not yet traced. Reconstructing the
    // 3-arg PC shape would silently drop the boost scaling; a quiet default would silently
    // decide crashes. So this is a LOUD trap, per the standing pattern for unreachable-today
    // code: NOTHING on this build can reach it (it needs TWO live race cars in contact), and
    // the moment that changes this assert names exactly what must be reconstructed.
    // ==============================================================================================
    bool VehicleManager::ShouldRaceCarCrashOnCarImpact(s32 /*liVictimActiveRaceCarIndex*/,
                                                       RaceCarPhysics* /*lpVictim*/,
                                                       RaceCarPhysics* /*lpOther*/)
    {
        CGS_ASSERT(false, "VehicleManager::ShouldRaceCarCrashOnCarImpact: FLAG trap -- the real "
                          "body @0x825C6FF8 takes two extra VECTOR args (boost-scaled by the "
                          "0x8327F240 vsel pair); reconstruct it + both call-site vector builds "
                          "before two race cars can trade paint");
        return false;
    }

    // ==============================================================================================
    // SendImpactMessage @0x825EACF0. Builds one ImpactEvent on the stack in field order and
    // appends it to the caller-supplied queue (a network-victim impact is reported through the
    // vehicle output interface instead of being applied locally).
    // ==============================================================================================
    void VehicleManager::SendImpactMessage(VehicleOutputInterface::ImpactEventQueue* lpImpactEventQueue,
                                           EImpactType leImpactType,
                                           EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                           EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                           Vector3 lvDirection,
                                           f32 lfMagnitude, f32 lfDuration,
                                           f32 lfSteeringDirection, f32 lfRecoveryTime,
                                           u8 lu8Score)
    {
        CGS_ASSERT(lpImpactEventQueue != 0, "lpImpactEventQueue");   // :9145

        ImpactEvent lEvent;
        lEvent.mDirection                    = lvDirection;                    // stvx  @+0x00
        lEvent.meImpactType                  = leImpactType;                   // stw   @+0x10
        lEvent.meAggressorActiveRaceCarIndex = leAggressorActiveRaceCarIndex;  // stw   @+0x14
        lEvent.meVictimActiveRaceCarIndex    = leVictimActiveRaceCarIndex;     // stw   @+0x18
        lEvent.mfMagnitude                   = lfMagnitude;                    // stfs  @+0x1C
        lEvent.mfDuration                    = lfDuration;                     // stfs  @+0x20
        lEvent.mfSteeringDirection           = lfSteeringDirection;            // stfs  @+0x24
        lEvent.mfRecoveryTime                = lfRecoveryTime;                 // stfs  @+0x28
        lEvent.muScore                       = lu8Score;                       // stb   @+0x2C

        lpImpactEventQueue->AddEvent(lEvent);
    }

    // ==============================================================================================
    // CalculateShuntData @0x825C7880. Derives the shunt push {direction, magnitude, vulnerable
    // time, steering direction} from the contact normal, the two cars' poses/velocities/masses
    // and the impact situation.
    // ==============================================================================================
    void VehicleManager::CalculateShuntData(RaceCarResponseInfo* lpInfo,
                                            Vector3* lpvWorldDirection,
                                            VecFloat* lpvfMagnitude,
                                            f32* lpfVulnerableTime,
                                            f32* lpfSteeringDirection)
    {
        const s32 liVictim    = static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex);      // +0xFC
        const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);   // +0xF8
        const s32 liSituation = static_cast<s32>(lpInfo->meImpactSitutation);              // +0x108

        RaceCarPhysics* const lpVictimCar    = &maRaceCarVehicles[liVictim];
        RaceCarPhysics* const lpAggressorCar = &maRaceCarVehicles[liAggressor];

        CGS_ASSERT(liVictim != liAggressor,
                   "leContactedActiveRaceCarIndex != leAggressorActiveRaceCarIndex");   // :8873
        CGS_ASSERT(liSituation < E_IMPACT_SITUATION_COUNT,
                   "leImpactSituation < E_IMPACT_SITUATION_COUNT");                     // :8874

        const Vector3 lvNormal   = lpInfo->mpContact->mNormal;                          // contact+0x30
        const Vector3 lvToVictim = lpVictimCar->mTransform.wAxis
                                 - lpAggressorCar->mTransform.wAxis;                    // vsubfp (+0x40)

        // Orient the contact normal from the aggressor toward the victim (vsel on the dot sign).
        Vector3 lvOriented = (vpu::Dot(lvToVictim, lvNormal) >= 0.0f) ? lvNormal
                                                                      : vpu::Negate(lvNormal);

        // Project out the victim's up axis and normalize -> the horizontal push direction
        // (vmsum3fp + vrsqrtefp 2-Newton, modelled through vpu::Normalize).
        const Vector3& lvUp = lpVictimCar->mTransform.yAxis;
        const Vector3 lvDirection = vpu::Normalize(lvOriented - lvUp * vpu::Dot(lvUp, lvOriented));

        // Closing speed along the push direction.
        const Vector3 lvClosingVel = lpAggressorCar->mLinearVelocity
                                   - lpVictimCar->mLinearVelocity;                      // +0x50
        const f32 lfClosingSpeed = vpu::Dot(lvDirection, lvClosingVel);

        // Steer AWAY from the impact side: -sign of the aggressor's lateral offset in the
        // victim's frame (fsel on dot(toVictim, victim right)).
        const f32 lfLateral = vpu::Dot(lvToVictim, lpVictimCar->mTransform.xAxis);
        const f32 lfSteer   = -((lfLateral >= 0.0f) ? 1.0f : -1.0f);

        // Mass ratio, clamped [0.9, 1.1] (vrefp + Newton against mfMass, both lanes splat).
        f32 lfMassRatio = lpAggressorCar->GetMass().x / lpVictimCar->GetMass().x;       // +0xE0
        if (lfMassRatio < 0.9f) lfMassRatio = 0.9f;    // 0x82FB9D10 <- splat(0.9)
        if (lfMassRatio > 1.1f) lfMassRatio = 1.1f;    // 0x82FB9D20 <- splat(1.1)

        f32 lfMagnitude = lfClosingSpeed * lfMassRatio;
        f32 lfVulnerableTime;
        if (liSituation != E_IMPACT_SITUATION_INVALID)
        {
            lfMagnitude     *= KAF_SHUNT_SITUATION_SCALE[liSituation];
            lfVulnerableTime = KAF_SHUNT_VULNERABLE_TIME[liSituation];

            // Boost shunt doubles the push unless the aggressor is AI.
            if (lpInfo->meImpactType == E_IMPACT_BOOST_SHUNT
                && maeRaceCarTypes[liAggressor] != BrnWorld::E_RACE_CAR_TYPE_AI)
            {
                lfMagnitude *= 2.0f;   // vcfsx(2)
            }
        }
        else
        {
            lfVulnerableTime = 2.0f;   // lfs flt_82F2A1F8 (the table's first element)
        }

        // Offline, no network cars, player aggressor: soften (magnitude x0.5, vuln time x0.25).
        if (meCurrentGameModeType == -1                        // +172380 (0x2A15C)
            && !lpInfo->mbRaceCarBIsNetworkCar                 // +0x55
            && !lpInfo->mbRaceCarAIsNetworkCar                 // +0x54
            && maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_PLAYER)
        {
            lfMagnitude      *= 0.5f;    // vcfsx(1,1)
            lfVulnerableTime *= 0.25f;   // 0.5 * 0.5
        }

        const f32 lfRawClosingSpeed = lfClosingSpeed;   // the debug stamp keeps the pre-clamp value
        if (lfMagnitude < 0.5f) lfMagnitude = 0.5f;     // vmaxfp vs 0x82FB9D80 <- splat(0.5)
        if (lfMagnitude > 5.0f) lfMagnitude = 5.0f;     // vminfp vs 0x82FB83A0 <- splat(5.0)

        *lpvWorldDirection    = lvDirection;                   // stvx -> out r5
        *lpvfMagnitude        = vpu::Splat(lfMagnitude);       // stvx -> out r6 (uniform lanes)
        *lpfVulnerableTime    = lfVulnerableTime;              // stfs -> out r7
        *lpfSteeringDirection = lfSteer;                       // stfs -> out r8

        mDebugComponent.meLastShuntImpactSituation = static_cast<EImpactSituation>(liSituation); // +0x434
        mDebugComponent.mfLastShuntMagnitude       = lfMagnitude;                                // +0x438
        mDebugComponent.mfLastShuntClosingSpeed    = lfRawClosingSpeed;                          // +0x43C
    }

    // ==============================================================================================
    // CalculateSlamData @0x825C7568. Derives the slam {duration, counter-slam duration, steering
    // sign, recovery time, direction, score byte, vulnerable time} from the two cars' slam
    // steering, masses and the impact situation.
    // ==============================================================================================
    void VehicleManager::CalculateSlamData(RaceCarResponseInfo* lpInfo,
                                           f32* lpfDuration,
                                           f32* lpfCounterDuration,
                                           f32* lpfSteeringDirection,
                                           f32* lpfRecoveryTime,
                                           Vector3* lpvDirection,
                                           u8* lpu8Score,
                                           f32* lpfVulnerableTime)
    {
        const s32 liVictim    = static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex);      // +0xFC
        const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);   // +0xF8
        const s32 liSituation = static_cast<s32>(lpInfo->meImpactSitutation);              // +0x108
        const u8  lu8Score    = static_cast<u8>(lpInfo->muImpactScore & 0xFFu);            // clrlwi 24

        RaceCarPhysics* const lpVictimCar    = &maRaceCarVehicles[liVictim];
        RaceCarPhysics* const lpAggressorCar = &maRaceCarVehicles[liAggressor];

        // The incoming slam number: the victim's current slam id + 1, capped at 2.
        s32 liSlamNumber = static_cast<s32>(lpVictimCar->mSlamEffect.mi8SlamNumber) + 1;   // lbz +0x1128
        if (liSlamNumber > 2)
            liSlamNumber = 2;

        CGS_ASSERT(liVictim != liAggressor,
                   "leContactedActiveRaceCarIndex != leAggressorActiveRaceCarIndex");   // :8760
        CGS_ASSERT(liSituation > E_IMPACT_SITUATION_INVALID,
                   "leImpactSituation > E_IMPACT_SITUATION_INVALID");                   // :8761
        CGS_ASSERT(liSituation < E_IMPACT_SITUATION_COUNT,
                   "leImpactSituation < E_IMPACT_SITUATION_COUNT");                     // :8762

        const f32 lfAggressorSteer = lpAggressorCar->mfSlamSteering;    // lfs +0x1404
        const f32 lfSign           = SignOrZero(lfAggressorSteer);
        const f32 lfAbsSteer       = lfSign * lfAggressorSteer;         // |aggressor slam steering|
        const f32 lfVictimSteer    = lpVictimCar->mfSlamSteering;

        // Mass ratio, clamped [0.9, 1.1] (vrefp + Newton on the splat mfMass registers).
        f32 lfMassRatio = lpAggressorCar->GetMass().x / lpVictimCar->GetMass().x;       // +0xE0
        if (lfMassRatio < 0.9f) lfMassRatio = 0.9f;
        if (lfMassRatio > 1.1f) lfMassRatio = 1.1f;

        // The steering-derived ease: cap |steer| at 1.2, then 1 - (1 - cap)^4 (the console
        // squares the (1-cap) term twice: fmuls then the fnmsubs re-multiply).
        f32 lfCap = lfAbsSteer;
        if (lfCap > 1.2f) lfCap = 1.2f;                        // flt_82009B84
        const f32 lfOneMinusSq = (1.0f - lfCap) * (1.0f - lfCap);
        const f32 lfEase       = 1.0f - lfOneMinusSq * lfOneMinusSq;

        f32 lfDuration = lfEase * lfMassRatio * KAF_SLAM_SITUATION_SCALE[liSituation];

        // Slam direction: the victim's right axis, flipped against the aggressor's steer sign.
        const Vector3 lvDirection = lpVictimCar->mTransform.xAxis * (-lfSign);   // +0x750 row0

        // Counter-slam (the aggressor's recoil) arms only when the victim steers AGAINST the
        // aggressor's steer (victimSteer * sign < -0.1): clamp(|victimSteer * 0.5|, 0.1, 1.0).
        f32 lfCounterDuration = 0.0f;
        if (lfVictimSteer * lfSign < -0.1f)                    // flt_8200D530
        {
            f32 lfCounter = std::fabs(lfVictimSteer * 0.5f);   // flt_82F2A4F8
            if (lfCounter < 0.1f) lfCounter = 0.1f;            // flt_82F2A4F4
            if (lfCounter > 1.0f) lfCounter = 1.0f;            // flt_82F2A4F0
            lfCounterDuration = lfCounter;
        }

        // Offline, no network cars, player aggressor: soften both durations x0.7.
        if (meCurrentGameModeType == -1
            && !lpInfo->mbRaceCarBIsNetworkCar
            && !lpInfo->mbRaceCarAIsNetworkCar
            && maeRaceCarTypes[liAggressor] == BrnWorld::E_RACE_CAR_TYPE_PLAYER)
        {
            lfCounterDuration *= 0.7f;   // flt_82004C68
            lfDuration        *= 0.7f;
        }

        // Recovery time: game modes 3 and 8 read the alternate table (identical values as shipped).
        const f32* lpfRecoveryTable =
            (meCurrentGameModeType == 3 || meCurrentGameModeType == 8) ? KAF_SLAM_RECOVERY_TIME_ALT
                                                                       : KAF_SLAM_RECOVERY_TIME;
        const f32 lfRecovery       = lpfRecoveryTable[liSituation];
        const f32 lfVulnerableTime = KAF_SLAM_VULNERABLE_BASE[liSituation] + lfDuration;

        mDebugComponent.mfLastSlamDuration       = lfDuration;                                  // +0x428
        mDebugComponent.meLastSlamImpactSituation = static_cast<EImpactSituation>(liSituation); // +0x420
        mDebugComponent.mfLastSlamBaseDuration   = 0.0f;                                        // +0x42C
        mDebugComponent.miLastSlamNumber         = liSlamNumber;                                // +0x424
        mDebugComponent.mfLastSlamMassFactor     = 0.0f;                                        // +0x430

        *lpfDuration          = lfDuration;         // stfs -> r5 out
        *lpfSteeringDirection = lfSign;             // stfs -> r7 out (the +/-1 steer sign)
        *lpfCounterDuration   = lfCounterDuration;  // stfs -> r6 out
        *lpfRecoveryTime      = lfRecovery;         // stfs -> r8 out
        *lpu8Score            = lu8Score;           // stb  -> r10 out
        *lpfVulnerableTime    = lfVulnerableTime;   // stfs -> stack out
        *lpvDirection         = lvDirection;        // stvx -> r9 out
    }

    // ==============================================================================================
    // ApplyShunt @0x8261A5B0. Calculate the shunt, then either report it over the network (the
    // victim is a NETWORK car) or apply it locally: AddShunt + AddSlam on the victim's physics,
    // the physics-slam slot bookkeeping, and (except for nudges) the vulnerability arm.
    // ==============================================================================================
    void VehicleManager::ApplyShunt(RaceCarResponseInfo* lpInfo)
    {
        Vector3  lvWorldDirection;
        VecFloat lvfMagnitude;
        f32      lfVulnerableTime;
        f32      lfSteer;
        CalculateShuntData(lpInfo, &lvWorldDirection, &lvfMagnitude, &lfVulnerableTime, &lfSteer);

        const s32 liVictim = static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex);   // +0xFC

        if (maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
        {
            // Report the shunt to the network victim's owner. Score 0xFF is the console's
            // network-shunt sentinel; magnitude is the calculated vector's (uniform) lane.
            SendImpactMessage(&lpInfo->mpVehicleOutputInterface->GetImpactEventQueue(),   // +8 -> +0x2310
                              lpInfo->meImpactType,                                   // +0xF4
                              mePlayerActiveRaceCarIndex,                             // +172204
                              lpInfo->meVictimActiveRaceCarIndex,
                              lvWorldDirection,
                              lvfMagnitude.x,        // lfs of the magnitude vector
                              0.2f,                  // flt_82004744 -- the fixed shunt-slam duration
                              lfSteer,
                              1.0f,                  // flt_82001C98 -- the fixed recovery time
                              0xFFu);
            return;
        }

        const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);   // +0xF8
        CGS_ASSERT(liVictim != liAggressor,
                   "leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex");   // :9840

        RaceCarPhysics* const lpVictimCar = &maRaceCarVehicles[liVictim];

        // The physical push (speed-increase-to-quit is the fixed splat(10.0) @0x82FB8050).
        const s8 li8ShuntSlot = lpVictimCar->AddShunt(lvfMagnitude, lvWorldDirection,
                                                      vpu::Splat(10.0f),
                                                      static_cast<s8>(liAggressor));
        maiPhysicsSlamIndex[liVictim] = li8ShuntSlot;                       // stbx @+171716

        // ...and the steering slam that rides it (duration 0.2, recovery 1.0; taper only online).
        const s8 li8SlamSlot = lpVictimCar->AddSlam(mbIsOnlineGameMode,     // lbzx @+172315
                                                    0.2f, lfSteer, 1.0f,
                                                    static_cast<s8>(liAggressor));
        maiPhysicsSlamIndex[liVictim] = li8SlamSlot;                        // the slam wins the slot

        if (lpInfo->meImpactType != E_IMPACT_NUDGE)   // +0xF4 != 2
        {
            mafVulnerableTimeSeconds[liVictim] = lfVulnerableTime;          // stfsx @+171744
            mafTotalVulnerableTime[liVictim]   = lfVulnerableTime;          // stfsx @+171808
        }
    }

    // ==============================================================================================
    // ApplySlam @0x8261A738. Calculate the slam, then either report it over the network (the
    // victim is a NETWORK car) or apply it locally: AddSlam on the victim, the victim's slam
    // steering reset, the offline counter-slam on the aggressor, and (except trading paint)
    // the vulnerability arm.
    // ==============================================================================================
    void VehicleManager::ApplySlam(RaceCarResponseInfo* lpInfo)
    {
        f32      lfDuration, lfCounterDuration, lfSteer, lfRecoveryTime, lfVulnerableTime;
        Vector3  lvDirection;
        u8       lu8Score;
        CalculateSlamData(lpInfo, &lfDuration, &lfCounterDuration, &lfSteer, &lfRecoveryTime,
                          &lvDirection, &lu8Score, &lfVulnerableTime);

        const s32 liVictim = static_cast<s32>(lpInfo->meVictimActiveRaceCarIndex);   // +0xFC

        if (maeRaceCarTypes[liVictim] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
        {
            SendImpactMessage(&lpInfo->mpVehicleOutputInterface->GetImpactEventQueue(),
                              lpInfo->meImpactType,
                              mePlayerActiveRaceCarIndex,
                              lpInfo->meVictimActiveRaceCarIndex,
                              lvDirection,
                              0.0f,                  // flt_82001CC0 -- a slam has no push magnitude
                              lfDuration, lfSteer, lfRecoveryTime,
                              lu8Score);
            return;
        }

        const s32 liAggressor = static_cast<s32>(lpInfo->meAggressorActiveRaceCarIndex);   // +0xF8
        CGS_ASSERT(liVictim != liAggressor,
                   "leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex");   // :9840

        RaceCarPhysics* const lpVictimCar = &maRaceCarVehicles[liVictim];

        const s8 li8SlamSlot = lpVictimCar->AddSlam(mbIsOnlineGameMode,      // lbz @+172315
                                                    lfDuration, lfSteer, lfRecoveryTime,
                                                    static_cast<s8>(liAggressor));
        maiPhysicsSlamIndex[liVictim] = li8SlamSlot;                         // stbx @+171716
        lpVictimCar->ClearSlamSteering();                                    // stfs 0 @+0x1404

        // Offline only: the aggressor recoils with the counter-slam (negated steer, victim id).
        if (!mbIsOnlineGameMode)
        {
            maRaceCarVehicles[liAggressor].AddSlam(false,
                                                   lfCounterDuration, -lfSteer, lfRecoveryTime,
                                                   static_cast<s8>(liVictim));
        }

        if (lpInfo->meImpactType != E_IMPACT_TRADING_PAINT)   // +0xF4 != 1
        {
            mafVulnerableTimeSeconds[liVictim] = lfVulnerableTime;           // stfsx @+171744
            mafTotalVulnerableTime[liVictim]   = lfVulnerableTime;           // stfsx @+171808
        }
    }
}
}
