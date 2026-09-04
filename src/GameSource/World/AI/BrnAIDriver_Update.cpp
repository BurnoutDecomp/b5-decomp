#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"     // the file-scope KF_ tunables
#include "GameSource/World/AI/BrnAICar.h"                  // AICar (named members + accessors)
#include "GameSource/World/AI/BrnAIUtils.h"                // Find{Signed,Unsigned}AngleBetween2DVectors
#include "GameSource/World/AI/BrnAIAggression.h"           // AIAggression (the embedded sub-machine)
#include "GameSource/World/AI/Route/BrnRacingLine.h"       // RacingLine
#include "GameSource/World/AI/Route/BrnRoute.h"            // Route / RouteNode
#include "GameSource/World/AI/PID/BrnPIDController.h"      // PIDController::Prepare
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // BrnTraffic::BrnTrafficIO::TrafficAIEntity

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // [FLAG PC witness] the [drv] behaviour-transition trace

#include "rw/math/vpu/vector3_operation.h"                 // vpu::Magnitude / vpu::Dot

#include <cmath>   // std::sqrt, std::fabs, std::isfinite

// =================================================================================================
// BrnAIDriver.cpp PARTFILE -- the Update-path callees.
//
// BrnAIDriver.cpp bodies the spine (Construct / SetAICar / Prepare / Update / UpdateBehaviour /
// UpdateSteeringAngle / CalculateSteeringAngle / CalculateCarControls / ComputeRouteDirection /
// GetTargetPosition / AttemptToDriveAtDesiredSpeed / CorneringTopSpeed / ProximitySpeed /
// ChooseRaceSteeringFan / SetDrivingFanBiases / IsInvulnerable / IsOnStartLine). This file bodies
// everything that spine calls. Both files share BrnAIDriver_Constants.h; nothing here duplicates a
// symbol from the other (one body per symbol -- LNK2005 checked).
//
// THE KEYSTONE'S CALLEE ORDER -- BrnAI::AIDriver::Update @0x8279AEB0, with the asm address of each
// call. Register map from the asm + AIModule::UpdateDrivers @0x8279B334:
//   r3 = this, f1 = lfTimeStep, r4 = SKIPPED (f1's GPR slot, PPC float-arg rule),
//   r5 = (slot == AIModule::miLineUpdateTokenCounter), v1 = player car position,
//   r6 = lpPlayerCar, r7 = mbDoInRangeCatchup, r8 = &AIModule::mRandom.
//
//   0x8279AEE8  if (!mbIsActive (0x1D69) || !lpPlayerCar) -> clear the 8 control fields, return
//   0x8279AF00  UpdatePlayerTimers(dt, lpPlayerCar)                        @0x82770320  [this file]
//   0x8279AF08  SetDrivingFanBiases(lpPlayerCar)                           @0x82770428  [spine]
//   0x8279AF10  if (mRacingLine.mbIsInitialised (0x1AF0)):
//   0x8279AF18    AICar::GetPosition(mpCar); BrnMath::Flatten
//   0x8279AF40    mRacingLine.mbCentreLineHereKnown (0x1AF1) =
//                   RacingLineGenerator::GetCentreCentreLineHere(this+0x1B30, this+0xF20,
//                                                                this+0x1B00, this+0x1B10)
//                 else mbCentreLineHereKnown = 0                                        [A6 gate]
//   0x8279AF60  if (mpCar->mbIsCrashing (0x1542)):
//   0x8279AF74      UpdateBehaviour(dt)                                    @0x8279A680  [spine]
//   0x8279AF84      mfInvulnerableTime (0x1D0C) = 2.0
//                 else:
//   0x8279AF9C      if (mfInvulnerableTime > 0) mfInvulnerableTime -= dt
//   0x8279AFB8      m2DCarPos (0x1CD0) = 2D(mpCar->GetPosition())
//   0x8279AFD8      mfCarSpeed (0x1D00) = mpCar->GetSpeed()
//   0x8279AFEC      if (mpCar->mbIsInAir (0x1541)) { clear the 8 control fields;
//   0x8279B020                                       UpdateSteeringAngle(0.0) }          [spine]
//                   else:
//   0x8279B03C        CalcDistanceFromPlayer(v1 = player position)         @0x8276E1C8  [this file]
//   0x8279B044        InitialiseRacingLine()                               @0x82792DF0  [this file]
//   0x8279B054        UpdateBehaviour(dt)                                  @0x8279A680  [spine]
//   0x8279B070        if (mbIsRacingLineInitialised (0x1D68)) {
//   0x8279B07C          if (!mbCurrentRouteComplete (0x1D6A)) GenerateRacingLine(dt) @0x8277C4E8 [this file]
//   0x8279B090          if (mbIsRacingLineInitialised) UpdateBrakingAnticipationData() @0x827964C0 [this file]
//                     }
//   0x8279B0A8        CalculateCarControls(dt, v1, r7, r8)                 @0x827998C0  [spine]
//   0x8279B0C4        assert(RwMath::IsValid(mfSteeringAngle))  (:823)
//
// and from there:
//   CalculateCarControls -> CalculateDesiredSpeed @0x827934C0 -> HardShoulderSpeed @0x827930B8
//                                                              -> ProximitySpeed   @0x82770800
//                                                              -> CorneringTopSpeed @0x8277D0F0
//                                                              -> SteeringFan::GetSpeedRatio @0x82779B90
//                        -> DoDrivingBehaviour @0x82799660 -> CalculateSteeringAngle @0x8277CD18
//                                                          -> CheckForSpeedMatch @0x82793020
//                                                          -> AttemptToDriveAtDesiredSpeed @0x827706D8
//                                                             -> CheckForBoosting @0x827705E0
//                                                          -> EstimateNeedForDrifting @0x82793438
//                                                          -> Determine180Turn @0x8277C758
//                                                          -> StartDrift @0x8277C870
//                                                          -> AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0
//                                                          -> DetermineDriftSteeringAngle @0x827931D0
//                        -> DoSlowTurn @0x8277CA88
//   UpdateBehaviour      -> UpdateStuck @0x82766440 -> (IsStuck @0x82766670 is called by AIModule)
//                        -> UpdateQuickTurn @0x8278B100 / DoSlowTurnBehaviour @0x8277C968
//                        -> AIAggression::Update
//
// FOUR of these have NO IDA export (.ida-exports has no .json and names.tsv has no row); their
// bodies below were read straight from the image bytes with capstone, and each carries its address
// range in the banner: CalculateDesiredSpeed @0x827934C0..0x82793560, DoDrivingBehaviour
// @0x82799660..0x827998BC, UpdateBrakingAnticipationData @0x827964C0..0x827965E4 and
// CalcDistanceFromPlayer @0x8276E1C8..0x8276E26C.
// =================================================================================================

namespace BrnAI
{
    // TU-local de-SIMD helpers (same shape as BrnAIDriver.cpp's; static, so no ODR clash).
    namespace
    {
        inline bool IsFiniteU(f32 lfValue) { return std::isfinite(lfValue); }

        Vector2 Normalize2DU(Vector2 lVector)
        {
            const f32 lfLenSq  = lVector.x * lVector.x + lVector.y * lVector.y;
            const f32 lfInvLen = (lfLenSq > 0.0f) ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;
            Vector2 lResult;
            lResult.x = lVector.x * lfInvLen;
            lResult.y = lVector.y * lfInvLen;
            lResult.z = 0.0f;
            lResult.w = 0.0f;
            return lResult;
        }

        // BrnMath::Flatten @0x822CB8E8 -- project onto the XZ GROUND plane (drop the height lane).
        // ⛔ FIXED 2026-09-04 (aiwave R7): was `lResult.y = lVector.y` (world HEIGHT). The vperm
        // mask Flatten loads (unk_82CDA450 = 00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03)
        // sources lane1 from vB bytes 0x18..0x1B == lane 2 == .z, and the open-coded twin in
        // CalculateSteeringAngle (`vrlimi128 v127,v0,8,0 ; vrlimi128 v127,v0,4,1` @0x8277CD78/8C)
        // does the same. See the long banner on BrnAIDriver.cpp's To2D.
        Vector2 To2DU(Vector3 lVector)
        {
            Vector2 lResult;
            lResult.x = lVector.x;
            lResult.y = lVector.z;   // ground plane: Vector2.y IS world Z
            lResult.z = 0.0f;
            lResult.w = 0.0f;
            return lResult;
        }

        inline f32 SaturateU(f32 lfValue)
        {
            if (lfValue < 0.0f) return 0.0f;
            if (lfValue > 1.0f) return 1.0f;
            return lfValue;
        }

        inline f32 Dot2D(Vector2 lA, Vector2 lB) { return lA.x * lB.x + lA.y * lB.y; }

        // The two half-extent vectors the avoidance box is built from. On the X360 these are two
        // lazily-initialised statics (unk_8300DC90 == (2,0,..), unk_8300DC80 == (0,2,..); the
        // `dword_8300DCA0 & 1/2` bits are the one-shot guards). Const data on the host.
        Vector2 MakeHalfExtentX()
        {
            Vector2 lV; lV.x = KF_NEARBY_VEHICLE_HALF_EXTENT; lV.y = 0.0f; lV.z = 0.0f; lV.w = 0.0f;
            return lV;
        }
        Vector2 MakeHalfExtentY()
        {
            Vector2 lV; lV.x = 0.0f; lV.y = KF_NEARBY_VEHICLE_HALF_EXTENT; lV.z = 0.0f; lV.w = 0.0f;
            return lV;
        }

