#pragma once

// BrnAI::AIDriver -- the per-car STEERING + CAR-CONTROL controller. This is the link that turns
// a navigation target (a heading + a desired speed) into the four physics inputs the car
// actually drives with: steering angle, accelerator, brake and hand-brake (plus the boost flag).
// One AIDriver shadows one AICar (mpCar); each frame Update refreshes the behaviour timers,
// chooses+biases a SteeringFan, gets a driving target, computes the steering angle through a PID
// controller and converts the (steerAngle, desiredSpeed-vs-currentSpeed) gap into throttle/brake.
//
// MINIMAL-SLICE HOME (NEW). The full AIDriver is large (it embeds NearbyVehicles, a SteeringFan,
// a RacingLine, a RacingLineGenerator, two PIDControllers and an AIAggression sub-machine, then a
// long scalar/flag tail). This header is NOT that full layout: it anchors ONLY the members the
// bodied functions touch at their X360-asm guest offsets, covers every untouched span with
// explicit padding, and pins each touched offset with a static_assert in a never-called
// _AssertLayout(). It mirrors the committed BrnAICar.h minimal-slice pattern.
//
// OFFSET AUTHORITY is the X360 pseudocode/asm (the DecFIGS BrnAIDriver.h DWARF gives the
// names/types/declaration-order but is the PS3 build and is NOT offset-authoritative). The anchor
// is mpCar @ this+0x1CE0 (7392) -- the AICar* loaded by `lwz r,0x1CE0(r31)` in every body. The
// scalar/flag tail (0x1CF8..0x1D6B) and the seven 16-byte Vector2 slots (0x1C70..0x1CD0) are laid
// out in DWARF declaration order; every offset is reconciled store-for-store against the asm.
//
// VISIBILITY: the touched members are public here so the de-inlined bodies can read/write them by
// name. The real class keeps them private behind inlined-away accessors; a named field
// read/write is the faithful de-inlined form of the X360's direct `*(driver+offset)` load/store.

#include <cstddef>            // offsetof (layout pinning)

#include "types.hpp"          // f32, s32, u8, u16, ...
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu, 16-byte SIMD)

namespace BrnAI
{
    struct AICar;   // pointer-only collaborator here (full minimal-slice home in BrnAICar.h)

    // DWARF BrnAIDriver.h:56 -- the AI's drift sub-state. meDriftState @ this+0x1CEC; the bodied
    // functions in this slice do not branch on it, but it pins the pointer-block layout.
    enum EDriftState
    {
        E_DRIFT_STATE_NORMAL_DRIVING = 0,
        E_DRIFT_STATE_START_DRIFT    = 1,
        E_DRIFT_STATE_DRIFTING       = 2,
        E_DRIFT_STATE_EXIT_DRIFTING  = 3,
        E_DRIFT_STATE_COUNT          = 4,
    };

    // DWARF BrnAIDriver.h:161. MINIMAL SLICE -- see file header.
    struct AIDriver
    {
        // ---- the 15 members bodied in BrnAIDriver.cpp (this UNIT) --------------------------
        void Construct();                                       // @0x82792B70
        void SetAICar(AICar* lpCar);                            // @0x827963C8
        void Prepare(/*AISectionsData*/ void* lpSectionsData,
                     s32   leRelatedActiveCarIndex,
                     /*Random*/ void* lpRandom);                // @0x82792CA8
        void Update(f32 lfTimeStep, bool lbActive, Vector3 lTargetMovement,
                    AICar* lpPlayerCar, bool lbValidPlayer,
                    /*Random*/ void* lpRandom);                 // @0x8279AEB0
        void UpdateBehaviour(f32 lfTimeStep, AICar* lpPlayerCar);   // @0x8279A680
        void UpdateSteeringAngle(f32 lfTargetAngle);            // @0x827708F0
        void CalculateSteeringAngle(f32 lfTimeStep);           // @0x8277CD18
        void CalculateCarControls(f32 lfTimeStep, Vector3 lTargetMovement,
                                  bool lbValidPlayer, /*Random*/ void* lpRandom); // @0x827998C0
        bool ComputeRouteDirection(Vector2& lrOutDirection);   // @0x82766500
        Vector2 GetTargetPosition();                           // @0x8277CBF8 (sret)
        void AttemptToDriveAtDesiredSpeed(f32 lfSpeedDelta);   // @0x827706D8
        f32  CorneringTopSpeed(f32 lfInputSpeed);              // @0x8277D0F0 -- returns the speed cap in fp1
        f32  ProximitySpeed(f32 lfMinSpeed);                   // @0x82770800
        s32  ChooseRaceSteeringFan(AICar* lpCar);              // @0x82766370  (EBiasMode)
        void SetDrivingFanBiases(AICar* lpPlayerCar);          // @0x82770428
        bool IsInvulnerable() const;                           // @0x82765740
        bool IsOnStartLine();                                  // @0x82765800 (DWARF BrnAIDriver.h:263 -- NON-const)

