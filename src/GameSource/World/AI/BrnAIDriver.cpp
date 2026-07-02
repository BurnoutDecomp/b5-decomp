#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAICar.h"                  // AICar (committed minimal-slice home + accessors)
#include "GameSource/World/AI/BrnAIUtils.h"                // BrnAI::StepTo (committed step-toward helper)
#include "GameSource/World/AI/BrnAIAggression.h"           // AIAggression (committed embedded sub-machine @this+0x1C00)
#include "GameSource/World/AI/Route/BrnRacingLine.h"       // RacingLine (committed embedded sub-object @this+0xF20)

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cmath>   // std::sqrt (de-SIMD'd 2D normalise), std::isfinite (RwMath::IsValid)

// BrnAI::AIDriver -- the STEERING + CAR-CONTROL link. This TU bodies the 15 members that turn a
// navigation target (heading + desired speed) into the car's physics inputs (steer / throttle /
// brake / hand-brake + boost). Reconstructed store-for-store from the X360 pseudocode/asm; the
// inlined SIMD 2D-vector normalise (vrsqrtefp + two Newton-Raphson refine steps) and the inlined
// vector lerps are reversed into the logical scalar/Vector2 maths they implement. Every call into
// a collaborator is restored as a NAMED CALL.
//
// COLLABORATOR HOMES:
//   * AICar (BrnAICar.h) and StepTo (BrnAIUtils.h) are committed -- used by name. The few AICar
//     fields the X360 reads that BrnAICar.h does not yet expose are read through small TU-local
//     offset accessors (CarFlagAt / CarFloatAt), documented as the de-inlined direct load and
//     NOT growing AICar's layout.
//   * The embedded sub-objects (SteeringFan @this+0x710, RacingLine @this+0xF20, the two
//     PIDControllers @this+0x1B34 / +0x1B98, the AIAggression sub-machine @this+0x1C00) have no
//     committed home; they are reached through minimal TU-LOCAL collaborator types built from
//     these documented sub-object offsets (the BrnRouteMapModule.cpp precedent).
//   * The free-function geometry helpers Find{Signed,Unsigned}AngleBetween2DVectors have no
//     committed home; declared TU-locally in their real BrnAI namespace.
//   * The remaining AIDriver members the bodies call (InitialiseRacingLine, CalculateDesiredSpeed,
//     DoDrivingBehaviour, ...) are declared-only in the header and bodied by their own recon
//     passes -- the faithful de-inlined form of the X360 `bl BrnAI__AIDriver__<name>` calls.
//
// FLAGGED rodata: where a tuning float is referenced only by ADDRESS in the X360 build (no
// immediate decoded by Hex-Rays) its VALUE is NOT recoverable from the dossier; it ships as a
// flagged placeholder (KF_*, value 0.0f) and is listed in open_questions. Literal immediates
// Hex-Rays decoded from the float bits (0.0/0.1/0.2/0.5/0.25/1.0/-1.0/2.0/3.0/4.0/50.0) are used
// inline. The shared zero/one/neg-one rodata are spelt as literals.

namespace BrnAI
{
    // ====================================================================================
    // TU-LOCAL collaborator declarations (no committed home yet; declare-only).
    // ====================================================================================

    // Signed/unsigned planar angle between two 2D direction vectors (radians). BrnAIUtils DWARF
    // surface (DistancePointToLine / ... / Find{Signed,Unsigned}AngleBetween2DVectors); bodied in
    // their own recon passes, so declared TU-locally here.
    f32 FindSignedAngleBetween2DVectors(Vector2 lFrom, Vector2 lTo);
    f32 FindUnsignedAngleBetween2DVectors(Vector2 lFrom, Vector2 lTo);

