#pragma once

// BrnAI::AIDriver -- the per-car STEERING + CAR-CONTROL controller. This is the link that turns
// a navigation target (a heading + a desired speed) into the four physics inputs the car
// actually drives with: steering angle, accelerator, brake and hand-brake (plus the boost flag).
// One AIDriver shadows one AICar (mpCar); each frame Update refreshes the behaviour timers,
// chooses+biases a SteeringFan, gets a driving target, computes the steering angle through a PID
// controller and converts the (steerAngle, desiredSpeed-vs-currentSpeed) gap into throttle/brake.
//
// OUTPUT CONTRACT (what AIModule::ProcessAIVehicleInputs @0x82795E10 copies into the physics
// record BrnPhysics::Vehicle::BrnAIDriverControls, one per active race car, every frame):
//   mfAccelerator      -> BrnPlayerDriverControls::mfGas                (@0x1D28 -> record +0x04)
//   mfBrake            -> ::mfBrake                                     (@0x1D2C -> +0x08)
//   mfHandBrake        -> ::mfHandBrake                                 (@0x1D30 -> +0x0C)
//   mfSteeringAngle    -> ::mfSteering                                  (@0x1D24 -> +0x10)
//   mbBoosting         -> ::mbBoost                                     (@0x1D6B -> +0x3B)
//   IsInvulnerable()   -> ::mbIsInvulnerableToVehicles (OR'd with the module's player flag)
//   mbWantToEnterDrift -> ::mbForceDrift                                (@0x1D65 -> +0x3E)
//   IsOnStartLine()    -> ::mbIsOnStartLine                             (+0x40)
//   mfForcedSpeed      -> BrnAIDriverControls::mfSpeedMatchSpeed        (@0x1D18 -> +0x48)
//   mbUseForcedSpeed   -> ::mbDoSpeedMatch                              (@0x1D66 -> +0x4C)
//   mbWantToExitDrift  -> ::mbForceComeOutOfDrift                       (@0x1D64 -> +0x4D)
// (the record's mbSlamPlayer +0x4E is cleared; meDriverType +0x44 is set to E_DRIVER_TYPE_AI==1.)
// The DWARF getters GetAccelerator/GetSteering/GetBrake/GetHandBrake/GetBoost/... below are the
// named form of those field reads (the X360 inlines every one of them).
//
// LAYOUT HOME. The class is large: it embeds NearbyVehicles (@0x000), a SteeringFan (@0x710), a
// RacingLine (@0xF20), a RacingLineGenerator (@0x1B30, no data), two PIDControllers (@0x1B34 /
// @0x1B98) and an AIAggression sub-machine (@0x1C00), then the seven 2D vectors + the scalar/flag
// tail (@0x1C70 .. @0x1D6B, 7532 bytes total). OFFSET AUTHORITY is the X360 asm (`lwz 0x1CE0(r31)`
// == mpCar in every body); the DecFIGS BrnAIDriver.h DWARF gives names/types/order. The layout
// below is pinned member-by-member by the static_asserts at the bottom.
//
// Native storage uses the original embedded objects and 32-bit pointers.
//
// VISIBILITY: the touched members are public here so the de-inlined bodies can read/write them by
// name. The real class keeps them private behind inlined-away accessors.

#include <cstddef>            // offsetof (layout pinning)

#include "types.hpp"          // f32, s32, u8, u16, ...
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu, 16-byte SIMD)
#include "GameSource/BurnoutConstants.h"                              // EGlobalRaceCarIndex / EActiveRaceCarIndex
#include "GameSource/World/AI/BrnAISharedConstants.h"                // ENearbyType, ERoundRobinType, EAIBehaviour
#include "GameSource/World/AI/BrnAIBoundaryLine.h"                   // BoundaryLine (NearbyVehicle::maHNGLines)
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"         // SteeringFan (BY VALUE @0x710)
#include "GameSource/World/AI/Route/BrnRacingLine.h"                  // RacingLine (native embedded member)
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"   // RacingLineGenerator (BY VALUE @0x1B30)
#include "GameSource/World/AI/PID/BrnPIDController.h"                // PIDController (BY VALUE @0x1B34 / @0x1B98)
#include "GameSource/World/AI/BrnAIAggression.h"                     // AIAggression (native embedded member)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                // CgsNumeric::Random (Prepare draws one float)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (NearbyVehicles::GetCount)

