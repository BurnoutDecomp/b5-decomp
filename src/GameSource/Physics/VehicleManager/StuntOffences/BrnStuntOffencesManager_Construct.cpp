#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.h"
#include <cstring>    // std::memset

// ==================================================================================================
// BrnPhysics::StuntOffencesManager::Construct (+ the ResetCompleteOutputValues it tail-calls)
// -- SPLIT OUT of BrnStuntOffencesManager.cpp on 2026-08-03 (task #116). BUILD-MECHANICS SPLIT
// ONLY: both bodies and their banner comments were MOVED verbatim, not retyped or re-derived.
//
// == WHY THE SPLIT ==
// VehicleManager::Construct @0x8263B7C8 calls StuntOffencesManager::Construct @0x825E8C08, and
// VehicleManager::Construct is now mounted (BrnVehicleManager_Construct.cpp) so that
// PhysicsModule::Construct could stop being a live empty stub. BrnStuntOffencesManager.cpp AS A
// WHOLE still cannot be mounted: its Update spine calls SEVEN RaceCarPhysics stunt accessors
// (GetDriftActiveTime / GetDriftLateralSpeed / IsHandbrakeHeld / IsConsideredAirborne /
// GetStuntReferenceVelocity / GetStuntWorldPosition / GetStuntForwardAxis -- RaceCarPhysics.h
// :268-274) that are declare-only with no body anywhere in the tree. /OPT:REF does not suppress
// LNK2019, so mounting the whole TU would drag all seven.
//
// These two bodies call NOTHING outside the class: only member stores and std::memset. Zero new
// closure.
//
// TO RE-MERGE: body the seven stunt accessors, mount BrnStuntOffencesManager.cpp, then move this
// text back and delete the TU.
// ==================================================================================================

namespace BrnPhysics
{
    // ============================================================================================
    // @0x825E8C08  Construct -- zero/seed all state.
    // ============================================================================================
    void StuntOffencesManager::Construct()
    {
        mfTimeInTheAirSoFar   = 0.0f;
        mvStuntRollInProgress = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mfLastAirTime         = 0.0f;
        muCurrentRaceCarState = 0;
        mfBearingLastFrame    = 0.0f;
        mbHandbreakTurnAttempting     = false;
        mbKeepCheckingForCleanLanding = false;
        mfHandBreakAngleSoFar = 0.0f;
        mbTookOffInReverse    = false;
        mfCleanLandingCheckTimeSoFar  = 0.0f;
        mfSuccesssfulLandingCheckTimeSoFar = -1.0f;   // flt_820037C8 == -1.0 (countdown disarmed)
        mbInAirLastFrame      = false;
        mfTimeDriftingLastFrame = 0.0f;
        mbKeepCheckingForSuccessfulLanding = false;
        mvCurrentInAirRotations = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mbWasDriftingLastFrame  = false;
        mvRaceCarPositionLastFrame = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        miConvoyCount = 0;
        mvTakeOffVector = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mvLandingVector = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        // seed the 3 parallel convoy arrays: distance = NaN (no link), timer/dist2 = 0
        for (s32 li = 0; li < 8; ++li)
        {
            maConvoyTimer[li]     = 0.0f;
            maConvoyDistance2[li] = 0.0f;
            std::memset(&maConvoyDistance[li], 0xFF, 4);   // 0xFFFFFFFF == NaN (asm `stw -1`)
        }
        ResetCompleteOutputValues();
        mfInProgressBarrelRollAngle    = 0.0f;
        mfInProgressAirSpinAngle       = 0.0f;
        muStuntActionInProgress        = 0;
        mfInProgressHandbreakTurnAngle = 0.0f;
        mfInProgressDriftTime          = 0.0f;
        mfInProgressDriftDistance      = 0.0f;
    }

    // ============================================================================================
    // @0x825C2D80  ResetCompleteOutputValues -- zero the completed-stunt output block.
    // ============================================================================================
    void StuntOffencesManager::ResetCompleteOutputValues()
    {
        mfCompletedBarrelRollAngle    = 0.0f;
        mbSuccessfulLanding           = false;   // +0xAE
        mfCompletedAirSpinAngle       = 0.0f;
        mbConvoyComplete              = false;   // +0x1A8
        mfCompletedHandbreakTurnAngle = 0.0f;
        miCompletedBarrelRolls        = 0;       // +0x19C
        mfCompletedDriftTime          = 0.0f;
        miCompletedAirSpinTurns       = 0;       // +0x1AC
        mfCompletedDriftDistance      = 0.0f;
        miCompletedConvoyCount        = 0;       // +0x184
        mfCompletedAir                = 0.0f;
        mfCompletedAirDistance        = 0.0f;
        // the 3 parallel completed-tailgate arrays + flag (loop base this+0x13C).
        for (s32 li = 0; li < 8; ++li)
        {
            maCompletedTailgateB[li] = 0.0f;
            std::memset(&maCompletedTailgateA[li], 0xFF, 4);   // NaN seed (asm stw -1)
            maCompletedTailgateC[li] = 0.0f;
            maCompletedTailgateFlag[li] = 0;
        }
        muStuntActionComplete = 0;
    }
}