    // The embedded SteeringFan @this+0x710. Only the two entry points these bodies call are
    // declared. GetDrivingTarget's X360 signature is
    //   GetDrivingTarget(&out, fan, car, racingLine, racingLineGen, driver, 0)
    // -- reconstructed here as the minimal called shape.
    struct SteeringFanLocal
    {
        void    Prepare();
        void    SetBiasMode(s32 leBiasMode);
        Vector2 GetDrivingTarget(AICar* lpCar, void* lpRacingLine,
                                 void* lpRacingLineGen, AIDriver* lpDriver, int liFlags);
    };

    // The embedded PIDController @this+0x1B34 (normal) / +0x1B98 (drift). Record(error, dt) then
    // GetOutput() -- the classic PID step.
    struct PIDControllerLocal
    {
        void Record(f32 lfError, f32 lfTimeStep);
        f32  GetOutput();
    };

    // ====================================================================================
    // file-local tuning constants (rodata referenced by address; values FLAGGED).
    // ====================================================================================
    // AttemptToDriveAtDesiredSpeed -- accelerator/brake ramp bands around the desired speed.
    const f32 KF_ATDS_OVER_LO  = 0.0f;  // flt_8300D96C -- FLAG: rodata unrecovered (over-speed band lo)
    const f32 KF_ATDS_OVER_HI  = 0.0f;  // flt_8300DB0C -- FLAG: rodata unrecovered (over-speed band hi)
    const f32 KF_ATDS_UNDER    = 1.0f;  // flt_8300D9A8 -- FLAG: rodata unrecovered (under-speed accel band; 1.0 keeps the divide safe)

    // CalculateCarControls -- case-0 (drive-to-line) full-brake speed threshold.
    const f32 KF_CCC_FULLBRAKE_SPEED = 0.0f;  // flt_8300D984 -- FLAG: rodata unrecovered

    // CorneringTopSpeed -- cornering-angle -> speed-fraction ramp.
    const f32 KF_CORNER_ANGLE_SCALE = 0.0f;   // flt_820C41FC -- FLAG: rodata unrecovered (input-speed -> angle scale)
    const f32 KF_CORNER_ANGLE_BASE  = 0.0f;   // flt_82F3038C -- FLAG: rodata unrecovered (angle subtract / threshold)
    const f32 KF_CORNER_ANGLE_RANGE = 1.0f;   // flt_8300DC38 -- FLAG: rodata unrecovered (angle ramp width; 1.0 keeps the divide safe)

    // ProximitySpeed -- proximity over-speed floor.
    const f32 KF_PROX_SPEED_FLOOR = 0.0f;     // flt_8300DC34 -- FLAG: rodata unrecovered (min over-speed offset)

    // UpdateSteeringAngle -- per-behaviour StepTo rates (rodata by address; UNRECOVERED).
    const f32 KF_STEER_RATE_A = 1.0f;  // flt_820C4228 -- FLAG: rodata unrecovered (player car, beh 1/9/10)
    const f32 KF_STEER_RATE_B = 1.0f;  // flt_820C422C -- FLAG: rodata unrecovered (player car, beh 2)
    const f32 KF_STEER_RATE_C = 0.1f;  // flt_820C424C -- RECOVERED: the same address is decoded as an
                                        // immediate 0.1 multiplier in ChooseRaceSteeringFan's asm
                                        // (@0x827663B8, "*(v1+5400) * 0.1"); player car, default rate.

    // ====================================================================================
    // TU-LOCAL helpers.
    // ====================================================================================

    // RwMath::IsValid( x ) -- the X360 finiteness check (vcmpeqfp x,x == "x is not NaN"; the
    // engine treats inf as invalid too).
    static inline bool IsFinite(f32 lfValue) { return std::isfinite(lfValue); }