#ifndef BRN_AI_RACINGLINE_STACK_PRESENT
#define BRN_AI_RACINGLINE_STACK_PRESENT 0
#endif
#ifndef BRN_AI_STEERINGFAN_TARGET_PRESENT
#define BRN_AI_STEERINGFAN_TARGET_PRESENT 0
#endif

// BrnTrafficAIInterfaces.h -- the avoidance feed's entity record (pointer-only here).
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficAIEntity; } }

namespace BrnAI
{
    struct AICar;            // pointer-only collaborator here (full minimal-slice home in BrnAICar.h)
    struct AISectionsData;   // SharedClasses/AI/AISectionsResourceType.h

    // DWARF BrnAIDriver.h:48 -- which car vector DetermineDriftSteeringAngle measures from.
    enum EDriftDirectionSelection
    {
        E_DRIFT_DIRECTION_SELECTION_CAR_MOVING = 0,
        E_DRIFT_DIRECTION_SELECTION_CAR_FACING = 1,
    };

    // DWARF BrnAIDriver.h:56 -- the AI's drift sub-state (meDriftState @ guest+0x1CEC;
    // DoDrivingBehaviour @0x82799660 switches on it).
    enum EDriftState
    {
        E_DRIFT_STATE_NORMAL_DRIVING = 0,
        E_DRIFT_STATE_START_DRIFT    = 1,
        E_DRIFT_STATE_DRIFTING       = 2,
        E_DRIFT_STATE_EXIT_DRIFTING  = 3,
        E_DRIFT_STATE_COUNT          = 4,
    };

    // DWARF BrnAIDriver.h:88 -- one entry of the driver's avoidance list. 0x70 bytes on both
    // console and host (pointer-free; the BoundaryLine array is 16-byte aligned).
    struct NearbyVehicle
    {
        static const s32 KI_TRAFFIC_NUM_HNG_LINES = 4;                  // :90

        Vector2             mVelocity;                                  // :92  +0x00
        Vector2             mCentre;                                    // :93  +0x10
        ENearbyType         mType;                                      // :94  +0x20
        EGlobalRaceCarIndex meGlobalRaceCarIndex;                       // :95  +0x24
        BoundaryLine        maHNGLines[KI_TRAFFIC_NUM_HNG_LINES];       // :96  +0x30
    };

    // DWARF BrnAIDriver.h:100 -- the avoidance list (16 slots + count). 0x710 bytes.
    // GetCount @0x82766748 / GetVehiclePointer @0x82766870 exist on the X360; Reset is the
    // `stw 0, 0x700(this)` AIDriver::Prepare @0x82792CA8 emits inline.
    struct NearbyVehicles
    {
        static const s32 KI_MAX_NEARBY_VEHICLES = 16;

        NearbyVehicle mVehicle[KI_MAX_NEARBY_VEHICLES];                 // :121 +0x000
        s32           miCount;                                          // :123 +0x700

        // :104 @0x82766748 -- the console body asserts before the load (BrnAIDriver.cpp:2912 /
        // :2913, `li r5, 0xB60` / `0xB61`); LineTestTrafficHNG inlines the same two asserts
        // (0x8277A910..0x8277A974). Restored 2026-09-23 (crash parity G07-D1).
        s32  GetCount() const
        {
            CGS_ASSERT(miCount >= 0, "miCount >= 0");
            CGS_ASSERT(miCount <= KI_MAX_NEARBY_VEHICLES, "miCount <= KI_MAX_NEARBY_TRAFFIC");
            return miCount;
        }
        void Reset()          { miCount = 0; }                          // :110 (inlined)