        // ---- AIDriver members the bodied functions CALL but that are bodied in their own recon
        // passes (declare-only; the per-TU cl /c gate needs only the declaration). Keeping these
        // here -- rather than as raw offset reaches -- is the faithful de-inlined form of the
        // X360's `bl BrnAI__AIDriver__<name>` calls.
        void InitialiseRacingLine();                            // @0x... (Update / SetAICar)
        void ResetAttribSysValues();                            // @0x... (SetAICar / Prepare)
        void GenerateRacingLine(f32 lfTimeStep);                // @0x... (Update)
        void UpdateBrakingAnticipationData();                   // @0x... (Update)
        void CalcDistanceFromPlayer(Vector3 lTargetMovement);   // @0x... (Update)
        void UpdatePlayerTimers(f32 lfTimeStep, AICar* lpPlayerCar); // @0x... (Update)
        void UpdateStuck(f32 lfTimeStep);                       // @0x... (UpdateBehaviour)
        void UpdateQuickTurn();                                 // @0x... (UpdateBehaviour case5)
        void DoSlowTurnBehaviour();                             // @0x... (UpdateBehaviour case6)
        void DoSlowTurn(f32 lfTimeStep);                        // @0x... (CalculateCarControls case6)
        void DoDrivingBehaviour(f32 lfTimeStep);                // @0x... (CalculateCarControls cases 1-4,10)
        void CalculateDesiredSpeed(Vector3 lTargetMovement, bool lbValidPlayer); // @0x... (CalculateCarControls)
        bool IsPlayerProtected();                               // @0x... (SetDrivingFanBiases)
        s32  ChooseAggressiveSteeringFan(AICar* lpCar);         // @0x... (SetDrivingFanBiases) (EBiasMode)
        bool CheckForBoosting();                                // @0x... (AttemptToDriveAtDesiredSpeed)

        // ---- storage (declaration order == layout order; pinned by _AssertLayout below) -----
        // [0x0000 .. 0x1C6F] mNearbyVehicles + mSteeringFan(@0x710) + mRacingLine(@0xF20) +
        // mRacingLineGenerator + mPIDController(@0x1B34) + mPIDControllerDrift(@0x1B98) +
        // mAggression(@0x1C00). Opaque here; the bodies reach the embedded SteeringFan /
        // PIDControllers / mAggression through TU-local collaborator pointers built from these
        // documented sub-object offsets in the .cpp (named calls, no raw field casts).
        u8 mPad0000[7280];                              // [0x0000 .. 0x1C6F]

        // The seven 2D vectors (16-byte SIMD slots) the steering math reads/writes.
        Vector2 mSteeringTargetVector;                  // +0x1C70 (7280) DWARF :550
        Vector2 mTargetRacingLinePos;                   // +0x1C80 (7296) DWARF :552
        Vector2 mTargetRoadDir;                         // +0x1C90 (7312) DWARF :553
        Vector2 mBrakingAnticipationPos;                // +0x1CA0 (7328) DWARF :554
        Vector2 mBrakingRoadDir;                        // +0x1CB0 (7344) DWARF :555
        Vector2 mFinalFacing;                           // +0x1CC0 (7360) DWARF :557
        Vector2 m2DCarPos;                              // +0x1CD0 (7376) DWARF :559