        // Fill one NearbyVehicle's four HNG boundary lines from a closed 4-corner quad, in the
        // console's order: line[i] runs corner[i] -> corner[i+1] (wrapping).
        void FillHNGQuad(NearbyVehicle* lpVehicle, Vector2 lC0, Vector2 lC1, Vector2 lC2, Vector2 lC3)
        {
            lpVehicle->maHNGLines[0].mfStartX = lC0.x; lpVehicle->maHNGLines[0].mfStartY = lC0.y;
            lpVehicle->maHNGLines[0].mfEndX   = lC1.x; lpVehicle->maHNGLines[0].mfEndY   = lC1.y;
            lpVehicle->maHNGLines[1].mfStartX = lC1.x; lpVehicle->maHNGLines[1].mfStartY = lC1.y;
            lpVehicle->maHNGLines[1].mfEndX   = lC2.x; lpVehicle->maHNGLines[1].mfEndY   = lC2.y;
            lpVehicle->maHNGLines[2].mfStartX = lC2.x; lpVehicle->maHNGLines[2].mfStartY = lC2.y;
            lpVehicle->maHNGLines[2].mfEndX   = lC3.x; lpVehicle->maHNGLines[2].mfEndY   = lC3.y;
            lpVehicle->maHNGLines[3].mfStartX = lC3.x; lpVehicle->maHNGLines[3].mfStartY = lC3.y;
            lpVehicle->maHNGLines[3].mfEndX   = lC0.x; lpVehicle->maHNGLines[3].mfEndY   = lC0.y;
        }
    }

    // ================================================================================
    // NearbyVehicles::Next @0x827667D8
    //
    // Claim the next free slot: both asserts, then the unconditional `++miCount`.
    // ================================================================================
    void NearbyVehicles::Next()
    {
        CGS_ASSERT(miCount >= 0, "miCount >= 0");                                 // :2921
        CGS_ASSERT(miCount < KI_MAX_NEARBY_VEHICLES, "miCount < KI_MAX_NEARBY_TRAFFIC");  // :2922
        ++miCount;                                                                // stw 0x700
    }

    // ================================================================================
    // NearbyVehicles::GetVehiclePointer @0x82766870
    //
    // &mVehicle[liEntry] (stride 0x70 on both console and host -- NearbyVehicle is pointer-free).
    // ================================================================================
    NearbyVehicle* NearbyVehicles::GetVehiclePointer(s32 liEntry)
    {
        CGS_ASSERT(liEntry >= 0, "liEntry >= 0");                                 // :2942
        CGS_ASSERT(liEntry < KI_MAX_NEARBY_VEHICLES, "liEntry < KI_MAX_NEARBY_TRAFFIC");  // :2943
        return &mVehicle[liEntry];                                                // mulli 0x70; add
    }

    // ================================================================================
    // UpdatePlayerTimers @0x82770320
    //
    // Three per-frame timers about the PLAYER, kept on this driver:
    //   mfPlayerSlowSpeedTime  -- climbs (capped at 6 s) while the player is slower than THIS
    //                             car's "decent" speed and is not protected; decays to 0 otherwise
    //                             (the two fsel pairs @0x827703B0 / @0x827703C0 are min/max).
    //   mfPlayerTimeSinceCrash -- 0 while the player is crashing, else += dt.
    //   mfPlayerTimeSinceAIDriven -- += dt while the player car is AI-driven, else 0.
    // With no player car both "time since" timers are zeroed and nothing else runs.
    // ================================================================================
    void AIDriver::UpdatePlayerTimers(f32 lfTimeStep, AICar* lpPlayerCar)
    {
        if (lpPlayerCar == 0)
        {
            mfPlayerTimeSinceCrash    = 0.0f;   // 0x1D34
            mfPlayerTimeSinceAIDriven = 0.0f;   // 0x1D38
            return;
        }

        const f32 lfPlayerSpeed = lpPlayerCar->GetSpeed();
        const f32 lfDecentSpeed = mpCarHost->GetDecentSpeed();

        if (lfPlayerSpeed >= lfDecentSpeed || IsPlayerProtected(lpPlayerCar))
        {
            // decay, floored at 0 (fsel f0, f0, f0, 0.0)
            const f32 lfNew = mfPlayerSlowSpeedTime - lfTimeStep;
            mfPlayerSlowSpeedTime = (lfNew >= 0.0f) ? lfNew : 0.0f;
        }
        else
        {
            // climb, capped at 6 s (fsel f0, (sum - 6.0), 6.0, sum)
            const f32 lfNew = mfPlayerSlowSpeedTime + lfTimeStep;
            mfPlayerSlowSpeedTime = (lfNew - KF_PLAYER_SLOW_SPEED_TIME_CAP >= 0.0f)
                                        ? KF_PLAYER_SLOW_SPEED_TIME_CAP
                                        : lfNew;
        }

        if (lpPlayerCar->IsCrashing())              // lbz 0x1542
            mfPlayerTimeSinceCrash = 0.0f;
        else
            mfPlayerTimeSinceCrash = mfPlayerTimeSinceCrash + lfTimeStep;

        if (lpPlayerCar->mbIsDrivenByPlayer)        // lbz 0x154A
            mfPlayerTimeSinceAIDriven = mfPlayerTimeSinceAIDriven + lfTimeStep;
        else
            mfPlayerTimeSinceAIDriven = 0.0f;
    }

    // ================================================================================
    // IsPlayerProtected @0x827660C8
    //
    // "Leave the player alone for now." Two arms, both keyed on the player's
    // mbIsDrivenByPlayer and on THIS car's mfRaceTimer (car+0x14FC):
    //   * early-out FALSE when the player is a ROAD-RAGE-style route finder (meRouteFindingStyle
    //     == 2), this car is still in the first 5 s of its race, and the player car is NOT
    //     AI-driven -- i.e. a human player at the start of a road rage IS fair game;
    //   * otherwise protected while the player car is not AI-driven, or this car is still in its
    //     first 5 s, or the player crashed less than 8 s ago, or has been AI-driven for under 3 s.
    // ================================================================================
    bool AIDriver::IsPlayerProtected(AICar* lpPlayerCar)
    {
        if (lpPlayerCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(2)   // lwz 0x14C0
            && mpCarHost->mfRaceTimer < KF_PRE_AGGRESSION_DELAY                          // flt_820C488C == 5.0
            && !lpPlayerCar->mbIsDrivenByPlayer)                                         // lbz 0x154A
        {
            return false;
        }

        if (!lpPlayerCar->mbIsDrivenByPlayer)                       return true;
        if (mpCarHost->mfRaceTimer < KF_PRE_AGGRESSION_DELAY)       return true;
        if (mfPlayerTimeSinceCrash < KF_BE_KIND_AFTER_CRASH_DELAY)  return true;   // flt_820C41E0 == 8.0
        if (mfPlayerTimeSinceAIDriven < KF_BE_KIND_AFTER_AI_DRIVEN_DELAY) return true;  // flt_820C4154 == 3.0
        return false;
    }

    // ================================================================================
    // ChooseAggressiveSteeringFan @0x82766150
    //
    // The bias-mode ladder for an AGGRESSIVE route-finding style (ROAD_RAGE / MARKED_MAN). Every
    // "% 2" / "% 3" below is the console's own opponent-index stagger, which keeps the pack from
    // all attacking on the same frame (the asm spells them as the mulhw/subfe reciprocal idioms).
    // ================================================================================
    s32 AIDriver::ChooseAggressiveSteeringFan(AICar* lpPlayerCar)
    {
        CGS_ASSERT(lpPlayerCar != 0, "lpPlayerCar");                     // :493

        // The player is crashing and this car drew an odd opponent index -> pile in (bias 2).
        if (lpPlayerCar->IsCrashing() && (mpCarHost->miOpponentIndex % 2) != 0)
        {
            return 2;
        }

        if (IsPlayerProtected(lpPlayerCar))
        {
            if (mpCarHost->mfBuzzDistanceToPlayer < KF_AGGRESSIVE_FAN_BUZZ_DISTANCE)  // flt_820C4150 == 10
                return 0;
            if (mpCarHost->meRelativeLocation == E_RELATIVE_BEHIND_APPROACHING)       // lwz 0x14D4 == 0
                return 0;
        }

        // 0x82766200: behind-approaching (0) or in-front-approaching (2) -> consider a spurt.
        const s32 liRelative = static_cast<s32>(mpCarHost->meRelativeLocation);
        if (liRelative == 0 || liRelative == 2)
        {
            bool lbSpurt = false;
            if (mfPlayerSlowSpeedTime >= KF_AGGRESSIVE_FAN_SLOW_PLAYER_TIME)          // flt_820C4250 == 6.0
            {
                const f32 lfMySpeed = mpCarHost->GetSpeed();
                if (lfMySpeed > lpPlayerCar->GetSpeed() + KF_AGGRESSIVE_FAN_SPEED_ADVANTAGE)  // flt_8300D934 == 10 mph
                    lbSpurt = true;
            }
            if (!lbSpurt
                && mpCarHost->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(6)
                && lpPlayerCar->GetSpeed() < KF_AGGRESSIVE_FAN_MARKED_MAN_PLAYER_SPEED)       // flt_8300D774 == 90 mph
            {
                lbSpurt = true;
            }
            if (lbSpurt)
            {
                if (mpCarHost->mfBuzzDistanceToPlayer < KF_AGGRESSIVE_FAN_BUZZ_DISTANCE)
                    return 7;
                return 2;
            }
        }

        // 0x8276629C: only the in-front-separating case (3) gets the distance-staggered fans.
        const s32 liRelative2 = static_cast<s32>(mpCarHost->meRelativeLocation);
        if (liRelative2 == 2 || liRelative2 != 3)
        {
            return 0;
        }

        const f32 lfDistance = mpCarHost->mfBuzzDistanceToPlayer;
        if (!(lfDistance > KF_AGGRESSIVE_FAN_MIN_DISTANCE))                          // flt_820C488C == 5
            return 0;
        if (mpCarHost->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(6))
            return 0;
        if (lfDistance > KF_AGGRESSIVE_FAN_MAX_DISTANCE)                             // flt_820C3FAC == 100
            return 0;

        const s32 liOpponent = static_cast<s32>(mpCarHost->miOpponentIndex);
        if (lfDistance > KF_AGGRESSIVE_FAN_MID_DISTANCE)                             // flt_820C4238 == 15
            return ((liOpponent % 3) == 0) ? 0 : 5;                                  // andi. r3, r11, 5
        return ((liOpponent % 2) == 0) ? 0 : 6;                                      // rlwinm r3, r11, 0,29,30
    }