        // @0x827667D8 -- claim the next free slot. Both asserts are the console's
        // (BrnAIDriver.cpp:2921 / :2922); the store after them is unconditional.
        void Next();                                                    // :107

        // @0x82766870 -- &mVehicle[liEntry], bounds-asserted ("liEntry >= 0" /
        // "liEntry < KI_MAX_NEARBY_TRAFFIC").
        NearbyVehicle* GetVehiclePointer(s32 liEntry);                  // :113
    };

    // DWARF BrnAIDriver.h:161.
    struct AIDriver
    {
        // ---- bodied in BrnAIDriver.cpp ---------------------------------------------------
        void Construct();                                       // @0x82792B70
        void SetAICar(AICar* lpCar);                            // @0x827963C8
        void Prepare(AISectionsData* lpSectionsData,
                     s32   leRelatedActiveCarIndex,
                     CgsNumeric::Random* lpRandom);             // @0x82792CA8 (DWARF :173)
        // DWARF :182 Update(float32_t lfTimeStep, bool lbLineUpdateToken, Vector3 lPlayerCarPosition,
        //                   AICar* lpPlayerCar, bool lbDoInRangeCatchup, Random* lpRandom).
        // X360 register map (asm @0x8279AEB0 + the AIModule::UpdateDrivers call @0x8279B334):
        // r3=this, f1=dt, r4=SKIPPED (f1's slot), r5=token (never read), v1=player position,
        // r6=player car, r7=catch-up flag, r8=&AIModule::mRandom.
        void Update(f32 lfTimeStep, bool lbLineUpdateToken, Vector3 lPlayerCarPosition,
                    AICar* lpPlayerCar, bool lbDoInRangeCatchup,
                    CgsNumeric::Random* lpRandom);              // @0x8279AEB0
        void UpdateBehaviour(f32 lfTimeStep, AICar* lpPlayerCar);   // @0x8279A680
        void UpdateSteeringAngle(f32 lfTargetAngle);            // @0x827708F0
        void CalculateSteeringAngle(f32 lfTimeStep);           // @0x8277CD18
        void CalculateCarControls(f32 lfTimeStep, Vector3 lPlayerCarPosition,
                                  bool lbDoInRangeCatchup,
                                  CgsNumeric::Random* lpRandom);      // @0x827998C0 (DWARF :440)
        bool ComputeRouteDirection(Vector2& lrOutDirection);   // @0x82766500
        Vector2 GetTargetPosition();                           // @0x8277CBF8 (sret)
        void AttemptToDriveAtDesiredSpeed(f32 lfTimeStep);     // @0x827706D8 (DWARF :305)
        f32  CorneringTopSpeed(f32 lfInputSpeed);              // @0x8277D0F0 -- returns the speed cap in fp1
        f32  ProximitySpeed(f32 lfMinSpeed);                   // @0x82770800
        s32  ChooseRaceSteeringFan(AICar* lpCar);              // @0x82766370  (EBiasMode)
        void SetDrivingFanBiases(AICar* lpPlayerCar);          // @0x82770428
        bool IsInvulnerable() const;                           // @0x82765740
        bool IsOnStartLine();                                  // @0x82765800 (DWARF BrnAIDriver.h:263 -- NON-const)