        // The pointer/enum block @ guest 0x1CE0..0x1CF7 (24 bytes):
        //   mpCar @+0x1CE0 (AICar*), mpSectionsData @+0x1CE4, mpAggressionVictim @+0x1CE8 (all
        //   GUEST 4-byte pointers), meDriftState @+0x1CEC, meAggressionVictim @+0x1CF0
        //   (EGlobalRaceCarIndex; -1 == none), meRelatedActiveCarIndex @+0x1CF4.
        // On the x64 GATE a real pointer is 8 bytes, which would shift every following offset, so
        // this block is represented as raw guest-width storage and accessed through the typed
        // accessors below (the de-inlined form of the X360's `lwz 0x1CE0(this)` etc.). This keeps
        // the scalar tail at its guest offsets and the static_asserts honest.
        u8 mPtrEnumBlock[0x18];                          // [0x1CE0 .. 0x1CF7]

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

        // ---- HOST-ONLY trailing members (NOT part of the guest layout) ----------------------
        // The guest pointers/enums that live inside mPtrEnumBlock are 4-byte on the X360 but
        // 8-byte on the x64 gate; to keep the guest offsets above exact, the usable typed values
        // are stored here (after the pinned guest tail) and reached through the accessors below.
        // This is purely a gate accommodation -- on the console these ARE the mPtrEnumBlock words.
        AICar* mpCarHost;                               // == guest mpCar @+0x1CE0
        s32    meAggressionVictimHost;                  // == guest meAggressionVictim @+0x1CF0
        s32    meRelatedActiveCarIndexHost;             // == guest meRelatedActiveCarIndex @+0x1CF4
        void*  mpSectionsDataHost;                      // == guest mpSectionsData @+0x1CE4
        AICar* mpAggressionVictimCarHost;               // == guest mpAggressionVictim @+0x1CE8

        // Typed accessors -- the de-inlined form of the X360 `*(this+0x1CE0)` etc.
        AICar* GetCar() const               { return mpCarHost; }
        void   SetCar(AICar* lpCar)         { mpCarHost = lpCar; }
        s32    GetAggressionVictim() const  { return meAggressionVictimHost; }
        void   SetAggressionVictim(s32 li)  { meAggressionVictimHost = li; }
        AICar* GetAggressionVictimCar() const     { return mpAggressionVictimCarHost; }
        void   SetAggressionVictimCar(AICar* lpCar) { mpAggressionVictimCarHost = lpCar; }

    private:
        // Never called: documents the offset contract. Every touched member is public, so the
        // static_asserts live at namespace scope below.
        static void _AssertLayout();
    };