    // ================================================================================
    // UpdateStuck @0x82766440
    //
    // Accumulate mfStuckTime while the car is crawling; once it has been stuck for 2 s (and it is
    // not already doing a slow turn, and its behaviour is not 0) push the car into behaviour 6
    // (SLOW TURN). The accumulate arm is skipped only for a DEFAULT-style (style 0) car that is
    // already in behaviour 6.
    //
    // THIS IS THE ONLY WAY INTO SLOW_TURN FOR A CAR WITH NO ROUTE. The other producer,
    // Determine180Turn @0x8277C758, opens with ComputeRouteDirection @0x82766500 and returns
    // early when the route has no nodes (asm 0x8277C788), so with `route 0` it can never fire.
    // Run6 (scratch/aiwave/run6/BrnGame.log): rival slot 1 entered FIGHTING at t~4.6 s at
    // 2.3 m/s, never passed 4.12 m/s (KF_MAX_SPEED_FOR_BEING_STUCK is 4.4704 m/s == 10 mph), so
    // mfStuckTime ran uninterrupted and this arm fired 2 s later, exactly as the console would.
    // With a real desired speed the car clears 10 mph inside the first second and mfStuckTime is
    // reset to 0 every frame, so the console never reaches this arm off a standing start.
    // ================================================================================
    void AIDriver::UpdateStuck(f32 lfTimeStep)
    {
        AICar* lpCar = mpCarHost;
        const f32 lfSpeed = lpCar->GetSpeed();
        const s32 liBehaviour = static_cast<s32>(lpCar->meBehaviour);     // lwz 0x14B4

        if (liBehaviour != 6 && liBehaviour != 0 && mfStuckTime >= KF_AI_TIME_TO_START_TURNING)  // 2.0
        {
            lpCar->mfBehaviourTimer    = 0.0f;                             // stfs 0x14E0
            lpCar->mePreviousBehaviour = static_cast<EAIBehaviour>(liBehaviour);  // stw 0x14B8
            lpCar->meBehaviour         = static_cast<EAIBehaviour>(6);      // stw 0x14B4, 6
            WitnessBehaviourTransition(this, lpCar, liBehaviour, 6,
                                       "UpdateStuck: mfStuckTime >= KF_AI_TIME_TO_START_TURNING (2s under 10mph)");   // [FLAG PC witness]
        }

        if (lpCar->GetRouteFindingStyle() != static_cast<ERouteFindingStyle>(0) || liBehaviour != 6)
        {
            if (lfSpeed >= KF_MAX_SPEED_FOR_BEING_STUCK)                   // flt_8300D818 == 10 mph
                mfStuckTime = 0.0f;                                        // 0x1D04
            else
                mfStuckTime = mfStuckTime + lfTimeStep;
        }
    }

    // ================================================================================
    // IsStuck @0x82766670
    //
    // Has this car been crawling long enough to be reported stuck? Never in the first 10 s of a
    // race for a non-default route finder, never while inactive, and the threshold is 0.75 s for
    // the (AI-shadowed) PLAYER car and 5 s for everyone else.
    // ================================================================================
    bool AIDriver::IsStuck()
    {
        CGS_ASSERT(mpCarHost != 0, "mpCar != NULL");                       // :2519

        AICar* lpCar = mpCarHost;
        f32 lfThreshold = KF_AI_TIME_FOR_BEING_STUCK;                      // 5.0

        if (lpCar->GetRouteFindingStyle() != static_cast<ERouteFindingStyle>(0)
            && lpCar->mfRaceTimer < KF_RACE_STUCK_FREE_TIME)               // flt_820C4150 == 10.0
        {
            return false;
        }

        if (lpCar->IsPlayerCar() && !lpCar->mbIsDrivenByPlayer)
            lfThreshold = KF_PLAYER_TIME_FOR_BEING_STUCK;                  // flt_820C41FC == 0.75

        if (!mbIsActive)                                                   // lbz 0x1D69
            return false;

        return mfStuckTime > lfThreshold;
    }

    // ================================================================================
    // CheckForBoosting @0x827705E0
    //
    // Three arms:
    //   * driving a FORCED speed  -> boost when already above 150 mph, or when the forced speed
    //                                is more than 5 mph above the current speed;
    //   * below the desired speed and above 40 mph -> boost when the desired speed is above
    //                                130 mph, or the gap to it is more than 50 mph;
    //   * otherwise -> boost only while the aggression machine is SPURT_FORWARD (state 12).
    // ================================================================================
    bool AIDriver::CheckForBoosting()
    {
        const f32 lfSpeed = mpCarHost->GetSpeed();

        if (mbUseForcedSpeed)                                              // lbz 0x1D66
        {
            return lfSpeed > KF_FORCED_BOOST_SPEED                          // flt_8300D7A4 == 150 mph
                || mfForcedSpeed > (KF_BOOST_TO_CLOSE_SPEED_GAP + lfSpeed); // flt_8300DB20 == 5 mph
        }

        const f32 lfDesired = mfDesiredSpeed;                              // 0x1D10
        if (lfSpeed < lfDesired && lfSpeed > KF_NO_BOOST_SPEED)            // flt_8300D81C == 40 mph
        {
            return lfDesired > KF_ALWAYS_BOOST_SPEED                        // flt_8300D750 == 130 mph
                || lfSpeed < (mfDesiredSpeed - KF_BOOST_UP_TO_DESIRED_SPEED_OFFSET);  // flt_8300DB24 == 50 mph
        }

        // lwz 0x1C00(this) == mAggression.meAggressionState
        return GetAggression()->GetAggressionState() == E_AI_AGGRESSION_STATE_SPURT_FORWARD;
    }

    // ================================================================================
    // CheckForSpeedMatch @0x82793020
    //
    // When the car has a speed-selection method AND the aggression machine is speed-matching
    // (mAggression.meSpeedMatchType != Disabled, driver+0x1C58), take the matched speed but never
    // let it fall below this car's own "decent" speed, publish it as mfForcedSpeed and -- only
    // while the desired speed is below the car's top speed -- also drive the desired speed with
    // it. mbUseForcedSpeed records the answer.
    // ================================================================================
    bool AIDriver::CheckForSpeedMatch(f32 lfTimeStep)
    {
        u8 lbUseForced = 0;

        if (mpCarHost->meSpeedSelectionMethod != 0                       // lwz 0x14BC(car)
            && GetAggression()->IsSpeedMatchingType())                       // lwz 0x1C58 != 0
        {
            // X360 @0x82793058: f1 = mfDesiredSpeed, f2 = dt (the FIRST float slot carries the
            // target speed, the second the time step -- read off the register set-up, not the
            // DWARF parameter names).
            const f32 lfMatched = GetAggression()->CalcSpeedMatchSpeed(mfDesiredSpeed, lfTimeStep);
            const f32 lfDecent  = mpCarHost->GetDecentSpeed();
            const bool lbBelowTop = (mfDesiredSpeed < mfTopSpeed);       // 0x1D10 < 0x1D14

            // fsel f0, (decent - matched), decent, matched == max(decent, matched)
            const f32 lfSpeed = ((lfDecent - lfMatched) >= 0.0f) ? lfDecent : lfMatched;
            mfForcedSpeed = lfSpeed;                                     // 0x1D18
            if (lbBelowTop)
                mfDesiredSpeed = lfSpeed;                                // 0x1D10
            lbUseForced = 1;
        }

        mbUseForcedSpeed = lbUseForced;                                  // stb 0x1D66
        return lbUseForced != 0;
    }

    // ================================================================================
    // CalculateDesiredSpeed @0x827934C0 .. 0x82793560  (NO IDA export -- image bytes)
    //
    // The speed the controls chase this frame. Start at the car's own desired speed
    // (AICar::mfSpeedOutOfRange, the CalcDesiredSpeed result), cap it by the three limiters in
    // turn (hard shoulder, proximity, cornering) and finally scale by how straight the chosen
    // steering-fan ray is:
    //   mfTopSpeed      = mpCar->mfSpeedOutOfRange
    //   f31 = HardShoulderSpeed(mfTopSpeed) ; f30 = ProximitySpeed(mfTopSpeed)
    //   f1  = CorneringTopSpeed(mfTopSpeed)
    //   mfDesiredSpeed  = min(f1, f31)                      (fsel @0x82793514)
    //   mfDesiredSpeed  = min(mfDesiredSpeed, f30)          (fsel @0x82793520)
    //   mfDesiredSpeed *= mSteeringFan.GetSpeedRatio() * 0.75 + 0.25
    // Neither parameter is read by the console body (the caller sets them up because the DWARF
    // signature is CalculateDesiredSpeed(Vector3, bool)).
    // ================================================================================
    void AIDriver::CalculateDesiredSpeed(Vector3 lPlayerCarPosition, bool lbDoInRangeCatchup)
    {
        (void)lPlayerCarPosition;
        (void)lbDoInRangeCatchup;

        mfTopSpeed = mpCarHost->GetDesiredSpeed();            // lfs 0x14E8(car) -> stfs 0x1D14

        const f32 lfHardShoulder = HardShoulderSpeed(mfTopSpeed);
        const f32 lfProximity    = ProximitySpeed(mfTopSpeed);
        const f32 lfCornering    = CorneringTopSpeed(mfTopSpeed);

        f32 lfDesired = ((lfCornering - lfHardShoulder) >= 0.0f) ? lfHardShoulder : lfCornering;
        mfDesiredSpeed = lfDesired;                           // stfs 0x1D10 (the console stores twice)

        lfDesired = ((lfDesired - lfProximity) >= 0.0f) ? lfProximity : lfDesired;
        mfDesiredSpeed = lfDesired;

#if BRN_AI_STEERINGFAN_TARGET_PRESENT
        const f32 lfRatio = mSteeringFan.GetSpeedRatio();     // @0x82779B90 (r3 = this + 0x710)
#else
        // [FLAG PC bring-up] SteeringFan::GetSpeedRatio @0x82779B90 IS bodied, and aiwave R6 also
        // bodied AccumulateWeightings / UpdateWeightings -- but 6 of the 13 contributors that fill
        // mfCumulativeWeighting[17] are still parked, which is why
        // BRN_AI_STEERINGFAN_TARGET_PRESENT (BrnRacingLineGenerator.h) is still 0 and every slot is
        // the Prepare-time 0.0f. GetBestIndex @0x82768D48 is "first wins on
        // ties", which then returns ray 0 -- the HARD-LEFT edge of the fan -- and GetSpeedRatio's
        // own maths turns that into v = (0*0.0625 - 0.5)*2 = -1, |v^3| = 1 >= 0.5 knee, ratio = 0.
        // The console never evaluates an all-zero fan while driving, so that 0 is an artifact of
        // the park, not console behaviour -- and it is expensive: CalculateDesiredSpeed's tail
        // scales the desired speed by (ratio * 0.75 + 0.25), so a parked fan permanently caps every
        // AI car at a QUARTER of its speed. Measured in run6 (scratch/aiwave/run6/BrnGame.log):
        // rival slot 1 top speed ~20.04 m/s -> desired 5.009642 m/s, which is under UpdateStuck's
        // KF_MAX_SPEED_FOR_BEING_STUCK (4.4704 m/s == 10 mph) once the accelerator ramp tapers, so
        // the car banked 2 s of mfStuckTime and UpdateStuck pushed it into SLOW_TURN.
        // FALLBACK: ray 8, the fan's CENTRE (straight ahead) -- v = 0, |v^3| = 0 < knee, ratio 1.0,
        // i.e. the value GetSpeedRatio returns for an unobstructed straight-ahead choice.
        // DELETE-WHEN BrnAISteeringFan.cpp's weighting half lands and the macro flips.
        const f32 lfRatio = 1.0f;
#endif
        mfDesiredSpeed = (lfRatio * KF_SPEED_RATIO_SCALE + KF_SPEED_RATIO_BASE) * lfDesired;
    }