        // ---- bodied in BrnAIDriver_Update.cpp (the Update-path callees) --------------------
        void UpdatePlayerTimers(f32 lfTimeStep, AICar* lpPlayerCar);   // @0x82770320 (DWARF :526)
        bool IsPlayerProtected(AICar* lpPlayerCar);             // @0x827660C8 (DWARF :309)
        s32  ChooseAggressiveSteeringFan(AICar* lpPlayerCar);   // @0x82766150 (DWARF :521) (EBiasMode)
        void UpdateStuck(f32 lfTimeStep);                       // @0x82766440 (DWARF :422)
        bool IsStuck();                                         // @0x82766670 (DWARF :260)
        bool CheckForBoosting();                                // @0x827705E0 (DWARF :312)
        bool CheckForSpeedMatch(f32 lfTimeStep);                // @0x82793020 (DWARF :454)
        void CalculateDesiredSpeed(Vector3 lPlayerCarPosition, bool lbDoInRangeCatchup); // @0x827934C0 (DWARF :451; NO IDA export -- image bytes)
        f32  HardShoulderSpeed(f32 lfInputSpeed);               // @0x827930B8 (DWARF :411)
        void DoDrivingBehaviour(f32 lfTimeStep);                // @0x82799660 (DWARF :484; NO IDA export -- image bytes)
        bool EstimateNeedForDrifting();                         // @0x82793438 (DWARF :469)
        f32  DetermineDriftSteeringAngle(EDriftDirectionSelection leSelection); // @0x827931D0 (DWARF :371)
        bool FindFinalDriftDirection(Vector2& lrOutDirection);  // @0x82793138 (DWARF :473)
        bool FindPositionInFuture(Vector2& lrOutPosition, Vector2& lrOutDirection,
                                  f32 lfTime, f32 lfMaxDistance, f32 lfMinDistance); // @0x82792F80 (DWARF :431)
        void StartDrift();                                      // @0x8277C870 (DWARF :295)
        void AttemptToDriveAtDesiredSpeedInDrift();             // @0x8277C8E0 (DWARF :301)
        void Determine180Turn();                                // @0x8277C758 (DWARF :339)
        f32  GetQuickTurnSteering(Vector2 lVectorToTarget);     // @0x8277C600 (DWARF :491)
        void UpdateQuickTurn();                                 // @0x8278B100 (DWARF :342)
        void DoSlowTurnBehaviour();                             // @0x8277C968 (DWARF :498)
        void DoSlowTurn(f32 lfTimeStep);                        // @0x8277CA88 (DWARF :495)
        void InitialiseRacingLine();                            // @0x82792DF0 (DWARF :389)
        void GenerateRacingLine(f32 lfTimeStep);                // @0x8277C4E8 (DWARF :375)
        void UpdateBrakingAnticipationData();                   // @0x827964C0 (DWARF :393; NO IDA export -- image bytes)
        void CalcDistanceFromPlayer(Vector3 lPlayerCarPosition); // @0x8276E1C8 (DWARF :380; NO IDA export -- image bytes)
        void ResetAttribSysValues();                            // @0x8278B2B0 (DWARF :321)
        void ResetPIDTuningState();                             // @0x8277DA48 (DWARF :324)
        s32  DoRoundRobinWork(ERoundRobinType leType);          // @0x82796340 (DWARF :327)

        // ---- the avoidance feed AIModule::SortTrafficIntoAICars @0x8278A970 drives ---------
        // Both take the candidate, pick a NearbyVehicles slot (the next free one, else the one
        // GetIndexOfFurthestVehicle nominates) and fill the record + its four HNG boundary
        // lines. Return true when the candidate was taken.
        bool AddNearbyTrafficToAvoidance(const BrnTraffic::BrnTrafficIO::TrafficAIEntity* lpEntity); // @0x8277D4F8 (DWARF :279)
        bool AddNearbyAIToAvoidance(const AICar* lpCar);        // @0x8277D6E0 (DWARF :283)
        s32  GetIndexOfFurthestVehicle(Vector2 lNewPosition);   // @0x8277D2E0 (DWARF :463)