    // Pin every touched offset -- the compile gate fails if the slice ever drifts.
    static_assert(offsetof(AIDriver, mSteeringTargetVector)        == 0x1C70, "AIDriver::mSteeringTargetVector @ +0x1C70");
    static_assert(offsetof(AIDriver, mTargetRacingLinePos)         == 0x1C80, "AIDriver::mTargetRacingLinePos @ +0x1C80");
    static_assert(offsetof(AIDriver, mTargetRoadDir)              == 0x1C90, "AIDriver::mTargetRoadDir @ +0x1C90");
    static_assert(offsetof(AIDriver, mBrakingAnticipationPos)     == 0x1CA0, "AIDriver::mBrakingAnticipationPos @ +0x1CA0");
    static_assert(offsetof(AIDriver, mBrakingRoadDir)            == 0x1CB0, "AIDriver::mBrakingRoadDir @ +0x1CB0");
    static_assert(offsetof(AIDriver, m2DCarPos)                  == 0x1CD0, "AIDriver::m2DCarPos @ +0x1CD0");
    // mPtrEnumBlock spans the guest pointer/enum region 0x1CE0..0x1CF7 (mpCar @0x1CE0,
    // mpSectionsData @0x1CE4, mpAggressionVictim @0x1CE8, meDriftState @0x1CEC, meAggressionVictim
    // @0x1CF0, meRelatedActiveCarIndex @0x1CF4). Its start + the resumed scalar tail are pinned.
    static_assert(offsetof(AIDriver, mPtrEnumBlock)             == 0x1CE0, "AIDriver::mPtrEnumBlock (guest mpCar) @ +0x1CE0");
    static_assert(offsetof(AIDriver, mQuickTurnSteeringLock)    == 0x1CF8, "AIDriver::mQuickTurnSteeringLock @ +0x1CF8");
    static_assert(offsetof(AIDriver, mfAngleToBrakingTarget)    == 0x1CFC, "AIDriver::mfAngleToBrakingTarget @ +0x1CFC");
    static_assert(offsetof(AIDriver, mfCarSpeed)               == 0x1D00, "AIDriver::mfCarSpeed @ +0x1D00");
    static_assert(offsetof(AIDriver, mfStuckTime)              == 0x1D04, "AIDriver::mfStuckTime @ +0x1D04");
    static_assert(offsetof(AIDriver, mfInvulnerableTime)      == 0x1D0C, "AIDriver::mfInvulnerableTime @ +0x1D0C");
    static_assert(offsetof(AIDriver, mfDesiredSpeed)          == 0x1D10, "AIDriver::mfDesiredSpeed @ +0x1D10");
    static_assert(offsetof(AIDriver, mfForcedSpeed)           == 0x1D18, "AIDriver::mfForcedSpeed @ +0x1D18");
    static_assert(offsetof(AIDriver, mfSteeringAngle)         == 0x1D24, "AIDriver::mfSteeringAngle @ +0x1D24");
    static_assert(offsetof(AIDriver, mfAccelerator)          == 0x1D28, "AIDriver::mfAccelerator @ +0x1D28");
    static_assert(offsetof(AIDriver, mfBrake)               == 0x1D2C, "AIDriver::mfBrake @ +0x1D2C");
    static_assert(offsetof(AIDriver, mfHandBrake)           == 0x1D30, "AIDriver::mfHandBrake @ +0x1D30");
    static_assert(offsetof(AIDriver, mfBoostTimeRemaining)  == 0x1D44, "AIDriver::mfBoostTimeRemaining @ +0x1D44");
    static_assert(offsetof(AIDriver, mfPerpendicularTarget) == 0x1D48, "AIDriver::mfPerpendicularTarget @ +0x1D48");
    static_assert(offsetof(AIDriver, mfCalculatedSteeringAngle) == 0x1D50, "AIDriver::mfCalculatedSteeringAngle @ +0x1D50");
    static_assert(offsetof(AIDriver, mfPIDOutput)           == 0x1D54, "AIDriver::mfPIDOutput @ +0x1D54");
    static_assert(offsetof(AIDriver, mfPlayerSlowSpeedTime) == 0x1D58, "AIDriver::mfPlayerSlowSpeedTime @ +0x1D58");
    static_assert(offsetof(AIDriver, mActiveRouteTimeStamp) == 0x1D5C, "AIDriver::mActiveRouteTimeStamp @ +0x1D5C");
    static_assert(offsetof(AIDriver, miCurrentRacingLineNodeIndex) == 0x1D60, "AIDriver::miCurrentRacingLineNodeIndex @ +0x1D60");
    static_assert(offsetof(AIDriver, mbWantToExitDrift)     == 0x1D64, "AIDriver::mbWantToExitDrift @ +0x1D64");
    static_assert(offsetof(AIDriver, mbWantToEnterDrift)    == 0x1D65, "AIDriver::mbWantToEnterDrift @ +0x1D65");
    static_assert(offsetof(AIDriver, mbUseForcedSpeed)      == 0x1D66, "AIDriver::mbUseForcedSpeed @ +0x1D66");
    static_assert(offsetof(AIDriver, mbIsRacingLineInitialised) == 0x1D68, "AIDriver::mbIsRacingLineInitialised @ +0x1D68");
    static_assert(offsetof(AIDriver, mbIsActive)            == 0x1D69, "AIDriver::mbIsActive @ +0x1D69");
    static_assert(offsetof(AIDriver, mbCurrentRouteComplete) == 0x1D6A, "AIDriver::mbCurrentRouteComplete @ +0x1D6A");
    static_assert(offsetof(AIDriver, mbBoosting)           == 0x1D6B, "AIDriver::mbBoosting @ +0x1D6B");
}
