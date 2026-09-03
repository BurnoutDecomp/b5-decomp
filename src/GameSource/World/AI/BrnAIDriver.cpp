#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"     // the file-scope KF_ tunables (both partfiles)
#include "GameSource/World/AI/BrnAICar.h"                  // AICar (named members + accessors)
#include "GameSource/World/AI/BrnAIUtils.h"                // StepTo / Find{Signed,Unsigned}AngleBetween2DVectors
#include "GameSource/World/AI/BrnAIAggression.h"           // AIAggression (the embedded sub-machine)
#include "GameSource/World/AI/Route/BrnRacingLine.h"       // RacingLine (the embedded sub-object)
#include "GameSource/World/AI/Route/BrnRoute.h"            // Route / RouteNode (the car's route, AICar+0)
#include "GameSource/World/AI/PID/BrnPIDController.h"      // PIDController (the two embedded controllers)

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cmath>   // std::sqrt (de-SIMD'd 2D normalise), std::isfinite (RwMath::IsValid)

// BrnAI::AIDriver -- the STEERING + CAR-CONTROL link. This TU (BrnAIDriver.cpp + the partfile
// BrnAIDriver_Update.cpp) bodies the members that turn a navigation target (heading + desired
// speed) into the car's physics inputs (steer / throttle / brake / hand-brake + boost).
// Reconstructed store-for-store from the X360 asm; the inlined SIMD 2D-vector normalise
// (vrsqrtefp + two Newton-Raphson refine steps) and the inlined vector lerps are reversed into
// the logical scalar/Vector2 maths they implement. Every call into a collaborator is a NAMED CALL.
//
// COLLABORATOR HOMES (all committed and reached BY NAME -- this file no longer performs any
// `this + <guest offset>` reinterpret_cast):
//   * AICar (BrnAICar.h) carries every field these bodies read as a named member.
//   * The embedded sub-objects are named members of AIDriver: mSteeringFan (@0x710),
//     mPIDController / mPIDControllerDrift (@0x1B34 / @0x1B98) sit at their guest offsets;
//     the pointer-bearing RacingLine and AIAggression live in the HOST-ONLY tail and are
//     reached through GetRacingLine() / GetAggression() (see BrnAIDriver.h's HOST-WIDTH RULE).
//   * The car's Route is the AICar's own base (AICar::GetRoute()); node x/y come from
//     Route::GetNode(i)->GetX()/GetY().
//   * StepTo and Find{Signed,Unsigned}AngleBetween2DVectors are declared in BrnAIUtils.h and
//     bodied in BrnAIUtils.cpp / BrnAIUtils_Angles.cpp.
//
// TUNABLES: BrnAIDriver_Constants.h. Every KF_ there is a CONSOLE value -- the rodata ones read
// straight from the image, the .bss ones evaluated from the unity-TU static initialisers at
// 0x82C67F00..0x82C69500 (see that header's banner). No placeholder tunables remain.

namespace BrnAI
{
    // ====================================================================================
    // TU-LOCAL helpers.
    // ====================================================================================

    // RwMath::IsValid( x ) -- the X360 finiteness check (vcmpeqfp x,x == "x is not NaN"; the
    // engine treats inf as invalid too).
    static inline bool IsFinite(f32 lfValue) { return std::isfinite(lfValue); }

    // de-inlined planar (x,y) normalise -- v / |v| over the (x,y) lanes (the rsqrt + 2x
    // Newton-Raphson refine idiom the X360 emits inline).
    static Vector2 Normalize2D(Vector2 lVector)
    {
        const f32 lfLenSq = lVector.x * lVector.x + lVector.y * lVector.y;
        const f32 lfInvLen = (lfLenSq > 0.0f) ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;
        Vector2 lResult;
        lResult.x = lVector.x * lfInvLen;
        lResult.y = lVector.y * lfInvLen;
        lResult.z = 0.0f;
        lResult.w = 0.0f;
        return lResult;
    }

    // 2D-flatten a Vector3 (drop z; the steering math is purely in the ground plane). This is
    // BrnMath::Flatten, which the X360 calls out of line in several of these bodies.
    static Vector2 To2D(Vector3 lVector)
    {
        Vector2 lResult;
        lResult.x = lVector.x;
        lResult.y = lVector.y;
        lResult.z = 0.0f;
        lResult.w = 0.0f;
        return lResult;
    }

    // clamp(x, 0, 1) -- the X360's `fsel max(x,0)` then `fsel min(.,1)` pair.
    static inline f32 Saturate(f32 lfValue)
    {
        if (lfValue < 0.0f) return 0.0f;
        if (lfValue > 1.0f) return 1.0f;
        return lfValue;
    }

    // ====================================================================================
    // Construct @0x82792B70
    //
    // Zero/seed the controller block at allocation: clears the stuck/distance/desired/top-speed
    // scalars, sets the invuln + finish-line-state timers to -1.0, NULLs mpCar, clears every
    // control output + flag, zeroes SIX 2D steering vectors (mFinalFacing@0x1CC0 is NOT cleared)
    // and constructs the embedded
    // AIAggression sub-machine (the X360 also registers two PerfMon timers once; that diagnostic
    // is presentation-only and intentionally omitted from this slice).
    // ====================================================================================
    void AIDriver::Construct()
    {
        mfStuckTime           = 0.0f;       // 0x1D04
        mfDistanceToPlayer    = 0.0f;       // 0x1D08
        mfDesiredSpeed        = 0.0f;       // 0x1D10
        mfCarSpeed            = 0.0f;       // 0x1D00
        mfAngleToBrakingTarget = 0.0f;      // 0x1CFC
        mfInvulnerableTime    = -1.0f;      // 0x1D0C
        mfStartLineWheelSpinTime = -1.0f;   // 0x1D20 (stfs flt_820037C8)
        mpCarHost             = nullptr;    // 0x1CE0
        mbIsRacingLineInitialised = 0;      // 0x1D68
        mbIsActive            = 0;          // 0x1D69
        mbCurrentRouteComplete = 0;         // 0x1D6A
        mfPlayerSlowSpeedTime = 0.0f;       // 0x1D58
        mfAccelerator         = 0.0f;       // 0x1D28
        miCurrentRacingLineNodeIndex = 0;   // 0x1D60
        mfBrake               = 0.0f;       // 0x1D2C
        mbUseForcedSpeed      = 0;          // 0x1D66
        mfForcedSpeed         = 0.0f;       // 0x1D18
        mbBoosting            = 0;          // 0x1D6B
        mfHandBrake           = 0.0f;       // 0x1D30
        mbWantToExitDrift     = 0;          // 0x1D64
        mfSteeringAngle       = 0.0f;       // 0x1D24
        mbWantToEnterDrift    = 0;          // 0x1D65

        // X360 zeroes SIX 2D vectors via stvx (r10=0x1C70, r9=0x1C80, r8=0x1C90, r7=0x1CA0,
        // r6=0x1CB0, r5=0x1CD0). mFinalFacing @0x1CC0 is deliberately NOT cleared here.
        mSteeringTargetVector.SetZero();    // 0x1C70
        mTargetRacingLinePos.SetZero();     // 0x1C80
        mTargetRoadDir.SetZero();           // 0x1C90
        mBrakingAnticipationPos.SetZero();  // 0x1CA0
        mBrakingRoadDir.SetZero();          // 0x1CB0
        m2DCarPos.SetZero();                // 0x1CD0

        GetAggression()->Construct(this);   // mAggression.Construct(this) (the host-side member)
    }