        // ---- DWARF-declared accessors the X360 inlines away (the field reads AIModule::
        //      ProcessAIVehicleInputs @0x82795E10 performs directly) --------------------------
        bool IsActive() const          { return mbIsActive != 0; }           // :194
        f32  GetAccelerator() const    { return mfAccelerator; }             // :201
        f32  GetSteering() const       { return mfSteeringAngle; }           // :204
        f32  GetBrake() const          { return mfBrake; }                   // :207
        f32  GetHandBrake() const      { return mfHandBrake; }               // :210
        bool GetBoost() const          { return mbBoosting != 0; }           // :213
        bool WantToExitDrift() const   { return mbWantToExitDrift != 0; }    // :223
        bool WantToEnterDrift() const  { return mbWantToEnterDrift != 0; }   // :226
        bool UseForcedSpeed() const    { return mbUseForcedSpeed != 0; }     // :229
        f32  GetForcedSpeed() const    { return mfForcedSpeed; }             // :232
        f32  GetDesiredSpeed() const   { return mfDesiredSpeed; }            // :269
        bool HasCar() const            { return mpCar != 0; }            // :251
        void ResetStuckTime()          { mfStuckTime = 0.0f; }               // :266 (UpdateBehaviour stores it inline)
        s32  GetRelatedActiveCarIndex() const { return meRelatedActiveCarIndex; }   // :216
        void SetRelatedActiveCarIndex(s32 leIndex) { meRelatedActiveCarIndex = leIndex; } // :220
        f32  GetCalcualtedSteeringAngle() const { return mfCalculatedSteeringAngle; }   // :318 (sic)
        AIAggression*       GetAggression()       { return &mAggression; }          // :333
        const AIAggression* GetAggression() const { return &mAggression; }
        RacingLine&         GetRacingLine()       { return mRacingLine; }
        RacingLineGenerator& GetRacingLineGenerator() { return mRacingLineGenerator; }  // :315
        void SetPerpendicularTarget(f32 lfTarget) { mfPerpendicularTarget = lfTarget; } // :289
        f32  GetPerpendicularTarget() const       { return mfPerpendicularTarget; }     // :292
        const NearbyVehicles* GetNearbyVehicles() const { return &mNearbyVehicles; }    // :286
        void ClearNearbyVehicles()                { mNearbyVehicles.Reset(); }           // :275

        // DWARF :458 ClearAIControls -- the eight-store block Update / UpdateBehaviour /
        // CalculateCarControls emit inline (@0x8279B104..B128 etc.).
        void ClearAIControls()
        {
            mfAccelerator = 0.0f;  mbUseForcedSpeed   = 0;
            mfBrake       = 0.0f;  mbBoosting         = 0;
            mfForcedSpeed = 0.0f;  mbWantToExitDrift  = 0;
            mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
        }
        // DWARF :480 SetDriftState -- the `stw N, 0x1CEC(this)` stores.
        void        SetDriftState(EDriftState leState) { meDriftState = leState; }
        EDriftState GetDriftState() const              { return meDriftState; }

        // ---- storage (declaration order == layout order; pinned by the static_asserts) ------
        NearbyVehicles      mNearbyVehicles;            // +0x0000 DWARF :539 (16 x 0x70 + count)
        SteeringFan         mSteeringFan;               // +0x0710 DWARF :540 (0x810, pointer-free)
        RacingLine mRacingLine;                         // +0x0F20
        RacingLineGenerator mRacingLineGenerator;       // +0x1B30 DWARF :542 (no data; 4-byte slot)
        PIDController       mPIDController;             // +0x1B34 DWARF :544 (0x64, pointer-free)
        PIDController       mPIDControllerDrift;        // +0x1B98 DWARF :545 (0x64, pointer-free)
        AIAggression mAggression;                       // +0x1C00
        // The seven 2D vectors (16-byte SIMD slots) the steering math reads/writes.
        Vector2 mSteeringTargetVector;                  // +0x1C70 (7280) DWARF :550
        Vector2 mTargetRacingLinePos;                   // +0x1C80 (7296) DWARF :552
        Vector2 mTargetRoadDir;                         // +0x1C90 (7312) DWARF :553
        Vector2 mBrakingAnticipationPos;                // +0x1CA0 (7328) DWARF :554
        Vector2 mBrakingRoadDir;                        // +0x1CB0 (7344) DWARF :555
        Vector2 mFinalFacing;                           // +0x1CC0 (7360) DWARF :557
        Vector2 m2DCarPos;                              // +0x1CD0 (7376) DWARF :559