    // Reach the embedded sub-objects at their documented this-relative offsets (no committed home;
    // de-inlined form of the X360 `addi r,this,<off>` sub-object address computation).
    static inline SteeringFanLocal* Fan(AIDriver* lpD)
    {
        return reinterpret_cast<SteeringFanLocal*>(reinterpret_cast<u8*>(lpD) + 0x710);
    }
    static inline PIDControllerLocal* Pid(AIDriver* lpD, bool lbDrift)
    {
        const std::size_t luOff = lbDrift ? 0x1B98u : 0x1B34u;
        return reinterpret_cast<PIDControllerLocal*>(reinterpret_cast<u8*>(lpD) + luOff);
    }
    // The embedded AIAggression sub-machine @this+0x1C00 -- a committed home (BrnAIAggression.h),
    // reached by name instead of a raw offset poke.
    static inline AIAggression* Aggression(AIDriver* lpD)
    {
        return reinterpret_cast<AIAggression*>(reinterpret_cast<u8*>(lpD) + 0x1C00);
    }
    // The embedded RacingLine sub-object @this+0xF20 -- a committed home (BrnRacingLine.h).
    static inline RacingLine* DriverRacingLine(AIDriver* lpD)
    {
        return reinterpret_cast<RacingLine*>(reinterpret_cast<u8*>(lpD) + 0xF20);
    }