    // ====================================================================================
    // SetAICar @0x827963C8
    //
    // Bind this driver to a car: assert the car is non-NULL and not already bound, store mpCar,
    // clear the car's drive-state slot, wire the car back to this driver, initialise the racing
    // line, then seed the per-car timers/flags and reset the attrib-sys values.
    // ====================================================================================
    void AIDriver::SetAICar(AICar* lpCar)
    {
        CGS_ASSERT(lpCar != nullptr, "Driver given NULL car");
        CGS_ASSERT(mpCarHost == nullptr,
                   "Shouldn't be reassigning an AI driver directly to another car.");

        mpCarHost = lpCar;
        lpCar->meCarState = E_AI_CAR_STATE_IN_RANGE;        // *(car+0x14C8) = 0
        lpCar->SetDriver(this);                             // X360 @0x82796444
        InitialiseRacingLine();

        mfStuckTime        = 0.0f;          // 0x1D04
        mbIsActive         = 1;             // 0x1D69 (driver becomes active on bind)
        mfDistanceToPlayer = 0.0f;          // 0x1D08
        mfInvulnerableTime = -1.0f;         // 0x1D0C

        // BLOCKED (out of this TU's file scope): X360 @0x82796480-0x827964B0 also seeds the
        // embedded AIAggression block here directly (mpCar, mfStateTime=-1, meAggressionState=0,
        // mpTargetCar=NULL, mpPlayerCar=NULL, mbTargetPosValid=false, meSpeedMatchType=Disabled,
        // mfRelativePositionAhead/mfRecentHitTimer/mfContinuousContactTimer/mfHangingAroundTimer=0,
        // mfNonSpeedMatchedSpeed=flt_8300DC64). Every one of those is a PRIVATE AIAggression data
        // member (BrnAIAggression.h has no public setter for this "seed for new car" bundle), so
        // this TU cannot write them without a new method declared on AIAggression -- out of scope.

        // 0x82796480..0x827964B0: the twelve aggression stores (mpCar = car, state OUT_OF_RANGE, timers) --
        // homed by lane A8 as AIAggression::Prepare(AICar*); the call was missing here (run4 AV:
        // AIAggression::Update @0x82799ABC `lwz r11,8(agg) ; lbz 0x1549(r11)` on a null mpCar).
        GetAggression()->Prepare(lpCar);

        ResetAttribSysValues();
    }

    // ====================================================================================
    // Prepare @0x82792CA8
    //
    // Per-race-entry init: store the sections-data pointer + active-car index, reset the
    // attrib-sys values, clear the per-race timers/flags/nearby-vehicle count/drift state, clear
    // the racing-line section cache, draw a random tuning value from the supplied Random (an
    // inlined LCG advance + range-lerp), seed the perpendicular-target tuning, and prepare the
    // embedded SteeringFan.
    // ====================================================================================
    void AIDriver::Prepare(AISectionsData* lpSectionsData, s32 leRelatedActiveCarIndex,
                           CgsNumeric::Random* lpRandom)
    {
        (void)lpRandom;
        mpSectionsDataHost = lpSectionsData;            // 0x1CE4
        ResetAttribSysValues();
        meRelatedActiveCarIndexHost = leRelatedActiveCarIndex; // 0x1CF4
        mfPerpendicularTarget   = 0.0f;             // 0x1D48
        mNearbyVehicles.Reset();                    // stw 0, 0x700(this)
        mfHandBrake             = 0.0f;             // 0x1D30
        mbWantToEnterDrift      = 0;                // 0x1D65
        mActiveRouteTimeStamp   = 0;                // 0x1D5C
        SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);   // stw 0, 0x1CEC(this)

        GetRacingLine().ClearSectionCache();           // mRacingLine.ClearSectionCache() (this+0xF20)

        // stb 0, 0x1AF0 / 0x1AF1 == mRacingLine.mbIsInitialised / mbCentreLineHereKnown
        // (RacingLine +0xBD0 / +0xBD1).
        GetRacingLine().mbIsInitialised       = false;
        GetRacingLine().mbCentreLineHereKnown = false;

        // X360 draws a per-car random tuning float in a [lo,hi] range (the inlined LCG advance:
        // multiplier 0x5851F42D, increment +1; then a vector lerp of two rodata bounds, minus 1.0)
        // and seeds the perpendicular-target tuning from it. The exact range bounds are
        // rodata-by-address (UNRECOVERED); the LCG advance touches the supplied Random's state.
        // Restored store-for-store would require the Random + RacingLineGenerator homes, so the
        // perpendicular-tuning seed is left at its zero-init here and FLAGGED.

        mSteeringFan.Prepare();                      // @0x82778E40