        AICar* mpCar;                                   // +0x1CE0
        AISectionsData* mpSectionsData;                 // +0x1CE4
        AICar* mpAggressionVictimCar;                   // +0x1CE8
        EDriftState meDriftState;                       // +0x1CEC
        s32 meAggressionVictim;                         // +0x1CF0
        s32 meRelatedActiveCarIndex;                    // +0x1CF4

        // The scalar tail (all f32 unless noted).
        f32 mQuickTurnSteeringLock;                     // +0x1CF8 (7416) DWARF :570
        f32 mfAngleToBrakingTarget;                     // +0x1CFC (7420) DWARF :571 -- CorneringTopSpeed writes the cornering angle here
        f32 mfCarSpeed;                                 // +0x1D00 (7424) DWARF :572
        f32 mfStuckTime;                                // +0x1D04 (7428) DWARF :574
        f32 mfDistanceToPlayer;                         // +0x1D08 (7432) DWARF :575
        f32 mfInvulnerableTime;                         // +0x1D0C (7436) DWARF :576
        f32 mfDesiredSpeed;                             // +0x1D10 (7440) DWARF :577 -- the target speed the controls chase
        f32 mfTopSpeed;                                 // +0x1D14 (7444) DWARF :578
        f32 mfForcedSpeed;                              // +0x1D18 (7448) DWARF :579
        f32 mfFinishLineStateTime;                      // +0x1D1C (7452) DWARF :580
        f32 mfStartLineWheelSpinTime;                   // +0x1D20 (7456) DWARF :581
        f32 mfSteeringAngle;                            // +0x1D24 (7460) DWARF :583 -- FINAL steering output (StepTo target)
        f32 mfAccelerator;                              // +0x1D28 (7464) DWARF :584 -- throttle output [0,1]
        f32 mfBrake;                                    // +0x1D2C (7468) DWARF :585 -- brake output [0,1]
        f32 mfHandBrake;                                // +0x1D30 (7472) DWARF :586 -- hand-brake output
        f32 mfPlayerTimeSinceCrash;                     // +0x1D34 (7476) DWARF :588
        f32 mfPlayerTimeSinceAIDriven;                  // +0x1D38 (7480) DWARF :589
        f32 mfTimeToLookAheadForDrift;                  // +0x1D3C (7484) DWARF :591
        f32 mfMinDistanceToLookAheadForDrift;           // +0x1D40 (7488) DWARF :592
        f32 mfBoostTimeRemaining;                       // +0x1D44 (7492) DWARF :594 -- AttemptToDriveAtDesiredSpeed counts it down
        f32 mfPerpendicularTarget;                      // +0x1D48 (7496) DWARF :596
        f32 mfAngleForDrift;                            // +0x1D4C (7500) DWARF :598
        f32 mfCalculatedSteeringAngle;                  // +0x1D50 (7504) DWARF :600 -- raw signed angle-to-target (pre-PID)
        f32 mfPIDOutput;                                // +0x1D54 (7508) DWARF :601 -- PID output, clamped [-1,1]
        f32 mfPlayerSlowSpeedTime;                      // +0x1D58 (7512) DWARF :603
        s32 mActiveRouteTimeStamp;                      // +0x1D5C (7516) DWARF :605
        s32 miCurrentRacingLineNodeIndex;               // +0x1D60 (7520) DWARF :607
        u8  mbWantToExitDrift;                          // +0x1D64 (7524) DWARF :610
        u8  mbWantToEnterDrift;                         // +0x1D65 (7525) DWARF :611
        u8  mbUseForcedSpeed;                           // +0x1D66 (7526) DWARF :612 -- drive at mfForcedSpeed
        u8  mbDriftingRequired;                         // +0x1D67 (7527) DWARF :613
        u8  mbIsRacingLineInitialised;                  // +0x1D68 (7528) DWARF :615 -- CalculateSteeringAngle gate
        u8  mbIsActive;                                 // +0x1D69 (7529) DWARF :616 -- Update top-level gate
        u8  mbCurrentRouteComplete;                     // +0x1D6A (7530) DWARF :617
        u8  mbBoosting;                                 // +0x1D6B (7531) DWARF :619 -- AttemptToDriveAtDesiredSpeed sets it