    // ================================================================================
    // HardShoulderSpeed @0x827930B8
    //
    // Slow to 75 % of the input speed as the car drifts onto the hard shoulder: t = clamp(
    // (roadPercentage - 0.85) * 10.000004, 0, 1); return lerp(inputSpeed, inputSpeed * 0.75, t).
    // ================================================================================
    f32 AIDriver::HardShoulderSpeed(f32 lfInputSpeed)
    {
        // fmsubs f12, f31, 0.75, f31 -- the lerp's (target - source) term, computed before the call.
        const f32 lfDelta = lfInputSpeed * KF_AI_HARD_SHOULDER_PROPORTION - lfInputSpeed;

#if BRN_AI_RACINGLINE_STACK_PRESENT
        const f32 lfRoadPercentage =
            mRacingLineGenerator.GetRoadPositionAsPercentage(&GetRacingLine(), mpCarHost);
#else
        // [FLAG PC bring-up] RacingLineGenerator::GetRoadPositionAsPercentage @0x8278... has no
        // body in this tree. 0.0 == "dead centre of the road", which makes the ramp 0 and this
        // limiter a no-op -- the console's own behaviour for a car on the racing line.
        // DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
        const f32 lfRoadPercentage = 0.0f;
#endif

        const f32 lfRamp = SaturateU((lfRoadPercentage - KF_HARD_SHOULDER_START)   // flt_820C7EC8
                                     * KF_HARD_SHOULDER_RAMP);                     // flt_820C822C
        return lfRamp * lfDelta + lfInputSpeed;                                    // fmadds
    }

    // ================================================================================
    // DoDrivingBehaviour @0x82799660 .. 0x827998BC  (NO IDA export -- image bytes)
    //
    // The drift state machine that runs under behaviours 1-4 and 10. Always steers first, then
    // dispatches on meDriftState (0x1CEC):
    //   0 NORMAL_DRIVING: speed-match + drive at the desired speed; if the car IS drifting go to
    //                     DRIFTING; if a drift is wanted go to START_DRIFT; then Determine180Turn.
    //   1 START_DRIFT   : same speed work; car drifting -> DRIFTING; drift no longer wanted ->
    //                     NORMAL_DRIVING; else StartDrift().
    //   2 DRIFTING      : latch mbWantToEnterDrift, drive-at-speed-in-drift, drop to
    //                     NORMAL_DRIVING if the car stopped drifting, and go to EXIT_DRIFTING
    //                     once the drift steering angle falls under 4 degrees.
    //   3 EXIT_DRIFTING : clear every control + drift flag, raise mbWantToExitDrift, and return
    //                     to NORMAL_DRIVING once the car has stopped drifting.
    //   default         : assert "AI Driver in bad Drift state" (:1817).
    // ================================================================================
    void AIDriver::DoDrivingBehaviour(f32 lfTimeStep)
    {
        CalculateSteeringAngle(lfTimeStep);                       // 0x82799678

        switch (GetDriftState())                                  // lwz 0x1CEC
        {
            case E_DRIFT_STATE_NORMAL_DRIVING:                    // 0x827996B0
                CheckForSpeedMatch(lfTimeStep);
                AttemptToDriveAtDesiredSpeed(lfTimeStep);
                if (mpCarHost->mbIsDrifting)                      // lbz 0x1544
                    SetDriftState(E_DRIFT_STATE_DRIFTING);
                if (EstimateNeedForDrifting())
                    SetDriftState(E_DRIFT_STATE_START_DRIFT);
                Determine180Turn();
                break;

            case E_DRIFT_STATE_START_DRIFT:                       // 0x82799710
                CheckForSpeedMatch(lfTimeStep);
                AttemptToDriveAtDesiredSpeed(lfTimeStep);
                if (mpCarHost->mbIsDrifting)
                {
                    SetDriftState(E_DRIFT_STATE_DRIFTING);
                    break;
                }
                if (!EstimateNeedForDrifting())
                {
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);
                    break;
                }
                StartDrift();
                break;

            case E_DRIFT_STATE_DRIFTING:                          // 0x82799784
            {
                mbWantToEnterDrift = 1;                           // stb 1, 0x1D65
                mbWantToExitDrift  = 0;                           // stb 0, 0x1D64
                AttemptToDriveAtDesiredSpeedInDrift();
                if (!mpCarHost->mbIsDrifting)
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);

                const f32 lfDriftAngle =
                    DetermineDriftSteeringAngle(E_DRIFT_DIRECTION_SELECTION_CAR_MOVING);   // r4 = 0
                if (std::fabs(lfDriftAngle) < KF_MAX_DRIFT_UNNECESSARY_ANGLE)              // flt_8300D7AC
                    SetDriftState(E_DRIFT_STATE_EXIT_DRIFTING);
                break;
            }