        mfBoostTimeRemaining      = 0.0f;            // 0x1D44
        mfPlayerTimeSinceCrash    = 0.0f;            // 0x1D34
        mbBoosting                = 0;               // 0x1D6B
        mfPlayerTimeSinceAIDriven = 0.0f;            // 0x1D38
        mfPlayerSlowSpeedTime     = 0.0f;            // 0x1D58
        SetAggressionVictimCar(nullptr);             // 0x1CE8 -- mpAggressionVictim = NULL
        meAggressionVictimHost        = -1;             // 0x1CF0 -- EGlobalRaceCarIndex victim = none
    }

    // ====================================================================================
    // Update @0x8279AEB0  -- the per-frame spine.
    //
    // Gate: only run when mbIsActive && a valid player car was supplied; otherwise clear the four
    // control outputs + their flags and bail. When active: refresh the player timers + choose/bias
    // the SteeringFan; (if the racing line is generating) sample the centre-line; if the car is
    // crashing, just decay the invuln timer and tick the behaviour once; else store the car's 2D
    // position + speed and -- unless showtime-frozen -- run the full pipeline (distance-from-player,
    // racing-line init, UpdateBehaviour, (re)generate the line + braking anticipation, then
    // CalculateCarControls) and assert the steering angle is finite.
    // ====================================================================================
    void AIDriver::Update(f32 lfTimeStep, bool lbLineUpdateToken, Vector3 lPlayerCarPosition,
                          AICar* lpPlayerCar, bool lbDoInRangeCatchup, CgsNumeric::Random* lpRandom)
    {
        // r5 (lbLineUpdateToken == (slot == AIModule::miLineUpdateTokenCounter)) is set up by the
        // caller but never read by the console body.
        (void)lbLineUpdateToken;

        if (mbIsActive && lpPlayerCar)
        {
            UpdatePlayerTimers(lfTimeStep, lpPlayerCar);
            SetDrivingFanBiases(lpPlayerCar);

            // 0x8279AF0C..0x8279AF50: while the racing line is initialised, cache the centre
            // line under the car; otherwise clear the "known" flag.
            if (GetRacingLine().mbIsInitialised)
            {
#if BRN_AI_RACINGLINE_STACK_PRESENT
                GetRacingLine().mbCentreLineHereKnown =
                    mRacingLineGenerator.GetCentreCentreLineHere(&GetRacingLine(),
                                                                 &GetRacingLine().mCentreHere,
                                                                 &GetRacingLine().mCentreAhead);
#else
                // [FLAG PC bring-up] RacingLineGenerator::GetCentreCentreLineHere has no body in
                // this tree; the console's own "not known" state is the safe stand-in
                // (GetQuickTurnSteering then takes its FindSignedAngleBetween2DVectors arm).
                // DELETE-WHEN BrnRacingLineGenerator.cpp lands and the macro flips.
                GetRacingLine().mbCentreLineHereKnown = false;
#endif
            }
            else
            {
                GetRacingLine().mbCentreLineHereKnown = false;   // stb 0, 0x1AF1
            }

            AICar* lpCar = mpCarHost;
            if (lpCar->IsCrashing())                       // *(car+0x1542)
            {
                UpdateBehaviour(lfTimeStep, lpPlayerCar);
                mfInvulnerableTime = KF_INVULNERABLE_AFTER_CRASH_TIME;  // 0x1D0C (flt_820C41F4)
            }
            else
            {
                if (mfInvulnerableTime > 0.0f)
                    mfInvulnerableTime = mfInvulnerableTime - lfTimeStep;

                m2DCarPos  = To2D(lpCar->GetPosition());   // 0x1CD0 (x,y)
                mfCarSpeed = mpCarHost->GetSpeed();            // 0x1D00

                // lbz 0x1541(car) == mbIsInAir -- an airborne car keeps its controls cleared.
                if (lpCar->mbIsInAir)
                {
                    mfAccelerator = 0.0f;  mbUseForcedSpeed = 0;
                    mfBrake       = 0.0f;  mbBoosting       = 0;
                    mfForcedSpeed = 0.0f;  mbWantToExitDrift = 0;
                    mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
                    UpdateSteeringAngle(0.0f);
                }
                else
                {
                    CalcDistanceFromPlayer(lPlayerCarPosition);
                    InitialiseRacingLine();
                    UpdateBehaviour(lfTimeStep, lpPlayerCar);

                    if (mbIsRacingLineInitialised)
                    {
                        if (!mbCurrentRouteComplete)
                            GenerateRacingLine(lfTimeStep);
                        if (mbIsRacingLineInitialised)
                            UpdateBrakingAnticipationData();
                    }

                    CalculateCarControls(lfTimeStep, lPlayerCarPosition, lbDoInRangeCatchup, lpRandom);

                    CGS_ASSERT(IsFinite(mfSteeringAngle), "RwMath::IsValid( mfSteeringAngle )");
                }
            }
        }
        else
        {
            mfAccelerator = 0.0f;  mbUseForcedSpeed = 0;
            mfBrake       = 0.0f;  mbBoosting       = 0;
            mfForcedSpeed = 0.0f;  mbWantToExitDrift = 0;
            mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
        }
    }

    // ====================================================================================
    // UpdateBehaviour @0x8279A680
    //
    // Tick the per-car behaviour state machine (meBehaviour @ car+5300). If the car is crashing,
    // force the CRASHING(7) behaviour first (saving the previous). Then dispatch on meBehaviour:
    //   0/9/10  -> clear the stuck timer;   1/2/8 -> no-op (return);
    //   3/4     -> stuck<->reverse toggle by the reverse flag, tick aggression + UpdateStuck;
    //   5       -> UpdateQuickTurn;         6 -> slow-turn behaviour + UpdateStuck;
    //   7       -> zero all controls + steering, then (if no longer crashing) restore behaviour 3;
    //   else    -> assert "Unknown AI behaviour".
    // ====================================================================================
    void AIDriver::UpdateBehaviour(f32 lfTimeStep, AICar* lpPlayerCar)
    {
        (void)lpPlayerCar;
        AICar* lpCar = mpCarHost;

        if (lpCar->IsCrashing())
        {
            const EAIBehaviour lePrev  = lpCar->meBehaviour;   // lwz 0x14B4
            lpCar->mfBehaviourTimer    = 0.0f;                 // stfs 0x14E0
            lpCar->mePreviousBehaviour = lePrev;               // stw  0x14B8
            lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRASHING;   // stw 0x14B4, 7
        }

        switch (lpCar->meBehaviour)
        {
            case 0:
            case 9:
            case 10:
                mfStuckTime = 0.0f;                          // 0x1D04
                break;

            case 1:
            case 2:
            case 8:
                return;

            case 3:
                // lbz 0x1C60(this) == mAggression.mbIsSuitableForAggression (AIAggression +0x60).
                // The X360 spells the test `(_cntlzw(byte) & 0x20) == 0`, which for a byte load
                // is exactly "the byte is non-zero".
                if (GetAggression()->IsSuitableForAggressionFlag())
                {
                    lpCar->mfBehaviourTimer    = 0.0f;                     // stfs 0x14E0
                    lpCar->mePreviousBehaviour = E_AI_BEHAVIOUR_CRUISING;   // stw 0x14B8, 3
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_FIGHTING;   // stw 0x14B4, 4
                }
                // [GUARD] 0x8279A768 `lwz r5, 0x1CE8(driver)` -- the aggression victim; the console never
                // has it null here (the producer that stores +0x1CE8/+0x1CF0 runs first). On the host it
                // can be (run1 AV in AIAggression::Update reading car+0x1549), so the tick is skipped.
                if (meAggressionVictimHost != -1 && GetAggressionVictimCar() != 0)   // lwz 0x1CF0 != -1
                    GetAggression()->Update(lfTimeStep, GetAggressionVictimCar());
                UpdateStuck(lfTimeStep);
                break;

            case 4:
                if (!GetAggression()->IsSuitableForAggressionFlag())
                {
                    lpCar->mfBehaviourTimer    = 0.0f;
                    lpCar->mePreviousBehaviour = E_AI_BEHAVIOUR_FIGHTING;   // stw 0x14B8, 4
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;   // stw 0x14B4, 3
                }
                // [GUARD] 0x8279A7BC (r5 not reloaded -- the case-3 victim) `lwz r5, 0x1CE8(driver)` -- the aggression victim; the console never
                // has it null here (the producer that stores +0x1CE8/+0x1CF0 runs first). On the host it
                // can be (run1 AV in AIAggression::Update reading car+0x1549), so the tick is skipped.
                if (GetAggressionVictimCar() != 0)
                    GetAggression()->Update(lfTimeStep, GetAggressionVictimCar());
                UpdateStuck(lfTimeStep);
                break;

            case 5:
                UpdateQuickTurn();
                break;

            case 6:
                DoSlowTurnBehaviour();
                UpdateStuck(lfTimeStep);
                break;

            case 7:
                mfAccelerator = 0.0f;  mfBrake = 0.0f;
                mfForcedSpeed = 0.0f;  mfHandBrake = 0.0f;
                mbUseForcedSpeed = 0;  mbBoosting = 0;
                mbWantToExitDrift = 0; mbWantToEnterDrift = 0;
                UpdateSteeringAngle(0.0f);
                mfStuckTime = 0.0f;
                if (!lpCar->IsCrashing())
                {
                    const EAIBehaviour lePrev  = lpCar->meBehaviour;
                    lpCar->mfBehaviourTimer    = 0.0f;
                    lpCar->mePreviousBehaviour = lePrev;
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;
                }
                break;

            default:
                CGS_ASSERT(false, "Unknown AI behaviour");
                break;
        }
    }

    // ====================================================================================
    // UpdateSteeringAngle @0x827708F0  -- the STEER ACTUATOR.
    //
    // Step the stored steering angle toward lfTargetAngle. The target is FLIPPED when the car is
    // reversing (speed < 0). The per-frame step rate is behaviour-dependent (non-player cars use
    // 1.0; player cars pick a rate by meBehaviour). Then StepTo() the angle and store it.
    // ====================================================================================
    void AIDriver::UpdateSteeringAngle(f32 lfTargetAngle)
    {
        const f32 lfSpeed = mpCarHost->GetSpeed();
        if (lfSpeed < 0.0f)
            lfTargetAngle = -lfTargetAngle;     // reverse: flip the target

        f32 lfRate;
        if (!mpCarHost->IsPlayerCar())              // lbz 0x1549(car)
        {
            lfRate = KF_AI_STEERING_STEP;           // flt_82001C98 == 1.0
        }
        else
        {
            const s32 liBeh = static_cast<s32>(mpCarHost->meBehaviour);   // lwz 0x14B4(car)
            if (liBeh == 1 || liBeh == 9 || liBeh == 10)
                lfRate = KF_PLAYER_ROLLING_START_STEERING_STEP;   // flt_820C4228 == 0.02
            else if (liBeh == 2)
                lfRate = KF_PLAYER_DRIVE_THRU_STEERING_STEP;      // flt_820C422C == 0.03
            else
                lfRate = KF_PLAYER_STEERING_STEP;                 // flt_820C424C == 0.1
        }

        mfSteeringAngle = StepTo(mfSteeringAngle, lfTargetAngle, lfRate);
    }

    // ====================================================================================
    // CalculateSteeringAngle @0x8277CD18  -- THE CORE STEERING MATHS.
    //
    // When the racing line is initialised:
    //   1. heading = normalise2D( useful-or-plain car direction );
    //   2. target  = GetTargetPosition();  steeringTargetVector = normalise2D(target - 2DCarPos)
    //      (asserts the raw target delta is finite);
    //   3. angle   = FindSignedAngleBetween2DVectors( heading, steeringTargetVector ) -> stored to
    //      mfCalculatedSteeringAngle (asserts finite);
    //   4. PID step: choose the drift vs normal controller by the car's useful-direction flag;
    //      pid.Record(angle, dt); out = pid.GetOutput();
    //   5. mfPIDOutput = clamp( out, -1, +1 );  then UpdateSteeringAngle( mfPIDOutput ).
    // ====================================================================================
    void AIDriver::CalculateSteeringAngle(f32 lfTimeStep)
    {
        if (!mbIsRacingLineInitialised)
            return;

        const bool lbUseful = mpCarHost->mbIsDrifting;   // lbz 0x1544(car) == mbIsDrifting

        // 1. current heading.
        Vector2 lHeading;
        if (lbUseful)
            lHeading = Normalize2D(To2D(mpCarHost->GetUsefulDirection()));
        else
            lHeading = Normalize2D(To2D(mpCarHost->GetDirection()));

        // 2. target delta -> steering target vector.
        const Vector2 lTarget = GetTargetPosition();
        Vector2 lDelta;
        lDelta.x = lTarget.x - m2DCarPos.x;
        lDelta.y = lTarget.y - m2DCarPos.y;
        lDelta.z = 0.0f; lDelta.w = 0.0f;
        CGS_ASSERT(IsFinite(lDelta.x) && IsFinite(lDelta.y), "Invalid target vector");
        mSteeringTargetVector = Normalize2D(lDelta);             // 0x1C70

        // 3. signed planar angle heading -> steering target.
        const f32 lfAngleToTarget =
            FindSignedAngleBetween2DVectors(lHeading, mSteeringTargetVector);
        CGS_ASSERT(IsFinite(lfAngleToTarget), "RwMath::IsValid( lfAngleToTarget )");
        mfCalculatedSteeringAngle = lfAngleToTarget;             // 0x1D50

        // 4. PID controller (the drift controller while the car is drifting, else the normal
        //    one). X360 @0x8277CE.. `addi r3, this, 0x1B98` vs `addi r3, this, 0x1B34`.
        PIDController& lrPid = lbUseful ? mPIDControllerDrift : mPIDController;
        lrPid.Record(lfAngleToTarget, lfTimeStep);
        const f32 lfPidOut = lrPid.GetOutput();

        // 5. clamp(pidOut, -1, +1) -> mfPIDOutput; steer toward it. The X360 stores the PID
        // output clamped DIRECTLY (asm: f0=flt_820037C8=-1.0; fsubs/fsel max(out,-1) then
        // min(.,+1); stfs 0x1D54). There is NO negation of the PID output.
        f32 lfSteer = lfPidOut;
        if (lfSteer < -1.0f) lfSteer = -1.0f;
        if (lfSteer >  1.0f) lfSteer =  1.0f;
        mfPIDOutput = lfSteer;                                   // 0x1D54
        UpdateSteeringAngle(lfSteer);
    }

    // ====================================================================================
    // CalculateCarControls @0x827998C0  -- THROTTLE/BRAKE/STEER DISPATCH.
    //
    // Clears the control outputs + flags, computes the desired speed (CalculateDesiredSpeed ->
    // mfDesiredSpeed), then dispatches on the CAR's behaviour (car+5300):
    //   0  (drive-to-line): CalculateSteeringAngle; if speed past KF_STOP_BRAKE_SPEED, force a
    //                       full brake (brake=1, forcedSpeed=0, useForcedSpeed=1);
    //   1-4,10 (driving)  : DoDrivingBehaviour;
    //   5  (quick turn)   : UpdateSteeringAngle(mQuickTurnSteeringLock);
    //   6  (slow turn)    : DoSlowTurn;
    //   7  (crashing)     : leave controls cleared;
    //   8  (launch/boost) : full accelerate (accel=1), forcedSpeed=-1, no boost/steer;
    //   9  (settle)       : CalculateSteeringAngle; light 0.2 brake;
    //   else              : assert "AI driver with unknown behaviour".
    // Finally asserts the steering angle is finite.
    // ====================================================================================
    void AIDriver::CalculateCarControls(f32 lfTimeStep, Vector3 lPlayerCarPosition,
                                        bool lbDoInRangeCatchup, CgsNumeric::Random* lpRandom)
    {
        // r6 (the module's Random) is set up by the caller and never read by the console body.
        (void)lpRandom;

        mfAccelerator      = 0.0f;  // 0x1D28
        mfBrake            = 0.0f;  // 0x1D2C
        mbUseForcedSpeed   = 0;     // 0x1D66
        mfForcedSpeed      = 0.0f;  // 0x1D18
        mbBoosting         = 0;     // 0x1D6B
        mfHandBrake        = 0.0f;  // 0x1D30
        mbWantToExitDrift  = 0;     // 0x1D64
        mbWantToEnterDrift = 0;     // 0x1D65

        // X360 @0x827998E4 `mr r4, r5` -- CalculateDesiredSpeed(v1 = the player position it was
        // handed, r4 = lbDoInRangeCatchup). Neither is read by that body (see the partfile).
        CalculateDesiredSpeed(lPlayerCarPosition, lbDoInRangeCatchup);   // -> mfDesiredSpeed

        switch (mpCarHost->meBehaviour)
        {
            case 0:
                CalculateSteeringAngle(lfTimeStep);
                if (mpCarHost->GetSpeed() > KF_STOP_BRAKE_SPEED)   // flt_8300D984 == 5 mph
                {
                    mfForcedSpeed    = 0.0f;       // 0x1D18
                    mfBrake          = 1.0f;       // 0x1D2C
                    mbUseForcedSpeed = 1;          // 0x1D66
                }
                break;

            case 1:
            case 2:
            case 3:
            case 4:
            case 10:
                DoDrivingBehaviour(lfTimeStep);
                break;

            case 5:
                UpdateSteeringAngle(mQuickTurnSteeringLock);     // 0x1CF8
                break;

            case 6:
                DoSlowTurn(lfTimeStep);
                break;

            case 7:
                break;

            case 8:
                mfBrake            = 0.0f;     // 0x1D2C (stfs f31)
                mbBoosting         = 0;        // 0x1D6B
                mbWantToExitDrift  = 0;        // 0x1D64
                mfAccelerator      = 1.0f;     // 0x1D28 (flt_82001C98 == 1.0)
                mbWantToEnterDrift = 1;        // 0x1D65
                mfSteeringAngle    = -1.0f;    // 0x1D24 (flt_820037C8 == -1.0) -- launch hard left
                break;

            case 9:
                CalculateSteeringAngle(lfTimeStep);
                mbUseForcedSpeed = 0;          // 0x1D66
                mfBrake          = KF_POST_RACE_BRAKE;   // 0x1D2C (flt_820C4300 == 0.2)
                break;

            default:
                CGS_ASSERT(false, "AI driver with unknown behaviour");
                break;
        }

        CGS_ASSERT(IsFinite(mfSteeringAngle), "RwMath::IsValid( mfSteeringAngle )");
    }

    // ====================================================================================
    // ComputeRouteDirection @0x82766500
    //
    // Where to aim along the ROUTE: needs a valid route with >1 node. Take the car's next route
    // node; if it is node 0 (or earlier) aim toward node 1, else aim FROM the previous node TO the
    // next node. The direction is normalise2D(toNode - fromNode), written to lrOutDirection.
    // Returns true on success, false when there is no usable route.
    // ====================================================================================
    bool AIDriver::ComputeRouteDirection(Vector2& lrOutDirection)
    {
        AICar* lpCar = mpCarHost;

        // 0x82766518..0x82766540: route validity == (meStatus (car+0x1408) != UNINITIALISED &&
        // miNodeCount (car+0x1400) > 0). The X360 is `cmpwi r11,0 ; bgt` -- strictly > 0, NOT > 1.
        const Route* lpRoute = lpCar->GetRoute();   // mRoute is the AICar's own base
        if (!(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpRoute->GetNodeCount() > 0))
        {
            return false;
        }

        const s32 liNextNode = lpCar->miNextRouteNodeIndex;   // lwz 0x1524(car)

        const RouteNode* lpNext = lpRoute->GetNode(liNextNode);
        const RouteNode* lpFrom;
        const RouteNode* lpTo;
        if (liNextNode <= 0)
        {
            lpFrom = lpNext;                 // node 0 -> aim at node 1
            lpTo   = lpRoute->GetNode(1);
        }
        else
        {
            lpFrom = lpRoute->GetNode(liNextNode - 1);
            lpTo   = lpNext;
        }

        Vector2 lDir;
        lDir.x = lpTo->GetX() - lpFrom->GetX();
        lDir.y = lpTo->GetY() - lpFrom->GetY();
        lDir.z = 0.0f; lDir.w = 0.0f;
        lrOutDirection = Normalize2D(lDir);
        return true;
    }

    // ====================================================================================
    // GetTargetPosition @0x8277CBF8  (sret Vector2)
    //
    // Where to aim THIS frame. When the car wants a direct-target line (mbIsDrivenByPlayer-style
    // flag @car+5450): aim at the car's position offset by its normalised facing (a unit-ahead
    // look point). Otherwise delegate to the SteeringFan's GetDrivingTarget. Returns the 2D point.
    // ====================================================================================
    Vector2 AIDriver::GetTargetPosition()
    {
        AICar* lpCar = mpCarHost;
        Vector2 lResult;

        if (lpCar->mbIsDrivenByPlayer)                           // lbz 0x154A(car)
        {
            const Vector3 lPos = lpCar->GetPosition();
            const Vector2 lDir = Normalize2D(To2D(lpCar->GetDirection()));
            lResult.x = lPos.x + lDir.x;                         // position + unit facing
            lResult.y = lPos.y + lDir.y;
            lResult.z = 0.0f; lResult.w = 0.0f;
        }
        else
        {
            // X360 asm: SteeringFan::GetDrivingTarget(out, this+1808 (== mSteeringFan @0x710),
            // car, this+3872 (== mRacingLine @0xF20), this+6960 (== mRacingLineGenerator @0x1B30),
            // this (the driver's NearbyVehicles @0x000), 0).
#if BRN_AI_RACINGLINE_STACK_PRESENT
            lResult = mSteeringFan.GetDrivingTarget(lpCar,
                                                    &GetRacingLine(),
                                                    &mRacingLineGenerator,
                                                    GetNearbyVehicles(),
                                                    false);
#else
            // [FLAG PC bring-up] SteeringFan::GetDrivingTarget @0x82779C30 and the 25 weighting
            // contributors it drives have no bodies in this tree. FALLBACK (documented, NOT the
            // console's): aim one metre along the car's ROUTE direction, i.e. the same look-ahead
            // point the direct-target arm above builds, but steered by ComputeRouteDirection
            // @0x82766500 (the console's own route heading) instead of the raw car facing. That
            // keeps a rival driving its route while the fan is absent; when there is no usable
            // route it degenerates to the car's own facing, exactly like the arm above.
            // DELETE-WHEN BrnAISteeringFan.cpp's weighting half lands and the macro flips.
            {
                Vector2 lRouteDir;
                if (!ComputeRouteDirection(lRouteDir))
                {
                    lRouteDir = Normalize2D(To2D(lpCar->GetDirection()));
                }
                lResult.x = m2DCarPos.x + lRouteDir.x;
                lResult.y = m2DCarPos.y + lRouteDir.y;
                lResult.z = 0.0f;
                lResult.w = 0.0f;
            }
#endif
        }

        return lResult;
    }

    // ====================================================================================
    // AttemptToDriveAtDesiredSpeed @0x827706D8  -- the SPEED ACTUATOR.
    //
    // lfSpeedDelta is this frame's allowed speed change. Clears accel/brake/handbrake. The
    // boost-time slot doubles as a force-boost timer: if >0, count it down by lfSpeedDelta and
    // mark not-boosting; else if CheckForBoosting(), latch a 3.0s boost window and set boosting;
    // else clear boosting. Then close the speed loop: if currentSpeed < desiredSpeed, ramp the
    // ACCELERATOR up across the KF_ACCELERATION_MAX_DIFF (20 mph) band; if currentSpeed >
    // desiredSpeed, ramp the BRAKE up across the [KF_BRAKING_START_DIFF, KF_BRAKING_MAX_DIFF]
    // (20 .. 40 mph) band. Each ramp = clamp(gap/width, 0, 1) --
    // i.e. the ramp GROWS with the speed gap (asm: fsel max(x,0) then fsel min(.,1); no '1-x').
    // ====================================================================================
    void AIDriver::AttemptToDriveAtDesiredSpeed(f32 lfTimeStep)
    {
        const f32 lfBoostTimer = mfBoostTimeRemaining;       // 0x1D44
        mfBrake       = 0.0f;                                 // 0x1D2C
        mfAccelerator = 0.0f;                                 // 0x1D28
        mfHandBrake   = 0.0f;                                 // 0x1D30

        if (lfBoostTimer <= 0.0f)
        {
            if (CheckForBoosting())
            {
                mfBoostTimeRemaining = KF_FORCED_BOOST_TIME;  // flt_820C4154 == 3.0 s
                mbBoosting = 1;
            }
            else
            {
                mbBoosting = 0;
            }
        }
        else
        {
            mbBoosting = 1;
            mfBoostTimeRemaining = lfBoostTimer - lfTimeStep;
        }

        const f32 lfSpeed   = mpCarHost->GetSpeed();
        const f32 lfDesired = mfDesiredSpeed;                 // 0x1D10

        if (lfSpeed < lfDesired)
        {
            // under speed -> accelerate. ramp = clamp((desired - speed) / 20 mph, 0, 1).
            mfAccelerator = Saturate((lfDesired - lfSpeed) / KF_ACCELERATION_MAX_DIFF);   // 0x1D28
        }
        else if (lfSpeed > lfDesired)
        {
            // over speed -> brake across the [20 mph, 40 mph] over-speed band.
            mfBrake = Saturate(((lfSpeed - lfDesired) - KF_BRAKING_START_DIFF)
                               / (KF_BRAKING_MAX_DIFF - KF_BRAKING_START_DIFF));          // 0x1D2C
        }
    }

    // ====================================================================================
    // CorneringTopSpeed @0x8277D0F0
    //
    // Clamp the target speed by how sharp the upcoming corner is. Computes two unsigned planar
    // angles -- between the car's useful direction and mBrakingRoadDir, and between that SAME
    // useful direction and the (m2DCarPos -> mBrakingAnticipationPos) racing-line direction --
    // whose MAX is the cornering angle, stored to mfAngleToBrakingTarget (asm @0x8277D238/0x8277D258:
    // fsubs is only the fsel condition, fsel picks max(angle1, angle2), not the difference). A ramp
    // clamp((angle - BASE)/RANGE, 0, 1) lerps from inputSpeed toward (inputSpeed * SCALE) to form
    // the cornering speed cap, returned to the caller (X360 returns it via fp1).
    // ====================================================================================
    f32 AIDriver::CorneringTopSpeed(f32 lfInputSpeed)
    {
        const Vector2 lUseful = Normalize2D(To2D(mpCarHost->GetUsefulDirection()));
        const f32 lfAngle1 = FindUnsignedAngleBetween2DVectors(lUseful, mBrakingRoadDir); // 0x1CB0

        Vector2 lToAnticip;
        lToAnticip.x = mBrakingAnticipationPos.x - m2DCarPos.x;   // 0x1CA0 - 0x1CD0
        lToAnticip.y = mBrakingAnticipationPos.y - m2DCarPos.y;
        lToAnticip.z = 0.0f; lToAnticip.w = 0.0f;
        const Vector2 lAnticipDir = Normalize2D(lToAnticip);
        // X360: the SECOND FindUnsignedAngleBetween2DVectors call reuses the normalised useful
        // direction (v2 := v125, the first normalise result) as its first arg and the normalised
        // (anticipPos - carPos) direction as its second -- i.e. angle2 = angle(usefulDir,
        // anticipDir). (NOT angle(anticipDir, anticipDir), which would always be 0.)
        const f32 lfAngle2 = FindUnsignedAngleBetween2DVectors(lUseful, lAnticipDir);

        const f32 lfCorneringAngle = (lfAngle1 >= lfAngle2) ? lfAngle1 : lfAngle2;
        mfAngleToBrakingTarget = lfCorneringAngle;               // 0x1CFC

        const f32 lfRamp = Saturate((lfCorneringAngle - KF_MIN_BRAKING_ANGLE)   // flt_82F3038C, 30 deg
                                    / KF_BRAKING_ANGLE_RANGE);                 // flt_8300DC38, 60 deg

        const f32 lfScaled = lfInputSpeed * KF_AI_MAX_BRAKING_SPEED_PROPORTION;   // flt_820C41FC
        // cap = inputSpeed + (scaled - inputSpeed) * ramp -- lerps from inputSpeed (ramp==0, no
        // cornering yet) toward the scaled-down speed (ramp==1, sharp corner).
        return lfInputSpeed + (lfScaled - lfInputSpeed) * lfRamp;
    }

    // ====================================================================================
    // ProximitySpeed @0x82770800
    //
    // Clamp the target speed by how close the car is to its proximity reference. t = clamp(
    // (proximityRef - 4.0) * 0.25, 0, 1). v = max( (currentSpeed - speedRef - 10 mph),
    // lfMinSpeed*0.5 ). Returns the lerp v + (lfMinSpeed - v) * t (drops toward lfMinSpeed as the
    // proximity ref grows; asm @0x827708C4/0x827708C8: vsubfp minSpeed-v, vmaddfp (minSpeed-v)*t+v).
    // ====================================================================================
    f32 AIDriver::ProximitySpeed(f32 lfMinSpeed)
    {
        // this+0x1A3C / +0x1A40 == mRacingLine.mfImmediateDistanceToTrafficImpact /
        // mfImmmediateApproachSpeedOfTrafficAhead (RacingLine +0xB1C / +0xB20).
        const f32 lfProxRef  = GetRacingLine().mfImmediateDistanceToTrafficImpact;
        const f32 lfSpeedRef = GetRacingLine().mfImmmediateApproachSpeedOfTrafficAhead;

        const f32 lfT = Saturate((lfProxRef - KF_PROXIMITY_CLOSE)      // flt_820C41C0 == 4.0
                                 * KF_PROXIMITY_FAR_RECIP);            // flt_82003F40 == 0.25

        const f32 lfSpeed = mpCarHost->GetSpeed();
        f32 lfV = (lfSpeed - lfSpeedRef) - K_PROXIMITY_SPEED_REDUCTION;   // flt_8300DC34 == 10 mph
        const f32 lfFloor = lfMinSpeed * KF_PROXIMITY_MIN_SPEED_SCALE;    // flt_820C4168 == 0.5
        if (lfV < lfFloor)
            lfV = lfFloor;

        return lfV + (lfMinSpeed - lfV) * lfT;
    }

    // ====================================================================================
    // ChooseRaceSteeringFan @0x82766370
    //
    // Pick the SteeringFan bias mode for RACE mode from the car's mode/relative state. Returns 0
    // (default) unless: the route-finding style is non-default (!=3 and !=0), the relative-location
    // is neither 0 nor 1, and a positive schedule gate puts the car ahead within
    // clamp(gate*0.1, 0, 1)*50 -> 1 (overtake bias); else if the relative-location is 3 and
    // (opponentIndex % 3) != 0 -> 5; else 0.
    // ====================================================================================
    s32 AIDriver::ChooseRaceSteeringFan(AICar* lpCar)
    {
        const s32 leStyle = static_cast<s32>(lpCar->GetRouteFindingStyle()); // car+0x14C0 (5312)
        if (leStyle == 3 || leStyle == 0)
            return 0;

        const s32 luRelLoc = static_cast<s32>(lpCar->meRelativeLocation);    // car+0x14D4 (5332)
        // asm @0x8276638C/0x82766394: two exact equality tests (==1, ==0), not a relational
        // compare -- a negative meRelativeLocation would fall through here in the binary.
        if (luRelLoc == 1 || luRelLoc == 0)
            return 0;

        const f32 lfScheduleGate = lpCar->mfScheduleOffset0;                 // lfs 0x1518(car)
        if (lfScheduleGate > 0.0f)
        {
            const f32 lfDistAhead = lpCar->mfDistanceAheadOfPlayer;          // lfs 0x14F8(car)
            const f32 lfClamped   = Saturate(lfScheduleGate * 0.1f);         // flt_820C424C == 0.1
            if (lfDistAhead > 0.0f)
            {
                if (lfDistAhead < lfClamped * K_NORMAL_SPREAD_HNG)           // flt_820C4244 == 50.0
                    return 1;
            }
        }

        if (luRelLoc != 3)
            return 0;

        const s8 liOpponent = lpCar->GetOpponentIndex();                     // car+0x153A (5434)
        if ((liOpponent % 3) == 0)
            return 0;
        return 5;
    }

    // ====================================================================================
    // SetDrivingFanBiases @0x82770428
    //
    // Select + weight the SteeringFan contributors for the car's current mode. Priority ladder
    // (each rung sets a bias-mode constant on the fan):
    //   - player-driven-AI-not-this, forced-standard or behaviour 2 -> bias 8;
    //   - behaviour 1                                                -> bias 4;
    //   - else by the aggression-machine state (this+0x1C00): state 3 & player NOT protected ->
    //     bias 2; state 13 -> bias 9; otherwise choose aggressive vs race fan from the car's
    //     route-finding style (style 2 or 6 -> aggressive, else race) and SetBiasMode with it.
    // ====================================================================================
    void AIDriver::SetDrivingFanBiases(AICar* lpPlayerCar)
    {
        AICar* lpCar = mpCarHost;

        const bool lbPlayer         = lpCar->IsPlayerCar();            // lbz 0x1549(car)
        const bool lbDrivenByPlayer = lpCar->mbIsDrivenByPlayer;      // lbz 0x154A(car)
        const bool lbForceStandard  = lpCar->ForceStandardRoute();    // lbz 0x1548(car)
        const s32  liBehaviour      = static_cast<s32>(lpCar->meBehaviour);   // lwz 0x14B4(car)

        if (lbPlayer && !lbDrivenByPlayer && (lbForceStandard || liBehaviour == 2))
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(8));
            return;
        }

        if (liBehaviour == 1)
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(4));
            return;
        }

        // lwz 0x1C00(this) == mAggression.meAggressionState (AIAggression +0x00).
        const EAIAggressionState leAggressionState = GetAggression()->GetAggressionState();
        if (leAggressionState == E_AI_AGGRESSION_STATE_ATTACK_SLAM && !IsPlayerProtected(lpPlayerCar))
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(2));
            return;
        }

        if (leAggressionState == E_AI_AGGRESSION_STATE_VEER_EXTREME)
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(9));
            return;
        }

        const s32 leStyle = static_cast<s32>(lpCar->GetRouteFindingStyle()); // lwz 0x14C0(car)
        const bool lbAggressive = (leStyle == 2 || leStyle == 6);
        const s32 leBias = lbAggressive ? ChooseAggressiveSteeringFan(lpPlayerCar)
                                        : ChooseRaceSteeringFan(lpCar);
        mSteeringFan.SetBiasMode(static_cast<EBiasMode>(leBias));
    }

    // ====================================================================================
    // IsInvulnerable @0x82765740
    //
    // Whether the AI car is currently protected from takedowns. Asserts mpCar is bound. When the
    // car is BEHIND the player (meRelativeLocation < 2 -- E_RELATIVE_BEHIND_* == 0/1): invulnerable
    // if it is more than 30m away OR the invuln timer is still positive. When AHEAD of the player
    // (meRelativeLocation >= 2): invulnerable only while the invuln timer is positive. The X360
    // tests meRelativeLocation for exact ==1 / ==0 (both branch to the distance arm), which is the
    // Hex-Rays `< 2u` form.
    // ====================================================================================
    bool AIDriver::IsInvulnerable() const
    {
        CGS_ASSERT(mpCarHost != nullptr, "mpCar != NULL");

        if (mpCarHost->meRelativeLocation < 2)          // car+0x14D4 (behind player: 0 or 1)
        {
            if (mfDistanceToPlayer > KF_INVULNERABLE_BEHIND_DISTANCE)   // flt_820C3FA8 == 30.0
                return true;
            if (mfInvulnerableTime > 0.0f)              // 0x1D0C (flt_82001CC0 == 0.0)
                return true;
            return false;
        }

        return mfInvulnerableTime > 0.0f;               // ahead of player: timer only
    }

    // ====================================================================================
    // IsOnStartLine @0x82765800
    //
    // Whether the AI car is sitting on the start line. Returns false when no car is bound. Asserts
    // the car is not INACTIVE (a start-line query on a dead car is a logic error), then returns the
    // car's own on-start-line flag (car+0x1547).
    // ====================================================================================
    bool AIDriver::IsOnStartLine()
    {
        AICar* lpCar = mpCarHost;                       // 0x1CE0
        if (!lpCar)
            return false;

        CGS_ASSERT(lpCar->meCarState != E_AI_CAR_STATE_INACTIVE,  // car+0x14C8 != 2
                   "GetState() != E_AI_CAR_STATE_INACTIVE");

        return lpCar->mbIsOnStartLine != 0;             // car+0x1547
    }
}