    // Read an unexposed AICar byte flag / float by its documented guest offset (de-inlined direct
    // load of the X360 `lbz/lfs <off>(car)`; AICar's committed layout is NOT grown).
    static inline u8  CarFlagAt(AICar* lpCar, std::size_t luOff)
    {
        return *(reinterpret_cast<const u8*>(lpCar) + luOff);
    }
    static inline f32 CarFloatAt(AICar* lpCar, std::size_t luOff)
    {
        return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpCar) + luOff);
    }
    static inline void CarStoreFlagAt(AICar* lpCar, std::size_t luOff, u8 luValue)
    {
        *(reinterpret_cast<u8*>(lpCar) + luOff) = luValue;
    }
    static inline void CarStoreF32(AICar* lpCar, std::size_t luOff, f32 lfValue)
    {
        *reinterpret_cast<f32*>(reinterpret_cast<u8*>(lpCar) + luOff) = lfValue;
    }
    static inline void CarStoreS32(AICar* lpCar, std::size_t luOff, s32 liValue)
    {
        *reinterpret_cast<s32*>(reinterpret_cast<u8*>(lpCar) + luOff) = liValue;
    }
    // Read a 32-bit word from the car's route block by guest offset (miNodeCount @0x1400,
    // route-valid flag @0x1408, miNextRouteNodeIndex @0x1524) -- de-inlined direct load.
    static inline s32 CarFloatBitsS32(AICar* lpCar, std::size_t luOff)
    {
        return *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpCar) + luOff);
    }

    // Route node (x,y) for a node index. The X360 calls BrnAI::Route::GetNode(car's route, i) and
    // reads node[0]/node[1] (x,y). The Route lives inside the AICar (committed BrnRoute home);
    // declared TU-locally here as a free function so this slice does not pull in the Route home.
    void RouteNodeXY(AICar* lpCar, s32 liNodeIndex, f32* lpfOutX, f32* lpfOutY);

    // DriverReverseRequested mirrors the X360 `(_cntlzw(*(this+0x1C60)) & 0x20)==0` reverse-flag
    // test (@0x8279A728-0x8279A73C) -- for an 8-bit-zero-extended-to-32 load, cntlzw==32 only when
    // the byte is 0, so `(cntlzw&0x20)==0` reduces to "the byte is non-zero". Driver-local byte
    // flag @this+0x1C60 (no committed name for it yet); de-inlined direct load.
    static inline bool DriverReverseRequested(AIDriver* lpDriver)
    {
        return *(reinterpret_cast<const u8*>(lpDriver) + 0x1C60) != 0;
    }
    // AggressionUpdate ticks the embedded AIAggression sub-machine @this+0x1C00 (a committed home).
    // X360 @0x8279A764/0x8279A7B4: r3=this+0x1C00, r5=*(this+0x1CE8) (mpAggressionVictim), f1=dt --
    // BrnAI::AIAggression::Update(f32 lfTimeStep, const AICar* lpPlayerCar).
    static inline void AggressionUpdate(AIDriver* lpDriver, f32 lfTimeStep)
    {
        Aggression(lpDriver)->Update(lfTimeStep, lpDriver->GetAggressionVictimCar());
    }

    // Read a float / 32-bit word from this driver's own block by guest offset, for the embedded
    // sub-object scalars this slice does not name (RacingLine/avoidance proximity refs @0x1A3C /
    // 0x1A40, the aggression-machine state word @0x1C00) -- de-inlined direct loads.
    static inline f32 DriverFloatAtImpl(AIDriver* lpD, std::size_t luOff)
    {
        return *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpD) + luOff);
    }
    static inline s32 DriverS32AtImpl(AIDriver* lpD, std::size_t luOff)
    {
        return *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpD) + luOff);
    }
    static inline void DriverStoreS32At(AIDriver* lpD, std::size_t luOff, s32 liValue)
    {
        *reinterpret_cast<s32*>(reinterpret_cast<u8*>(lpD) + luOff) = liValue;
    }
    static inline void DriverStoreFlagAt(AIDriver* lpD, std::size_t luOff, u8 luValue)
    {
        *(reinterpret_cast<u8*>(lpD) + luOff) = luValue;
    }

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

    // 2D-flatten a Vector3 (drop z; the steering math is purely in the ground plane).
    static Vector2 To2D(Vector3 lVector)
    {
        Vector2 lResult;
        lResult.x = lVector.x;
        lResult.y = lVector.y;
        lResult.z = 0.0f;
        lResult.w = 0.0f;
        return lResult;
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

        Aggression(this)->Construct(this);  // mAggression.Construct(this) @this+0x1C00
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
        // BLOCKED (out of this TU's file scope): X360 @0x82796444 calls
        // BrnAI::AICar::SetDriver(car, this) here to wire the car back to its driver. AICar's
        // committed minimal-slice (BrnAICar.h) does not declare SetDriver -- adding the call
        // needs a declaration added to BrnAICar.h, which this TU may not edit.
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
    void AIDriver::Prepare(void* lpSectionsData, s32 leRelatedActiveCarIndex, void* lpRandom)
    {
        (void)lpRandom;
        mpSectionsDataHost = lpSectionsData;            // 0x1CE4
        ResetAttribSysValues();
        meRelatedActiveCarIndexHost = leRelatedActiveCarIndex; // 0x1CF4
        mfPerpendicularTarget   = 0.0f;             // 0x1D48
        DriverStoreS32At(this, 0x700, 0);           // mNearbyVehicles.miCount = 0
        mfHandBrake             = 0.0f;             // 0x1D30
        mbWantToEnterDrift      = 0;                // 0x1D65
        mActiveRouteTimeStamp   = 0;                // 0x1D5C
        DriverStoreS32At(this, 0x1CEC, E_DRIFT_STATE_NORMAL_DRIVING);  // meDriftState

        DriverRacingLine(this)->ClearSectionCache();  // mRacingLine.ClearSectionCache() @this+0xF20

        // byte flags @this+0x1AF0/+0x1AF1 = 0 (no committed names yet; de-inlined direct stores).
        DriverStoreFlagAt(this, 0x1AF0, 0);
        DriverStoreFlagAt(this, 0x1AF1, 0);

        // X360 draws a per-car random tuning float in a [lo,hi] range (the inlined LCG advance:
        // multiplier 0x5851F42D, increment +1; then a vector lerp of two rodata bounds, minus 1.0)
        // and seeds the perpendicular-target tuning from it. The exact range bounds are
        // rodata-by-address (UNRECOVERED); the LCG advance touches the supplied Random's state.
        // Restored store-for-store would require the Random + RacingLineGenerator homes, so the
        // perpendicular-tuning seed is left at its zero-init here and FLAGGED.

        Fan(this)->Prepare();                        // mSteeringFan.Prepare() @this+0x710

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
    void AIDriver::Update(f32 lfTimeStep, bool lbActive, Vector3 lTargetMovement,
                          AICar* lpPlayerCar, bool lbValidPlayer, void* lpRandom)
    {
        (void)lbActive;

        if (mbIsActive && lpPlayerCar)
        {
            UpdatePlayerTimers(lfTimeStep, lpPlayerCar);
            SetDrivingFanBiases(lpPlayerCar);

            // (X360 here samples the racing-line centre when the line is generating; that path
            //  reads the RacingLineGenerator @this+0xF20 and is bodied in its home.)

            AICar* lpCar = mpCarHost;
            if (lpCar->IsCrashing())                       // *(car+0x1542)
            {
                UpdateBehaviour(lfTimeStep, lpPlayerCar);
                mfInvulnerableTime = 2.0f;                 // 0x1D0C (flt_820C41F4 == 2.0)
            }
            else
            {
                if (mfInvulnerableTime > 0.0f)
                    mfInvulnerableTime = mfInvulnerableTime - lfTimeStep;

                m2DCarPos  = To2D(lpCar->GetPosition());   // 0x1CD0 (x,y)
                mfCarSpeed = mpCarHost->GetSpeed();            // 0x1D00

                // showtime-frozen flag @car+0x1541 (== car+5441).
                if (CarFlagAt(lpCar, 0x1541) != 0)
                {
                    mfAccelerator = 0.0f;  mbUseForcedSpeed = 0;
                    mfBrake       = 0.0f;  mbBoosting       = 0;
                    mfForcedSpeed = 0.0f;  mbWantToExitDrift = 0;
                    mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
                    UpdateSteeringAngle(0.0f);
                }
                else
                {
                    CalcDistanceFromPlayer(lTargetMovement);
                    InitialiseRacingLine();
                    UpdateBehaviour(lfTimeStep, lpPlayerCar);

                    if (mbIsRacingLineInitialised)
                    {
                        if (!mbCurrentRouteComplete)
                            GenerateRacingLine(lfTimeStep);
                        if (mbIsRacingLineInitialised)
                            UpdateBrakingAnticipationData();
                    }

                    CalculateCarControls(lfTimeStep, lTargetMovement, lbValidPlayer, lpRandom);

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
            const s32 liPrev = lpCar->meBehaviour;          // car+0x14B4
            CarStoreF32(lpCar, 0x14E0, 0.0f);               // behaviour timer (car+5344) = 0
            CarStoreS32(lpCar, 0x14B8, liPrev);             // previous behaviour (car+5304)
            lpCar->meBehaviour = E_AI_BEHAVIOUR_CRASHING;
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
                // X360: (_cntlzw(*(this+0x1C60)) & 0x20)==0 -> the aggression reverse flag is set.
                if (DriverReverseRequested(this))
                {
                    CarStoreF32(lpCar, 0x14E0, 0.0f);
                    CarStoreS32(lpCar, 0x14B8, E_AI_BEHAVIOUR_CRUISING);
                    lpCar->meBehaviour = E_AI_BEHAVIOUR_FIGHTING;
                }
                if (meAggressionVictimHost != -1)               // this+0x1CF0 (1852) != -1
                    AggressionUpdate(this, lfTimeStep);
                UpdateStuck(lfTimeStep);
                break;

            case 4:
                if (!DriverReverseRequested(this))
                {
                    CarStoreF32(lpCar, 0x14E0, 0.0f);
                    CarStoreS32(lpCar, 0x14B8, E_AI_BEHAVIOUR_FIGHTING);
                    lpCar->meBehaviour = E_AI_BEHAVIOUR_CRUISING;
                }
                AggressionUpdate(this, lfTimeStep);
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
                    const s32 liPrev = lpCar->meBehaviour;
                    CarStoreF32(lpCar, 0x14E0, 0.0f);
                    CarStoreS32(lpCar, 0x14B8, liPrev);
                    lpCar->meBehaviour = E_AI_BEHAVIOUR_CRUISING;
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
        if (!mpCarHost->IsPlayerCar())              // *(car+0x1549)
        {
            lfRate = 1.0f;
        }
        else
        {
            const s32 liBeh = mpCarHost->meBehaviour;   // car+0x14B4
            if (liBeh == 1 || liBeh == 9 || liBeh == 10)
                lfRate = KF_STEER_RATE_A;
            else if (liBeh == 2)
                lfRate = KF_STEER_RATE_B;
            else
                lfRate = KF_STEER_RATE_C;
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

        const bool lbUseful = (CarFlagAt(mpCarHost, 0x1544) != 0);   // "use useful direction" (car+5444)

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

        // 4. PID controller (drift controller on a useful-direction line, else normal).
        PIDControllerLocal* lpPid = Pid(this, lbUseful);
        lpPid->Record(lfAngleToTarget, lfTimeStep);
        const f32 lfPidOut = lpPid->GetOutput();

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
    //   0  (drive-to-line): CalculateSteeringAngle; if speed past KF_CCC_FULLBRAKE_SPEED, force a
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
    void AIDriver::CalculateCarControls(f32 lfTimeStep, Vector3 lTargetMovement,
                                        bool lbValidPlayer, void* lpRandom)
    {
        (void)lpRandom;

        mfAccelerator      = 0.0f;  // 0x1D28
        mfBrake            = 0.0f;  // 0x1D2C
        mbUseForcedSpeed   = 0;     // 0x1D66
        mfForcedSpeed      = 0.0f;  // 0x1D18
        mbBoosting         = 0;     // 0x1D6B
        mfHandBrake        = 0.0f;  // 0x1D30
        mbWantToExitDrift  = 0;     // 0x1D64
        mbWantToEnterDrift = 0;     // 0x1D65

        CalculateDesiredSpeed(lTargetMovement, lbValidPlayer);   // -> mfDesiredSpeed

        switch (mpCarHost->meBehaviour)
        {
            case 0:
                CalculateSteeringAngle(lfTimeStep);
                if (mpCarHost->GetSpeed() > KF_CCC_FULLBRAKE_SPEED)
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
                mfBrake          = 0.2f;       // 0x1D2C (flt_820C4300 == 0.2)
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

        // route validity: miRouteValid (car+0x1408 != 0) && miNodeCount (car+0x1400) > 1.
        const s32 liRouteValid = CarFloatBitsS32(lpCar, 0x1408);
        const s32 liNodeCount  = CarFloatBitsS32(lpCar, 0x1400);
        if (!(liRouteValid != 0 && liNodeCount > 1))
        {
            return false;
        }

        const s32 liNextNode = CarFloatBitsS32(lpCar, 0x1524);   // miNextRouteNodeIndex (car+0x1524)

        // RouteGetNode(car, index) -> &node; node x@+0, y@+4. Reached through the committed
        // BrnRoute home in the real call; here the node x/y are read via the car's route node
        // accessor (TU-local declare-only) so the math is faithful without the Route home.
        f32 lfFromX, lfFromY, lfToX, lfToY;
        f32 lfNX, lfNY; RouteNodeXY(lpCar, liNextNode, &lfNX, &lfNY);

        if (liNextNode <= 0)
        {
            f32 lfN1X, lfN1Y; RouteNodeXY(lpCar, 1, &lfN1X, &lfN1Y);
            lfFromX = lfNX;  lfFromY = lfNY;
            lfToX   = lfN1X; lfToY   = lfN1Y;
        }
        else
        {
            f32 lfPX, lfPY; RouteNodeXY(lpCar, liNextNode - 1, &lfPX, &lfPY);
            lfFromX = lfPX; lfFromY = lfPY;
            lfToX   = lfNX; lfToY   = lfNY;
        }

        Vector2 lDir;
        lDir.x = lfToX - lfFromX;
        lDir.y = lfToY - lfFromY;
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

        if (CarFlagAt(lpCar, 0x154A) != 0)                       // mbIsDrivenByPlayer (car+5450)
        {
            const Vector3 lPos = lpCar->GetPosition();
            const Vector2 lDir = Normalize2D(To2D(lpCar->GetDirection()));
            lResult.x = lPos.x + lDir.x;                         // position + unit facing
            lResult.y = lPos.y + lDir.y;
            lResult.z = 0.0f; lResult.w = 0.0f;
        }
        else
        {
            // mSteeringFan.GetDrivingTarget(car, mRacingLine, mRacingLineGen, this, 0). X360 asm:
            //   SteeringFan::GetDrivingTarget(out, this+1808, car, this+3872, this+6960, this, 0)
            // -- this+1808 == fan@0x710 (the implicit fan `this`); this+3872 == mRacingLine@0xF20;
            // this+6960 == 0x1B30 (the RacingLineGenerator scratch, NOT 0x710); this == driver.
            lResult = Fan(this)->GetDrivingTarget(
                          lpCar,
                          reinterpret_cast<u8*>(this) + 0xF20,   // mRacingLine        (this+3872)
                          reinterpret_cast<u8*>(this) + 0x1B30,  // racing-line-gen     (this+6960)
                          this,
                          0);
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
    // ACCELERATOR up across the KF_ATDS_UNDER band; if currentSpeed > desiredSpeed, ramp the BRAKE
    // up across the [KF_ATDS_OVER_LO, KF_ATDS_OVER_HI] band. Each ramp = clamp(gap/width, 0, 1) --
    // i.e. the ramp GROWS with the speed gap (asm: fsel max(x,0) then fsel min(.,1); no '1-x').
    // ====================================================================================
    void AIDriver::AttemptToDriveAtDesiredSpeed(f32 lfSpeedDelta)
    {
        const f32 lfBoostTimer = mfBoostTimeRemaining;       // 0x1D44
        mfBrake       = 0.0f;                                 // 0x1D2C
        mfAccelerator = 0.0f;                                 // 0x1D28
        mfHandBrake   = 0.0f;                                 // 0x1D30

        if (lfBoostTimer <= 0.0f)
        {
            if (CheckForBoosting())
            {
                mfBoostTimeRemaining = 3.0f;                 // latch a 3.0s boost window
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
            mfBoostTimeRemaining = lfBoostTimer - lfSpeedDelta;
        }

        const f32 lfSpeed   = mpCarHost->GetSpeed();
        const f32 lfDesired = mfDesiredSpeed;                 // 0x1D10

        if (lfSpeed < lfDesired)
        {
            // under speed -> accelerate. ramp = clamp((desired-speed)/KF_ATDS_UNDER, 0, 1).
            f32 lfRamp = (lfDesired - lfSpeed) / KF_ATDS_UNDER;
            lfRamp = (lfRamp < 0.0f) ? 0.0f : lfRamp;
            lfRamp = (lfRamp > 1.0f) ? 1.0f : lfRamp;
            mfAccelerator = lfRamp;                           // 0x1D28
        }
        else if (lfSpeed > lfDesired)
        {
            // over speed -> brake. ramp = clamp(((over)-LO)/(HI-LO), 0, 1).
            f32 lfRamp = ((lfSpeed - lfDesired) - KF_ATDS_OVER_LO) / (KF_ATDS_OVER_HI - KF_ATDS_OVER_LO);
            lfRamp = (lfRamp < 0.0f) ? 0.0f : lfRamp;
            lfRamp = (lfRamp > 1.0f) ? 1.0f : lfRamp;
            mfBrake = lfRamp;                                 // 0x1D2C
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

        f32 lfRamp = (lfCorneringAngle - KF_CORNER_ANGLE_BASE) / KF_CORNER_ANGLE_RANGE;
        lfRamp = (lfRamp < 0.0f) ? 0.0f : lfRamp;
        lfRamp = (lfRamp > 1.0f) ? 1.0f : lfRamp;

        const f32 lfScaled = lfInputSpeed * KF_CORNER_ANGLE_SCALE;
        // cap = inputSpeed + (scaled - inputSpeed) * ramp -- lerps from inputSpeed (ramp==0, no
        // cornering yet) toward the scaled-down speed (ramp==1, sharp corner).
        return lfInputSpeed + (lfScaled - lfInputSpeed) * lfRamp;
    }

    // ====================================================================================
    // ProximitySpeed @0x82770800
    //
    // Clamp the target speed by how close the car is to its proximity reference. t = clamp(
    // (proximityRef - 4.0) * 0.25, 0, 1). v = max( (currentSpeed - speedRef - KF_PROX_SPEED_FLOOR),
    // lfMinSpeed*0.5 ). Returns the lerp v + (lfMinSpeed - v) * t (drops toward lfMinSpeed as the
    // proximity ref grows; asm @0x827708C4/0x827708C8: vsubfp minSpeed-v, vmaddfp (minSpeed-v)*t+v).
    // ====================================================================================
    f32 AIDriver::ProximitySpeed(f32 lfMinSpeed)
    {
        // proximityRef @this+0x1A3C ; speedRef @this+0x1A40 (both in the embedded
        // RacingLine/avoidance block; de-inlined direct loads).
        const f32 lfProxRef = DriverFloatAtImpl(this, 0x1A3C);
        const f32 lfSpeedRef = DriverFloatAtImpl(this, 0x1A40);

        f32 lfT = (lfProxRef - 4.0f) * 0.25f;                 // flt_82003F40 == 0.25
        lfT = (lfT < 0.0f) ? 0.0f : lfT;
        lfT = (lfT > 1.0f) ? 1.0f : lfT;

        const f32 lfSpeed = mpCarHost->GetSpeed();
        f32 lfV = (lfSpeed - lfSpeedRef) - KF_PROX_SPEED_FLOOR;
        const f32 lfFloor = lfMinSpeed * 0.5f;
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

        const f32 lfScheduleGate = CarFloatAt(lpCar, 0x1518);                // car+0x1518 (5400)
        if (lfScheduleGate > 0.0f)
        {
            const f32 lfDistAhead = lpCar->mfDistanceAheadOfPlayer;          // car+0x14F8 (5368)
            f32 lfClamped = lfScheduleGate * 0.1f;                           // flt_820C424C == 0.1
            lfClamped = (lfClamped < 0.0f) ? 0.0f : lfClamped;
            lfClamped = (lfClamped > 1.0f) ? 1.0f : lfClamped;
            if (lfDistAhead > 0.0f)
            {
                if (lfDistAhead < lfClamped * 50.0f)                        // flt_820C4244 == 50.0
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
        (void)lpPlayerCar;
        AICar* lpCar = mpCarHost;

        const bool lbPlayer         = lpCar->IsPlayerCar();          // car+0x1549 (5449)
        const bool lbDrivenByPlayer = (CarFlagAt(lpCar, 0x154A) != 0); // car+0x154A (5450)
        const bool lbForceStandard  = lpCar->ForceStandardRoute();   // car+0x1548 (5448)
        const s32  liBehaviour      = lpCar->meBehaviour;            // car+0x14B4 (5300)

        if (lbPlayer && !lbDrivenByPlayer && (lbForceStandard || liBehaviour == 2))
        {
            Fan(this)->SetBiasMode(8);
            return;
        }

        if (liBehaviour == 1)
        {
            Fan(this)->SetBiasMode(4);
            return;
        }

        // The aggression-machine state @this+0x1C00 (result[1792]).
        const s32 liAggressionState = DriverS32AtImpl(this, 0x1C00);
        if (liAggressionState == 3 && !IsPlayerProtected())
        {
            Fan(this)->SetBiasMode(2);
            return;
        }

        if (liAggressionState == 13)
        {
            Fan(this)->SetBiasMode(9);
            return;
        }

        const s32 leStyle = static_cast<s32>(lpCar->GetRouteFindingStyle()); // car+0x14C0 (5312)
        const bool lbAggressive = (leStyle == 2 || leStyle == 6);
        const s32 leBias = lbAggressive ? ChooseAggressiveSteeringFan(lpCar)
                                        : ChooseRaceSteeringFan(lpCar);
        Fan(this)->SetBiasMode(leBias);
    }
}