            case E_DRIFT_STATE_EXIT_DRIFTING:                     // 0x827997F4
                mfBrake            = 0.0f;   // 0x1D2C
                mbUseForcedSpeed   = 0;      // 0x1D66
                mfAccelerator      = 0.0f;   // 0x1D28
                mbBoosting         = 0;      // 0x1D6B
                mfHandBrake        = 0.0f;   // 0x1D30
                mbDriftingRequired = 0;      // 0x1D67
                mfAngleForDrift    = 0.0f;   // 0x1D4C
                mbWantToEnterDrift = 0;      // 0x1D65
                mbWantToExitDrift  = 1;      // 0x1D64
                if (!mpCarHost->mbIsDrifting)
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);
                break;

            default:
                CGS_ASSERT(false, "AI Driver in bad Drift state");   // :1817
                break;
        }
    }

    // ================================================================================
    // EstimateNeedForDrifting @0x82793438
    //
    // Below 30 mph never drift. Otherwise measure the drift steering angle from the car's MOVING
    // direction, cache it in mfAngleForDrift, and want a drift once |angle| exceeds 0.9 rad.
    // ================================================================================
    bool AIDriver::EstimateNeedForDrifting()
    {
        if (mpCarHost->GetSpeed() < K_MIN_DRIFT_SPEED)              // flt_8300DBE4 == 30 mph
            return false;

        const f32 lfAngle = DetermineDriftSteeringAngle(E_DRIFT_DIRECTION_SELECTION_CAR_MOVING);
        mfAngleForDrift = lfAngle;                                  // stfs 0x1D4C
        return std::fabs(lfAngle) > KF_DRIFT_TRIGGER_ANGLE;         // 0.89999998
    }

    // ================================================================================
    // DetermineDriftSteeringAngle @0x827931D0
    //
    // Signed planar angle from the car's chosen direction (velocity for CAR_MOVING, facing for
    // CAR_FACING) to the racing line's final drift direction. Returns 0 when the racing line is
    // not initialised or the drift direction cannot be found -- on the console fp1 is simply left
    // untouched on those two paths, which the two consumers (EstimateNeedForDrifting and
    // DoDrivingBehaviour case 2) both compare with fabs against a small threshold; 0.0 is the
    // value that makes both read "no drift needed".
    // [FLAG PC bring-up] the console leaves fp1 UNSET on the two early-outs; 0.0 here is the host
    // stand-in. DELETE-WHEN the racing-line stack lands and the early-outs stop being taken.
    // ================================================================================
    f32 AIDriver::DetermineDriftSteeringAngle(EDriftDirectionSelection leSelection)
    {
        if (!GetRacingLine().mbIsInitialised)                       // lbz 0x1AF0
            return 0.0f;

        Vector2 lFrom;
        if (leSelection == E_DRIFT_DIRECTION_SELECTION_CAR_FACING)
            lFrom = To2DU(mpCarHost->GetDirection());               // @0x8276B488
        else
            lFrom = To2DU(mpCarHost->GetVelocity());                // @0x8276B570
        lFrom = Normalize2DU(lFrom);

        // The out slot the X360 hands FindFinalDriftDirection is this+7280 == mSteeringTargetVector.
        if (!FindFinalDriftDirection(mSteeringTargetVector))
            return 0.0f;

        return FindSignedAngleBetween2DVectors(lFrom, mSteeringTargetVector);
    }

    // ================================================================================
    // FindFinalDriftDirection @0x82793138
    //
    // Ask the racing-line generator for a point (speed * 1.1) metres ahead, floored at 10 m,
    // measured from m2DCarPos (this+7376).
    // ================================================================================
    bool AIDriver::FindFinalDriftDirection(Vector2& lrOutDirection)
    {
        f32 lfLookAhead = mpCarHost->GetSpeed() * KF_DRIFT_LOOK_AHEAD_SPEED_SCALE;   // * 1.1
        if (lfLookAhead < KF_DRIFT_LOOK_AHEAD_MIN)                                   // flt_820C4150 == 10
            lfLookAhead = KF_DRIFT_LOOK_AHEAD_MIN;

#if BRN_AI_RACINGLINE_STACK_PRESENT
        // DWARF/X360 argument order (CachePointAhead @0x82791424..44 is the attesting call site):
        // (lpRacingLine r4, lfDistanceAhead f1, lFrom2D v1, lrOutPosition r6, lrOutDirection r7).
        Vector2 lPosition;
        return mRacingLineGenerator.GetPointFarAhead(&GetRacingLine(), lfLookAhead, m2DCarPos,
                                                     lPosition, lrOutDirection);
#else
        // [FLAG PC bring-up] RacingLineGenerator::GetPointFarAhead has no body in this tree; the
        // console's own "no point found" answer is false, and every caller has a real fallback.
        // DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
        (void)lfLookAhead;
        (void)lrOutDirection;
        return false;
#endif
    }

    // ================================================================================
    // FindPositionInFuture @0x82792F80
    //
    // Where the car will be lfTime seconds from now, clamped into [lfMinDistance, lfMaxDistance]:
    //   d = clamp(speed * lfTime, lfMinDistance, lfMaxDistance)   (the two fsel pairs)
    // then hand that distance and m2DCarPos (this+7376) to the racing-line generator. Returns
    // false outright when the racing line is not initialised (lbz 0x1AF0).
    // ================================================================================
    bool AIDriver::FindPositionInFuture(Vector2& lrOutPosition, Vector2& lrOutDirection,
                                        f32 lfTime, f32 lfMaxDistance, f32 lfMinDistance)
    {
        if (!GetRacingLine().mbIsInitialised)                       // lbz 0x1AF0 (this+6896)
            return false;

        const f32 lfSpeed = mpCarHost->GetSpeed();
        f32 lfDistance = lfSpeed * lfTime;
        // fsel f0, (min - dist), min, dist  == max(dist, min)
        if ((lfMinDistance - lfDistance) >= 0.0f) lfDistance = lfMinDistance;
        // fsel f1, (max - dist), dist, max  == min(dist, max)
        if ((lfMaxDistance - lfDistance) < 0.0f)  lfDistance = lfMaxDistance;

#if BRN_AI_RACINGLINE_STACK_PRESENT
        return mRacingLineGenerator.GetPointFarAhead(&GetRacingLine(), lfDistance, m2DCarPos,
                                                     lrOutPosition, lrOutDirection);
#else
        // [FLAG PC bring-up] see FindFinalDriftDirection. The one caller
        // (UpdateBrakingAnticipationData) carries the console's own "no line" fallback.
        (void)lfDistance;
        (void)lrOutPosition;
        (void)lrOutDirection;
        return false;
#endif
    }

    // ================================================================================
    // StartDrift @0x8277C870
    //
    // Enter a drift: full brake, latch the enter flag, steer hard toward the side the cached
    // drift angle points at (fsel on mfAngleForDrift -> +-1), release the hand brake, then full
    // throttle. NOTE the store order -- the accelerator is written AFTER UpdateSteeringAngle.
    // ================================================================================
    void AIDriver::StartDrift()
    {
        mfBrake            = 1.0f;   // stfs flt_82001C98, 0x1D2C
        mbWantToEnterDrift = 1;      // stb  1, 0x1D65
        mbWantToExitDrift  = 0;      // stb  0, 0x1D64

        // fsel f1, mfAngleForDrift, 1.0, -1.0
        const f32 lfSteer = (mfAngleForDrift >= 0.0f) ? 1.0f : -1.0f;

        mfHandBrake = 0.0f;          // stfs flt_82001CC0, 0x1D30
        UpdateSteeringAngle(lfSteer);
        mfAccelerator = 1.0f;        // stfs flt_82001C98, 0x1D28
    }

    // ================================================================================
    // AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0
    //
    // While drifting the throttle is binary: no forced speed, no boost, and full throttle while
    // the fan's speed ratio says the line ahead is straight enough (>= 0.75), else full brake.
    // ================================================================================
    void AIDriver::AttemptToDriveAtDesiredSpeedInDrift()
    {
        mbUseForcedSpeed = 0;        // stb 0, 0x1D66
        mbBoosting       = 0;        // stb 0, 0x1D6B

        if (mSteeringFan.GetSpeedRatio() >= KF_DRIFT_SPEED_RATIO_THRESHOLD)   // flt_820C41FC == 0.75
        {
            mfBrake       = 0.0f;    // 0x1D2C
            mfAccelerator = 1.0f;    // 0x1D28
        }
        else
        {
            mfBrake       = 1.0f;
            mfAccelerator = 0.0f;
        }
    }

    // ================================================================================
    // Determine180Turn @0x8277C758
    //
    // When the route doubles back on the car (dot(routeDirection, carFacing) < -0.5) push it into
    // a QUICK TURN (behaviour 5, above 30 mph, with the steering lock GetQuickTurnSteering picks)
    // or a SLOW TURN (behaviour 6, below it). The behaviour is written directly, NOT through
    // AICar::SetBehaviour (the console stores mePreviousBehaviour / meBehaviour / the timer here).
    // ================================================================================
    void AIDriver::Determine180Turn()
    {
        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
            return;

        const Vector2 lFacing = To2DU(mpCarHost->GetDirection());       // GetDirection + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lFacing);

        // vcmpgtfp v13(-KF_TRIGGER_TURN), v0(dot) -> -K > dot
        if (!(-KF_TRIGGER_TURN > lfDot))                                // flt_8300D74C == cos(60 deg)
            return;

        AICar* lpCar = mpCarHost;
        s32 liNewBehaviour;
        if (lpCar->GetSpeed() >= KF_QUICKTURN_SLOWNESS_DROPIN)          // flt_8300D960 == 30 mph
        {
            mQuickTurnSteeringLock = GetQuickTurnSteering(lRouteDirection);   // stfs 0x1CF8
            liNewBehaviour = 5;
        }
        else
        {
            liNewBehaviour = 6;
        }

        const s32 liOldBehaviour = static_cast<s32>(lpCar->meBehaviour);
        lpCar->mePreviousBehaviour = lpCar->meBehaviour;                // stw 0x14B8
        lpCar->meBehaviour         = static_cast<EAIBehaviour>(liNewBehaviour);   // stw 0x14B4
        lpCar->mfBehaviourTimer    = 0.0f;                              // stfs 0x14E0
        WitnessBehaviourTransition(this, lpCar, liOldBehaviour, liNewBehaviour,
                                   (liNewBehaviour == 5)
                                       ? "Determine180Turn: route doubles back, speed >= 30mph"
                                       : "Determine180Turn: route doubles back, speed < 30mph");   // [FLAG PC witness]
    }

    // ================================================================================
    // GetQuickTurnSteering @0x8277C600
    //
    // Which way to spin the wheel for a 180. Returns +1 or -1 (fsel on flt_82001C98 / flt_820037C8).
    // With the centre line under the car known, the sign is the 2D cross of the car's normalised
    // facing with (mRacingLine.mCentreHere - carPosition2D); otherwise it is the sign of the
    // signed angle from that facing to lVectorToTarget.
    // ================================================================================
    f32 AIDriver::GetQuickTurnSteering(Vector2 lVectorToTarget)
    {
        const Vector2 lFacing = Normalize2DU(To2DU(mpCarHost->GetDirection()));

        f32 lfSelector;
        if (GetRacingLine().mbCentreLineHereKnown)                      // lbz 0x1AF1 (this+6897)
        {
            const Vector2 lCarPos2D = To2DU(mpCarHost->GetPosition());
            // lvx128 v12, r31, 6912 == this+0x1B00 == mRacingLine.mCentreHere (RacingLine +0xBE0)
            const Vector2 lCentreHere = GetRacingLine().mCentreHere;
            const f32 lfDeltaX = lCentreHere.x - lCarPos2D.x;
            const f32 lfDeltaY = lCentreHere.y - lCarPos2D.y;
            // vmulfp/vsubfp: facing.x * delta.y - facing.y * delta.x  (the 2D cross)
            lfSelector = lFacing.x * lfDeltaY - lFacing.y * lfDeltaX;
        }
        else
        {
            lfSelector = FindSignedAngleBetween2DVectors(lFacing, lVectorToTarget);
        }

        return (lfSelector >= 0.0f) ? KF_QUICK_TURN_STEERING_LOCK : -KF_QUICK_TURN_STEERING_LOCK;
    }

    // ================================================================================
    // UpdateQuickTurn @0x8278B100
    //
    // Leave the quick turn once the car's normalised USEFUL direction points at the driving
    // target (dot > cos(30 deg)) or the car has fallen below 10 mph. NOTE the dot is taken
    // against the UNNORMALISED (target - carPosition) delta -- that is the console's own maths.
    // ================================================================================
    void AIDriver::UpdateQuickTurn()
    {
        const Vector2 lUseful = Normalize2DU(To2DU(mpCarHost->GetUsefulDirection()));
        const Vector2 lTarget = GetTargetPosition();
        const Vector2 lCarPos = To2DU(mpCarHost->GetPosition());

        Vector2 lDelta;
        lDelta.x = lTarget.x - lCarPos.x;
        lDelta.y = lTarget.y - lCarPos.y;
        lDelta.z = 0.0f; lDelta.w = 0.0f;

        const f32 lfDot = Dot2D(lUseful, lDelta);

        if (lfDot > KF_QUICKTURN_DROP_OUT                                // flt_8300DC00 == cos(30 deg)
            || mpCarHost->GetSpeed() < KF_QUICKTURN_SLOWNESS_DROPOUT)    // flt_8300D6FC == 10 mph
        {
            CGS_ASSERT(mpCarHost->mePreviousBehaviour != static_cast<EAIBehaviour>(5),
                       "Nested quick turns!\n");                          // :1151
            const s32 liOldBehaviour = static_cast<s32>(mpCarHost->meBehaviour);
            const s32 liNewBehaviour = static_cast<s32>(mpCarHost->mePreviousBehaviour);
            mpCarHost->SetBehaviour(mpCarHost->mePreviousBehaviour);      // @0x82764DE0
            WitnessBehaviourTransition(this, mpCarHost, liOldBehaviour, liNewBehaviour,
                                       "UpdateQuickTurn: aimed at target (dot > cos30) or below 10mph");   // [FLAG PC witness]
        }
    }

    // ================================================================================
    // DoSlowTurnBehaviour @0x8277C968
    //
    // Leave the slow turn once the car's facing has come back within 20 degrees of the route
    // direction: restore behaviour 3 (CRUISING), clear the stuck timer and centre the wheel.
    //
    // THE EXIT IS GATED ON THE ROUTE. asm 0x8277C97C..0x8277C988: ComputeRouteDirection first,
    // and on false the body branches to loc_8277CA7C -- the epilogue -- WITHOUT testing
    // KF_SLOW_TURN_DROP_OUT. So a car that is put into SLOW_TURN while its route has no nodes
    // stays in SLOW_TURN for ever; there is no timer, no drop-out angle and no speed test that
    // can rescue it. That is console behaviour, not a host deviation: on the console a routeless
    // stuck car is recovered from OUTSIDE the driver, by AIModule::ProcessRequestInterface
    // @0x8278A7A8's 8-slot sweep, which sees AIDriver::IsStuck() and pushes a synthetic
    // ResetOnTrackRequest, then zeroes mfStuckTime (asm 0x8278A86C..0x8278A92C).
    // ================================================================================
    void AIDriver::DoSlowTurnBehaviour()
    {
        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
            return;

        AICar* lpCar = mpCarHost;
        const Vector2 lFacing = To2DU(lpCar->GetDirection());          // GetDirection + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lFacing);

        // vcmpgtfp v0(dot), v13(KF_SLOW_TURN_DROP_OUT)
        if (!(lfDot > KF_SLOW_TURN_DROP_OUT))                          // flt_8300D710 == cos(20 deg)
            return;

        CGS_ASSERT(lpCar->mePreviousBehaviour != static_cast<EAIBehaviour>(6),
                   "Nested slow turns!\n");                            // :1866

        const s32 liOldBehaviour = static_cast<s32>(lpCar->meBehaviour);
        lpCar->mePreviousBehaviour = lpCar->meBehaviour;               // stw 0x14B8
        lpCar->mfBehaviourTimer    = 0.0f;                             // stfs 0x14E0
        lpCar->meBehaviour         = static_cast<EAIBehaviour>(3);     // stw 0x14B4, 3
        mfStuckTime                = 0.0f;                             // stfs 0x1D04
        WitnessBehaviourTransition(this, lpCar, liOldBehaviour, 3,
                                   "DoSlowTurnBehaviour: facing back within 20 deg of the route");   // [FLAG PC witness]
        UpdateSteeringAngle(0.0f);
    }

    // ================================================================================
    // DoSlowTurn @0x8277CA88
    //
    // The behaviour-6 controls: steer AWAY from the side the route is on (the sign of
    // dot(routeDirection, carRight), negated by the vxor with the sign mask), then alternate
    // between a throttle phase and a brake phase every 3 seconds of behaviour time:
    //   phase = (s32)(mpCar->mfBehaviourTimer * 0.33333334)
    //   phase even -> mfAccelerator = 1 - clamp( speed / 20 mph, 0, 1)
    //   phase odd  -> mfBrake       = 1 - clamp(-speed / 20 mph, 0, 1)   (the console negates)
    // With no usable route the wheel is simply centred.
    //
    // NOTE the no-route arm (asm 0x8277CBC4) calls UpdateSteeringAngle(0.0f) and jumps to the
    // epilogue: it never reaches either throttle phase, so mfAccelerator and mfBrake keep the
    // zeroes CalculateCarControls wrote at its head. A routeless car in SLOW_TURN therefore
    // reports gas 0 / brake 0 / steer 0 every frame and coasts to a stop -- run6's `behaviour 6
    // ... gas 0.000000 ... speed 0.076035` line, reproduced exactly.
    // ================================================================================
    void AIDriver::DoSlowTurn(f32 lfTimeStep)
    {
        (void)lfTimeStep;   // the console reads only mpCar->mfBehaviourTimer here

        AICar* lpCar = mpCarHost;

        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
        {
            UpdateSteeringAngle(0.0f);                                 // 0x8277CBC4
            return;
        }

        const Vector2 lRight = To2DU(lpCar->GetRight());               // GetRight + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lRight);
        // rw::math::fpu::SgnNonZero -> +-1, then vxor with the 0x80000000 sign mask == negate.
        const f32 lfSteer = -((lfDot >= 0.0f) ? 1.0f : -1.0f);
        UpdateSteeringAngle(lfSteer);

        const s32 liPhase = static_cast<s32>(lpCar->mfBehaviourTimer * KF_SLOW_TURN_PHASE_RATE);
        if ((liPhase % 2) == 0)
        {
            const f32 lfT = SaturateU(lpCar->GetSpeed() / KF_SLOW_TURN_SPEED);      // flt_8300D718
            mfAccelerator = 1.0f - lfT;                                             // stfs 0x1D28
        }
        else
        {
            const f32 lfT = SaturateU(-(lpCar->GetSpeed() / KF_SLOW_TURN_SPEED));   // the console's fneg
            mfBrake = 1.0f - lfT;                                                   // stfs 0x1D2C
        }
    }

    // ================================================================================
    // InitialiseRacingLine @0x82792DF0
    //
    // Bring the racing line up for the car's CURRENT route. Clears the three "line is up" flags
    // and the last-known section, then (only with a usable route) drops the section cache when
    // the route time stamp moved, zeroes the four steering/braking 2D vectors, picks the HNG
    // spread distance (25 m while FIGHTING with a valid aggression target, else 50 m), asks the
    // generator to build the line and raises mbIsRacingLineInitialised.
    // ================================================================================
    void AIDriver::InitialiseRacingLine()
    {
        AICar* lpCar = mpCarHost;

        mbIsRacingLineInitialised = 0;                        // stb 0, 0x1D68
        mbCurrentRouteComplete    = 0;                        // stb 0, 0x1D6A
        GetRacingLine().mbIsInitialised = false;              // stb 0, 0x1AF0
        GetRacingLine().miLastKnownSectionID = 0x7FFF;        // stw 0x7FFF, 0x1B20 (RacingLine +0xC00)

        const Route* lpRoute = lpCar->GetRoute();
        // v3[1282] / v3[1280]: meStatus (car+0x1408) != UNINITIALISED and miNodeCount > 0.
        if (!(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpRoute->GetNodeCount() > 0))
            return;

        if (lpCar->miRouteTimeStamp != mActiveRouteTimeStamp)  // lwz 0x1528(car) vs 0x1D5C
        {
            GetRacingLine().ClearSectionCache();               // @0x8276E090
            GetRacingLine().mbIsInitialised = false;
            mActiveRouteTimeStamp = lpCar->miRouteTimeStamp;
        }

        CGS_ASSERT(lpCar != 0, "AI car doesn't have a route");   // :905

        if (lpRoute->GetStatus() == Route::E_STATUS_UNINITIALISED)
            return;

        miCurrentRacingLineNodeIndex = 0;                      // stw 0, 0x1D60
        mfAngleToBrakingTarget       = 0.0f;                   // stfs 0x1CFC

        mTargetRacingLinePos.SetZero();                        // stvx 0x1C80
        mBrakingAnticipationPos.SetZero();                     // stvx 0x1CA0
        mBrakingRoadDir.SetZero();                             // stvx 0x1CB0
        mTargetRoadDir.SetZero();                              // stvx 0x1C90

        // this+7236 == mAggression.mbTargetPosValid (AIAggression +0x44).
        if (lpCar->meBehaviour == static_cast<EAIBehaviour>(4) && GetAggression()->IsTargetPosValid())
            GetRacingLine().mfSpreadDistance = K_ROAD_RAGE_SPREAD_HNG;   // 25.0 -> 0x1AEC
        else
            GetRacingLine().mfSpreadDistance = K_NORMAL_SPREAD_HNG;      // 50.0

#if BRN_AI_RACINGLINE_STACK_PRESENT
        // X360 @0x82792EE8..0x82792F30: r3 = this+0x1B30 (generator), r4 = this+0xF20 (line),
        // r5 = mpCar (0x1CE0), r6 = mpCar->miNextRouteNodeIndex (car+0x1524) - 2,
        // r7 = mpSectionsData (0x1CE4).
        mRacingLineGenerator.InitialiseRacingLine(&GetRacingLine(), lpCar,
                                                  lpCar->miNextRouteNodeIndex - 2,
                                                  mpSectionsDataHost);   // @0x8278FB20
#else
        // [FLAG PC bring-up] RacingLineGenerator::InitialiseRacingLine has no body in this tree,
        // so the generator cannot raise RacingLine::mbIsInitialised. The flag is raised here so
        // the driver still runs its steering/controls chain off the route (see GetTargetPosition's
        // fallback); the console's own :954 assert below then reads true either way.
        // DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
        GetRacingLine().mbIsInitialised = true;
#endif

        mSteeringTargetVector.SetZero();                       // stvx 0x1C70
        mbIsRacingLineInitialised = 1;                         // stb 1, 0x1D68

        CGS_ASSERT(GetRacingLine().mbIsInitialised, "Racing line not initialised properly");  // :954
    }

    // ================================================================================
    // GenerateRacingLine @0x8277C4E8
    //
    // Snapshot the car's position into the racing line and either hand the line the aggression
    // target (FIGHTING with a valid target -> "default to slam player", and while the machine is
    // ATTACK_SLAM copy the default perpendicular offset into the road placement) or reset the
    // offset. Either way the route is marked complete so the line is generated once per route.
    // ================================================================================
    void AIDriver::GenerateRacingLine(f32 lfTimeStep)
    {
        (void)lfTimeStep;   // the console body takes no float

        CGS_ASSERT(mpCarHost != 0, "AI driver trying to make a racing line without a car");   // :993
        CGS_ASSERT(GetRacingLine().mbIsInitialised, "Racing line not initialised properly");  // :994

        // stvx128 v0, r31, 6736 == this+0x1A50 == mRacingLine.mCarPos (RacingLine +0xB30)
        GetRacingLine().mCarPos = mpCarHost->GetPosition();

        if (mpCarHost->meBehaviour == static_cast<EAIBehaviour>(4) && GetAggression()->IsTargetPosValid())
        {
            mbCurrentRouteComplete = 1;                                  // stb 1, 0x1D6A
            const Vector3 lTargetPos = GetAggression()->GetTargetPos();  // @0x827656D0
#if BRN_AI_RACINGLINE_STACK_PRESENT
            mRacingLineGenerator.RaceLineDefaultsToSlamPlayer(&GetRacingLine(), lTargetPos);
#else
            // [FLAG PC bring-up] RacingLineGenerator::RaceLineDefaultsToSlamPlayer has no body
            // in this tree. DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
            (void)lTargetPos;
#endif
            if (GetAggression()->GetAggressionState() == E_AI_AGGRESSION_STATE_ATTACK_SLAM)
            {
                // this+6724 = this+6704 == mfRoadPlacement = mfDefaultPerpendicularOffset
                GetRacingLine().mfRoadPlacement = GetRacingLine().mfDefaultPerpendicularOffset;
            }
        }
        else
        {
            mbCurrentRouteComplete = 1;                                  // stb 1, 0x1D6A
            GetRacingLine().mfDefaultPerpendicularOffset = 0.0f;         // stfs 0x1A30
            GetRacingLine().mbDefiniteDestination        = false;        // stw  0x1A34
        }
    }

    // ================================================================================
    // UpdateBrakingAnticipationData @0x827964C0 .. 0x827965E4  (NO IDA export -- image bytes)
    //
    // Where the car will be in 1.5 s (clamped into 16..75 m along the racing line) and which way
    // the road runs there; those two feed CorneringTopSpeed. With no racing line, fall back on
    // "one car-length straight ahead" along the car's USEFUL direction. Either way the road
    // direction is 2D-normalised on the way out (the vrsqrtefp + 2 refine steps at 0x827965AC).
    // ================================================================================
    void AIDriver::UpdateBrakingAnticipationData()
    {
        if (!FindPositionInFuture(mBrakingAnticipationPos, mBrakingRoadDir,
                                  KF_BRAKING_ANTICIPATION_TIME,        // flt_82004D04 == 1.5
                                  KF_BRAKING_MAX_LOOK_AHEAD_DIST,      // flt_820C41F0 == 75
                                  KF_BRAKING_MIN_LOOK_AHEAD_DIST))     // flt_82004000 == 16
        {
            CGS_ASSERT(mpCarHost != 0, "No car available");            // :1058

            const Vector3 lUseful   = mpCarHost->GetUsefulDirection();  // @0x82770028
            const Vector3 lPosition = mpCarHost->GetPosition();         // @0x8276B1F0

            mBrakingAnticipationPos.x = lPosition.x + lUseful.x;        // stvx 0x1CA0
            mBrakingAnticipationPos.y = lPosition.y + lUseful.y;
            mBrakingAnticipationPos.z = 0.0f;
            mBrakingAnticipationPos.w = 0.0f;

            mBrakingRoadDir.x = lUseful.x;                              // stvx 0x1CB0
            mBrakingRoadDir.y = lUseful.y;
            mBrakingRoadDir.z = 0.0f;
            mBrakingRoadDir.w = 0.0f;
        }

        mBrakingRoadDir = Normalize2DU(mBrakingRoadDir);
    }

    // ================================================================================
    // CalcDistanceFromPlayer @0x8276E1C8 .. 0x8276E26C  (NO IDA export -- image bytes)
    //
    // mfDistanceToPlayer = |mpCar->GetPosition() - lPlayerCarPosition| (the vrsqrtefp + 2 refine
    // magnitude idiom, with the vsel that yields 0 for a zero-length delta).
    // ================================================================================
    void AIDriver::CalcDistanceFromPlayer(Vector3 lPlayerCarPosition)
    {
        const Vector3 lPosition = mpCarHost->GetPosition();
        mfDistanceToPlayer = rw::math::vpu::Magnitude(lPosition - lPlayerCarPosition);   // stfs 0x1D08
    }

    // ================================================================================
    // ResetAttribSysValues @0x8278B2B0
    //
    // Re-seed the per-car steering tunables from the AttribSys. Always resets the two PID
    // controllers first; then, with a car bound, reads the burnoutcarasset -> physicsvehiclehandling
    // (RefSpec @+0x158) -> physicsvehiclesteeringattribs (RefSpec @+0x18) chain and takes
    //   mfTimeToLookAheadForDrift        = steering data +0x34
    //   mfMinDistanceToLookAheadForDrift = steering data +0x30
    // With no car it uses the two rodata defaults (1.1 / 10.0).
    // ================================================================================
    void AIDriver::ResetAttribSysValues()
    {
        ResetPIDTuningState();                                    // @0x8277DA48

        if (mpCarHost != 0)
        {
            // [FLAG PC bring-up] the console walks
            //   sub_82204998(&inst, mpCar->mCarAssetAttribKey (ld 0x14D8), 0)
            //   -> Attrib::RefSpec::GetCollection(inst.data + 0x158)
            //   -> Attrib::Gen::physicsvehiclehandling(collection, 0)
            //   -> Attrib::RefSpec::GetCollection(handling.data + 0x18)
            //   -> Attrib::Gen::physicsvehiclesteeringattribs(collection, 0)
            //   -> mfTimeToLookAheadForDrift = data[+0x34]; mfMinDistanceToLookAheadForDrift = data[+0x30]
            // sub_82204998 (the burnoutcarasset instance ctor from an Attribute::Key) has no name
            // and no reconstruction in this tree, so the attrib read is PARKED and the rodata
            // defaults below are used for every car. DELETE-WHEN sub_82204998 @0x82204998 is
            // identified and the burnoutcarasset generated class lands.
            mfMinDistanceToLookAheadForDrift = KF_DEFAULT_MIN_DIST_LOOK_AHEAD_FOR_DRIFT;  // 10.0
            mfTimeToLookAheadForDrift        = KF_DEFAULT_TIME_TO_LOOK_AHEAD_FOR_DRIFT;   // 1.1
            return;
        }

        mfMinDistanceToLookAheadForDrift = KF_DEFAULT_MIN_DIST_LOOK_AHEAD_FOR_DRIFT;   // stfs 0x1D40
        mfTimeToLookAheadForDrift        = KF_DEFAULT_TIME_TO_LOOK_AHEAD_FOR_DRIFT;    // stfs 0x1D3C
    }

    // ================================================================================
    // ResetPIDTuningState @0x8277DA48
    //
    // Re-Prepare the two steering PID controllers with their fixed {P, I, D} triples:
    //   mPIDController      (this+0x1B34) = {1.5, 0.0, 0.5}
    //   mPIDControllerDrift (this+0x1B98) = {2.0, 0.0, 1.0}
    // The console ALSO constructs and immediately destructs the burnoutcarasset ->
    // physicsvehiclehandling -> physicsvehiclesteeringattribs Attrib chain when a car is bound
    // (0x8277DA68..0x8277DAC4) WITHOUT reading a single field out of it -- the whole block is the
    // Attrib layer's own class-check assert, with no effect on this object. Not reproduced.
    // ================================================================================
    void AIDriver::ResetPIDTuningState()
    {
        const f32 lafNormal[3] = { KF_PID_NORMAL_P, KF_PID_NORMAL_I, KF_PID_NORMAL_D };
        const f32 lafDrift[3]  = { KF_PID_DRIFT_P,  KF_PID_DRIFT_I,  KF_PID_DRIFT_D  };

        mPIDController.Prepare(lafNormal);         // r3 = this + 0x1B34
        mPIDControllerDrift.Prepare(lafDrift);     // r3 = this + 0x1B98
    }

    // ================================================================================
    // DoRoundRobinWork @0x82796340
    //
    // The AIModule hands each active driver a slice of the per-frame line work:
    //   E_ROUND_ROBIN_FAN (0): one full SteeringFan::UpdateWeightings pass, worth 1 unit;
    //   E_ROUND_ROBIN_HNG (1): up to FOUR RacingLineGenerator::SpreadHNGBackOneStep steps,
    //                          stopping early when the spread reports it is done; the return is
    //                          the number of steps actually taken (the loop counts BEFORE the
    //                          break, so a finished spread contributes 0).
    // ================================================================================
    s32 AIDriver::DoRoundRobinWork(ERoundRobinType leType)
    {
        s32 liWorkDone = 0;

        if (leType == E_ROUND_ROBIN_HNG)
        {
            for (s32 liStep = 0; liStep <= 3; ++liStep)
            {
#if BRN_AI_RACINGLINE_STACK_PRESENT
                if (mRacingLineGenerator.SpreadHNGBackOneStep(&GetRacingLine()))
                    break;
                ++liWorkDone;
#else
                // [FLAG PC bring-up] RacingLineGenerator::SpreadHNGBackOneStep has no body in
                // this tree; "the spread is finished" is the console's own loop-exit answer.
                // DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
                break;
#endif
            }
        }
        else if (leType == E_ROUND_ROBIN_FAN)
        {
#if BRN_AI_STEERINGFAN_TARGET_PRESENT
            // X360: UpdateWeightings(this+1808 == mSteeringFan, mpCar (0x1CE0), this+3872 ==
            // mRacingLine, this+6960 == mRacingLineGenerator, this == &mNearbyVehicles,
            // *(this+7408) == meAggressionVictim (0x1CF0)).
            mSteeringFan.UpdateWeightings(mpCarHost, &GetRacingLine(), &mRacingLineGenerator,
                                          GetNearbyVehicles(),
                                          static_cast<EGlobalRaceCarIndex>(meAggressionVictimHost));
#endif
            // [FLAG PC bring-up] SteeringFan::UpdateWeightings @0x82794600 IS bodied (aiwave R6,
            // BrnAISteeringFan_Weightings.cpp) but 6 of its 13 contributors are still parked, so
            // BRN_AI_STEERINGFAN_TARGET_PRESENT (BrnRacingLineGenerator.h) is still 0 -- read that
            // gate's banner before flipping it. The unit is counted either way so AIModule's work
            // budget and round-robin cursor advance exactly as the console's do.
            // DELETE-WHEN the gate goes to 1.
            liWorkDone = 1;
        }

        return liWorkDone;
    }

    // ================================================================================
    // GetIndexOfFurthestVehicle @0x8277D2E0
    //
    // Nominates the avoidance slot to evict when the 16-slot list is full: the console walks the
    // list from this+0x10 measuring each entry against the car's position and the candidate
    // centre it is handed, returning -1 when nothing should be evicted.
    //
    // [FLAG PC bring-up] this body is a NAMED PARK. The function has no IDA export (recovered
    // address only, from AddNearbyAIToAvoidance's `bl`), and its 60-odd instructions are almost
    // entirely VMX128 forms capstone does not decode, so the selection rule cannot be read
    // faithfully yet. Returning -1 means "keep what is already in the list", which drops the new
    // candidate -- the console's own answer whenever it decides nothing is further away. With the
    // eight-driver cap the list only fills in dense traffic, and the whole avoidance feed is
    // consumed by the (gated) SteeringFan HNG contributors, so nothing observable changes until
    // that stack lands. DELETE-WHEN the VMX128 decode of 0x8277D2E0..0x8277D3E0 is available.
    // ================================================================================
    s32 AIDriver::GetIndexOfFurthestVehicle(Vector2 lCentre)
    {
        (void)lCentre;
        return -1;
    }

    // ================================================================================
    // AddNearbyTrafficToAvoidance @0x8277D4F8
    //
    // Copy one traffic entity into the avoidance list: its velocity and centre verbatim, type
    // E_NEARBY_TRAFFIC(0), no race-car index (-1), and four HNG boundary lines around the closed
    // quad formed by bounding-box corners 2, 3, 7 and 6 (the entity reads at +80 / +96 / +160 /
    // +144 == maBBCorners[2] / [3] / [7] / [6], each flattened to 2D).
    // ================================================================================
    bool AIDriver::AddNearbyTrafficToAvoidance(const BrnTraffic::BrnTrafficIO::TrafficAIEntity* lpEntity)
    {
        CGS_ASSERT(mNearbyVehicles.GetCount() >= 0, "mNearbyVehicles.GetCount() >= 0");   // :2591
        CGS_ASSERT(mNearbyVehicles.GetCount() <= NearbyVehicles::KI_MAX_NEARBY_VEHICLES,
                   "mNearbyVehicles.GetCount() <= KI_MAX_NEARBY_TRAFFIC");                // :2592

        s32 liIndex;
        if (mNearbyVehicles.miCount < NearbyVehicles::KI_MAX_NEARBY_VEHICLES)   // lwz 0x700
        {
            liIndex = mNearbyVehicles.GetCount();
            mNearbyVehicles.Next();
        }
        else
        {
            liIndex = GetIndexOfFurthestVehicle(lpEntity->mCentre);             // v1 = entity+16
            if (liIndex == -1)
                return false;
        }

        NearbyVehicle* lpVehicle = mNearbyVehicles.GetVehiclePointer(liIndex);

        lpVehicle->mCentre   = lpEntity->mCentre;                 // stvx v0, r3, 16
        lpVehicle->mVelocity = lpEntity->mVelocity;               // stvx v0, r0, r3
        lpVehicle->meGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(-1);   // stw -1, +36
        lpVehicle->mType                = E_NEARBY_TRAFFIC;                       // stw  0, +32

        FillHNGQuad(lpVehicle,
                    To2DU(lpEntity->maBBCorners[2]),              // lvx r31, 80
                    To2DU(lpEntity->maBBCorners[3]),              // lvx r31, 96
                    To2DU(lpEntity->maBBCorners[7]),              // lvx r31, 160
                    To2DU(lpEntity->maBBCorners[6]));             // lvx r31, 144
        return true;
    }

    // ================================================================================
    // AddNearbyAIToAvoidance @0x8277D6E0
    //
    // Copy one rival (or the player) into the avoidance list. Rejected outright when the other car
    // is more than 200 m away, or is BEHIND this car (dot of the separation with this car's useful
    // direction < 0) and is not the player. The stored velocity is the other car's velocity, or
    // its useful direction when it is barely moving (|v|^2 < 1). The type is E_NEARBY_PLAYER(2)
    // for the player car and E_NEARBY_AI(1) otherwise, and the four HNG lines box the car's centre
    // by +-2 m in x and y.
    // ================================================================================
    bool AIDriver::AddNearbyAIToAvoidance(const AICar* lpCar)
    {
        const Vector3 lMyPosition    = mpCarHost->GetPosition();
        const Vector3 lOtherPosition = lpCar->GetPosition();
        const Vector3 lSeparation    = lOtherPosition - lMyPosition;

        if (rw::math::vpu::Dot(lSeparation, lSeparation) > KF_NEARBY_AI_MAX_DISTANCE_SQ)  // flt_8201C220
            return false;

        const Vector3 lUseful = mpCarHost->GetUsefulDirection();
        if (rw::math::vpu::Dot(lSeparation, lUseful) < 0.0f && !lpCar->mbIsPlayer)         // lbz 0x1549
            return false;

        CGS_ASSERT(mNearbyVehicles.GetCount() >= 0, "mNearbyVehicles.GetCount() >= 0");    // :2674
        CGS_ASSERT(mNearbyVehicles.GetCount() <= NearbyVehicles::KI_MAX_NEARBY_VEHICLES,
                   "mNearbyVehicles.GetCount() <= KI_MAX_NEARBY_TRAFFIC");                 // :2675

        const Vector2 lCentre = To2DU(lpCar->GetPosition());       // the console re-fetches it

        s32 liIndex;
        if (mNearbyVehicles.miCount < NearbyVehicles::KI_MAX_NEARBY_VEHICLES)
        {
            liIndex = mNearbyVehicles.GetCount();
            mNearbyVehicles.Next();
        }
        else
        {
            liIndex = GetIndexOfFurthestVehicle(lCentre);
            if (liIndex == -1)
                return false;
        }

        NearbyVehicle* lpVehicle = mNearbyVehicles.GetVehiclePointer(liIndex);

        const Vector3 lVelocity = lpCar->GetVelocity();
        // vcmpgtfp 1.0 > |v|^2 -> the car is barely moving, use its useful direction instead.
        const Vector3 lHeading = (1.0f > rw::math::vpu::Dot(lVelocity, lVelocity))
                                     ? lpCar->GetUsefulDirection()
                                     : lVelocity;
        lpVehicle->mVelocity = To2DU(lHeading);                    // stvx v13, r0, r31
        lpVehicle->mCentre   = lCentre;                            // stvx v127, r31, 16

        lpVehicle->mType = lpCar->mbIsPlayer ? E_NEARBY_PLAYER : E_NEARBY_AI;      // stw +32 (2 / 1)
        lpVehicle->meGlobalRaceCarIndex =
            static_cast<EGlobalRaceCarIndex>(lpCar->miRaceCarIndex);               // stw +36 (car+0x14C4)

        // The two lazily-built half-extent vectors: X == (2, 0), Y == (0, 2).
        const Vector2 lHalfX = MakeHalfExtentX();
        const Vector2 lHalfY = MakeHalfExtentY();

        Vector2 lMinusX; lMinusX.x = lCentre.x - lHalfX.x; lMinusX.y = lCentre.y - lHalfX.y;
        lMinusX.z = 0.0f; lMinusX.w = 0.0f;
        Vector2 lPlusX;  lPlusX.x  = lCentre.x + lHalfX.x; lPlusX.y  = lCentre.y + lHalfX.y;
        lPlusX.z = 0.0f; lPlusX.w = 0.0f;

        Vector2 lC0; lC0.x = lMinusX.x + lHalfY.x; lC0.y = lMinusX.y + lHalfY.y; lC0.z = 0.0f; lC0.w = 0.0f;
        Vector2 lC1; lC1.x = lPlusX.x  + lHalfY.x; lC1.y = lPlusX.y  + lHalfY.y; lC1.z = 0.0f; lC1.w = 0.0f;
        Vector2 lC2; lC2.x = lPlusX.x  - lHalfY.x; lC2.y = lPlusX.y  - lHalfY.y; lC2.z = 0.0f; lC2.w = 0.0f;
        Vector2 lC3; lC3.x = lMinusX.x - lHalfY.x; lC3.y = lMinusX.y - lHalfY.y; lC3.z = 0.0f; lC3.w = 0.0f;

        FillHNGQuad(lpVehicle, lC0, lC1, lC2, lC3);
        return true;
    }
}