        // Typed accessors -- the de-inlined form of the X360 `*(this+0x1CE0)` etc.
        AICar* GetCar() const               { return mpCar; }
        void   SetCar(AICar* lpCar)         { mpCar = lpCar; }
        s32    GetAggressionVictim() const  { return meAggressionVictim; }
        void   SetAggressionVictim(s32 li)  { meAggressionVictim = li; }
        AICar* GetAggressionVictimCar() const     { return mpAggressionVictimCar; }
        void   SetAggressionVictimCar(AICar* lpCar) { mpAggressionVictimCar = lpCar; }

    private:
        // Never called: documents the offset contract. Every touched member is public, so the
        // static_asserts live at namespace scope below.
        static void _AssertLayout();
    };

    CGS_STATIC_ASSERT((sizeof(AIDriver) == 0x1D70), "AIDriver native stride");
    // Pin every guest offset -- the compile gate fails if the layout ever drifts.
    CGS_STATIC_ASSERT((sizeof(NearbyVehicle)  == 0x70),  "NearbyVehicle is 0x70 bytes (16 x 0x70 == 0x700)");
    CGS_STATIC_ASSERT((sizeof(NearbyVehicles) == 0x710), "NearbyVehicles is 0x710 bytes (AIDriver @0x000 .. @0x710)");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mSteeringFan)                 == 0x0710), "AIDriver::mSteeringFan @ +0x710");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mRacingLine)             == 0x0F20), "AIDriver::mRacingLine  @ +0xF20");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mRacingLineGenerator)         == 0x1B30), "AIDriver::mRacingLineGenerator @ +0x1B30");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mPIDController)               == 0x1B34), "AIDriver::mPIDController @ +0x1B34 (ResetPIDTuningState this+6964)");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mPIDControllerDrift)          == 0x1B98), "AIDriver::mPIDControllerDrift @ +0x1B98 (this+7064)");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mAggression)             == 0x1C00), "AIDriver::mAggression  @ +0x1C00");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mSteeringTargetVector)        == 0x1C70), "AIDriver::mSteeringTargetVector @ +0x1C70");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mTargetRacingLinePos)         == 0x1C80), "AIDriver::mTargetRacingLinePos @ +0x1C80");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mTargetRoadDir)              == 0x1C90), "AIDriver::mTargetRoadDir @ +0x1C90");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mBrakingAnticipationPos)     == 0x1CA0), "AIDriver::mBrakingAnticipationPos @ +0x1CA0");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mBrakingRoadDir)            == 0x1CB0), "AIDriver::mBrakingRoadDir @ +0x1CB0");
    CGS_STATIC_ASSERT((offsetof(AIDriver, m2DCarPos)                  == 0x1CD0), "AIDriver::m2DCarPos @ +0x1CD0");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mpCar)             == 0x1CE0), "AIDriver::mpCar (guest mpCar) @ +0x1CE0");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mpSectionsData) == 0x1CE4), "AIDriver::mpSectionsData native offset");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mpAggressionVictimCar) == 0x1CE8), "AIDriver::mpAggressionVictimCar native offset");
    CGS_STATIC_ASSERT((offsetof(AIDriver, meDriftState) == 0x1CEC), "AIDriver::meDriftState native offset");
    CGS_STATIC_ASSERT((offsetof(AIDriver, meAggressionVictim) == 0x1CF0), "AIDriver::meAggressionVictim native offset");
    CGS_STATIC_ASSERT((offsetof(AIDriver, meRelatedActiveCarIndex) == 0x1CF4), "AIDriver::meRelatedActiveCarIndex native offset");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mQuickTurnSteeringLock)    == 0x1CF8), "AIDriver::mQuickTurnSteeringLock @ +0x1CF8");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfAngleToBrakingTarget)    == 0x1CFC), "AIDriver::mfAngleToBrakingTarget @ +0x1CFC");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfCarSpeed)               == 0x1D00), "AIDriver::mfCarSpeed @ +0x1D00");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfStuckTime)              == 0x1D04), "AIDriver::mfStuckTime @ +0x1D04");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfInvulnerableTime)      == 0x1D0C), "AIDriver::mfInvulnerableTime @ +0x1D0C");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfDesiredSpeed)          == 0x1D10), "AIDriver::mfDesiredSpeed @ +0x1D10");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfTopSpeed)              == 0x1D14), "AIDriver::mfTopSpeed @ +0x1D14");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfForcedSpeed)           == 0x1D18), "AIDriver::mfForcedSpeed @ +0x1D18");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfSteeringAngle)         == 0x1D24), "AIDriver::mfSteeringAngle @ +0x1D24");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfAccelerator)          == 0x1D28), "AIDriver::mfAccelerator @ +0x1D28");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfBrake)               == 0x1D2C), "AIDriver::mfBrake @ +0x1D2C");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfHandBrake)           == 0x1D30), "AIDriver::mfHandBrake @ +0x1D30");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfTimeToLookAheadForDrift)      == 0x1D3C), "AIDriver::mfTimeToLookAheadForDrift @ +0x1D3C");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfMinDistanceToLookAheadForDrift) == 0x1D40), "AIDriver::mfMinDistanceToLookAheadForDrift @ +0x1D40");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfBoostTimeRemaining)  == 0x1D44), "AIDriver::mfBoostTimeRemaining @ +0x1D44");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfPerpendicularTarget) == 0x1D48), "AIDriver::mfPerpendicularTarget @ +0x1D48");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfAngleForDrift)       == 0x1D4C), "AIDriver::mfAngleForDrift @ +0x1D4C");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfCalculatedSteeringAngle) == 0x1D50), "AIDriver::mfCalculatedSteeringAngle @ +0x1D50");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfPIDOutput)           == 0x1D54), "AIDriver::mfPIDOutput @ +0x1D54");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mfPlayerSlowSpeedTime) == 0x1D58), "AIDriver::mfPlayerSlowSpeedTime @ +0x1D58");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mActiveRouteTimeStamp) == 0x1D5C), "AIDriver::mActiveRouteTimeStamp @ +0x1D5C");
    CGS_STATIC_ASSERT((offsetof(AIDriver, miCurrentRacingLineNodeIndex) == 0x1D60), "AIDriver::miCurrentRacingLineNodeIndex @ +0x1D60");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbWantToExitDrift)     == 0x1D64), "AIDriver::mbWantToExitDrift @ +0x1D64");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbWantToEnterDrift)    == 0x1D65), "AIDriver::mbWantToEnterDrift @ +0x1D65");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbUseForcedSpeed)      == 0x1D66), "AIDriver::mbUseForcedSpeed @ +0x1D66");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbDriftingRequired)    == 0x1D67), "AIDriver::mbDriftingRequired @ +0x1D67");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbIsRacingLineInitialised) == 0x1D68), "AIDriver::mbIsRacingLineInitialised @ +0x1D68");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbIsActive)            == 0x1D69), "AIDriver::mbIsActive @ +0x1D69");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbCurrentRouteComplete) == 0x1D6A), "AIDriver::mbCurrentRouteComplete @ +0x1D6A");
    CGS_STATIC_ASSERT((offsetof(AIDriver, mbBoosting)           == 0x1D6B), "AIDriver::mbBoosting @ +0x1D6B");
}
