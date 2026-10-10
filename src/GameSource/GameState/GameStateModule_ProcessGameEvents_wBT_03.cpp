// BrnGameState::GameStateModule::ProcessGameEvents_Group3 -- the driving, crash and stunt arms of the
// game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes these ids here).
//
// Each case is one arm of the console's single switch, in the order the arms sit in the function:
// the world's driving events (near misses, drifts, spins, air, oncoming, tailgating, traffic checks)
// become the boost-ticker actions and feed ModeManager::ProcessEvent; the crash-mode events drive
// the crash scorer and post their crash actions; vehicle impacts reach the scoring system and the
// rumble; completed and in-progress stunts post actions 15 / 16, feed the developer challenges,
// achievements and training tips and keep the profile's best stunt stats; prop hits go to the stunt
// manager, prop-progression requests arm the dispatcher's tail, and training requests reach the
// training manager.
//
// lpActionQueue is the dispatcher's output action queue, lpOutput its output buffer. ProcessEvent's
// float is the module's cached sim time step. Payloads are read through their event structs
// (BrnGameEvents.h).

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                       // IsShowtimeGameMode
#include "GameSource/GameState/BrnGameEvents.h"                              // the event payloads
#include "GameSource/GameState/BrnGameActions.h"                             // the action records
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                 // ModeManager::ProcessEvent
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"          // GameMode::OnPlayerInShortCut / HandleGameEvents
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"       // ScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // CrashModeScoring
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"             // RumbleManager
#include "GameSource/GameState/Offences/BrnStuntManager.h"                   // StuntManager
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"         // TrainingManager::RequestTraining
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // DeveloperChallengeManager
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"              // CarSelectManager::IsInJunkyard
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"        // OnlineCarSelectManager::IsInOnlineCarSelect
#include "GameSource/GameState/Progression/BrnProgressionManager.h"          // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                     // Profile
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"           // BrnPhysics::Vehicle::EImpactType
#include "SharedClasses/Progression/BrnTrainingTypes.h"                      // BrnProgression::ETrainingType

namespace BrnGameState
{

namespace
{
    // Radians to degrees for the completed flat-spin angle (case 119).
    const f32 KF_RADIANS_TO_DEGREES = 57.29578f;
    // The flat spin, in degrees, a completed stunt must beat before the flat-spin tip is offered.
    const f32 KF_FLAT_SPIN_TRAINING_MIN_DEGREES = 90.0f;
}

void GameStateModule::ProcessGameEvents_Group3(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    (void)lpPreWorldInput;   // no group-3 arm reads the pre-world input

    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_RACE_CAR_NEEDS_HIDING:
        mModeManager.GetCurrentGameMode()->HandleGameEvents(lpEvent, GameStateModuleIO::E_EVENT_RACE_CAR_NEEDS_HIDING);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_IN_SHORT_CUT:
        mModeManager.GetCurrentGameMode()->OnPlayerInShortCut();
        break;

    case GameStateModuleIO::E_EVENT_RECORD_PROP_HIT:
    {
        const GameStateModuleIO::RecordPropHitEvent* lpPropHitEvent =
            reinterpret_cast<const GameStateModuleIO::RecordPropHitEvent*>(lpEvent);
        mStuntManager.OnPropHit(lpPropHitEvent->muZoneId, lpPropHitEvent->muPropId, lpPropHitEvent->mPosition);
        break;
    }

    case GameStateModuleIO::E_EVENT_OVERHEAD_SIGN_HIT:
        // Only showtime scores overhead signs.
        if (GameStateModuleIO::IsShowtimeGameMode(mModeManager.GetCurrentGameModeType()))
        {
            const GameStateModuleIO::HitOverheadSignEvent* lpSignEvent =
                reinterpret_cast<const GameStateModuleIO::HitOverheadSignEvent*>(lpEvent);
            mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithHitOverheadSign();

            GameStateModuleIO::OverheadSignHitAction lSignAction = {};
            lSignAction.miPointsAwarded = KI_SCORE_BONUS_PER_OVERHEAD_SIGN;
            lSignAction.mu8SignFlags    = lpSignEvent->muRaceCarId;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignAction),
                                    GameStateModuleIO::E_ACTION_OVERHEAD_SIGN_HIT,
                                    static_cast<s32>(sizeof(lSignAction)));
        }
        break;

    case GameStateModuleIO::E_EVENT_REQUEST_PROP_PROGRESSION:
        // The dispatcher's tail posts the profile's hit props once the walk is done.
        mbPropSystemNeedsProgression = true;
        break;

    case GameStateModuleIO::E_EVENT_GAME_TRAINING_REQUEST:
    {
        const GameStateModuleIO::RequestGameTrainingEvent* lpTrainingEvent =
            reinterpret_cast<const GameStateModuleIO::RequestGameTrainingEvent*>(lpEvent);
        mpTrainingManager->RequestTraining(static_cast<BrnProgression::ETrainingType>(lpTrainingEvent->meTrainingType));
        break;
    }

    case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:
    {
        // Not while the junkyard or the online car select owns the player car.
        if (mCarSelectManager.IsInJunkyard() || mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            break;
        }

        const GameStateModuleIO::CompletedStuntEvent* lpCompletedStuntEvent =
            reinterpret_cast<const GameStateModuleIO::CompletedStuntEvent*>(lpEvent);
        CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");

        GameStateModuleIO::CompletedStuntAction lStuntAction = {};
        lStuntAction.muStuntActionComplete         = lpCompletedStuntEvent->muStuntActionComplete;
        lStuntAction.mfCompletedBarrelRollAngle    = lpCompletedStuntEvent->mfCompletedBarrelRollAngle;
        lStuntAction.mfCompletedAirSpinAngle       = lpCompletedStuntEvent->mfCompletedAirSpinAngle;
        lStuntAction.mfCompletedHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
        lStuntAction.mfCompletedDriftTime          = lpCompletedStuntEvent->mfCompletedDriftTime;
        lStuntAction.mfCompletedDriftDistance      = lpCompletedStuntEvent->mfCompletedDriftDistance;
        lStuntAction.miCompletedBarrelRolls        = lpCompletedStuntEvent->miCompletedBarrelRolls;
        lStuntAction.mbSuccessfulLanding           = lpCompletedStuntEvent->mbSuccessfulLanding;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStuntAction),
                                GameStateModuleIO::E_ACTION_COMPLETED_STUNT,
                                static_cast<s32>(sizeof(lStuntAction)));

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_COMPLETED_STUNT, lpEvent, mfSimTimeStep);
        mDeveloperChallengeManager.OnStuntOffenceComplete(reinterpret_cast<const u8*>(lpEvent));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");

        const u32 luStuntActionComplete = lpCompletedStuntEvent->muStuntActionComplete;
        s32       liCompletedBarrelRolls = 0;
        f32       lfFlatSpinDegrees      = 0.0f;
        f32       lfHandbreakTurnAngle   = 0.0f;
        f32       lfDriftDistance        = 0.0f;

        if ((luStuntActionComplete & 0x1u) == 0x1u)
        {
            liCompletedBarrelRolls = lpCompletedStuntEvent->miCompletedBarrelRolls;
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_BARREL_ROLL);
            mAchievementManager.OnBarrelRoll(liCompletedBarrelRolls);
            mModeManager.GetScoringSystem()->PlayerPerformedBarrelRolls(GetPlayerActiveRaceCarIndex(),
                                                                        liCompletedBarrelRolls);
        }
        if ((luStuntActionComplete & 0x2u) == 0x2u)
        {
            lfFlatSpinDegrees = lpCompletedStuntEvent->mfCompletedAirSpinAngle * KF_RADIANS_TO_DEGREES;
            mAchievementManager.OnFlatSpin(lfFlatSpinDegrees);
            mDeveloperChallengeManager.OnFlatSpin(lfFlatSpinDegrees);
            if (lfFlatSpinDegrees > KF_FLAT_SPIN_TRAINING_MIN_DEGREES)
            {
                mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_FLAT_SPIN);
            }
        }
        if ((luStuntActionComplete & 0x4u) == 0x4u)
        {
            mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_USES_E_BRAKE);
            lfHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
        }
        if ((luStuntActionComplete & 0x40u) == 0x40u)
        {
            lfDriftDistance = lpCompletedStuntEvent->mfCompletedDriftDistance;
        }
        lpProfile->SetBestStuntStats(liCompletedBarrelRolls, lfFlatSpinDegrees, lfHandbreakTurnAngle, lfDriftDistance);
        break;
    }

    case GameStateModuleIO::E_EVENT_INPROGRESS_STUNT:
    {
        // Not while the junkyard or the online car select owns the player car.
        if (mCarSelectManager.IsInJunkyard() || mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            break;
        }

        const GameStateModuleIO::InProgressStuntEvent* lpInProgressStuntEvent =
            reinterpret_cast<const GameStateModuleIO::InProgressStuntEvent*>(lpEvent);
        CGS_ASSERT(lpInProgressStuntEvent, "lpInProgressStuntEvent");

        // The console adds the player's leg distance to the record's mfConvoyLegDistance without
        // writing that word first, so its sum starts from whatever the stack slot held. The record is
        // zero-initialised here, so the sum starts from 0.
        GameStateModuleIO::InProgressStuntAction lStuntAction = {};
        lStuntAction.muStuntActionInProgress        = lpInProgressStuntEvent->muStuntActionInProgress;
        lStuntAction.mfInProgressBarrelRollAngle    = lpInProgressStuntEvent->mfInProgressBarrelRollAngle;
        lStuntAction.mfInProgressAirSpinAngle       = lpInProgressStuntEvent->mfInProgressAirSpinAngle;
        lStuntAction.mfInProgressHandbreakTurnAngle = lpInProgressStuntEvent->mfInProgressHandbreakTurnAngle;
        lStuntAction.mfInProgressDriftTime          = lpInProgressStuntEvent->mfInProgressDriftTime;
        lStuntAction.mfInProgressDriftDistance      = lpInProgressStuntEvent->mfInProgressDriftDistance;

        // In a convoy of two or more: find the player, report its leg distance and the car ahead.
        if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x80u) == 0x80u &&
            lpInProgressStuntEvent->miConvoyMemberCount > 1)
        {
            const EActiveRaceCarIndex lePlayerIndex = GetPlayerActiveRaceCarIndex();
            s32 liConvoyIndex = 0;
            for (; liConvoyIndex < lpInProgressStuntEvent->miConvoyMemberCount; ++liConvoyIndex)
            {
                if (lpInProgressStuntEvent->maConvoyMemberARCIs[liConvoyIndex] == lePlayerIndex)
                {
                    lStuntAction.mfConvoyLegDistance += lpInProgressStuntEvent->maConvoyLegDistances[liConvoyIndex];
                    lStuntAction.miCarInFrontIndex =
                        (liConvoyIndex > 0) ? lpInProgressStuntEvent->maConvoyMemberARCIs[liConvoyIndex - 1] : -1;
                    break;
                }
            }
            CGS_ASSERT(liConvoyIndex < lpInProgressStuntEvent->miConvoyMemberCount,
                       "Player supposed to be in convoy, but not found in CarIndices");
        }
        else
        {
            lStuntAction.mfConvoyLegDistance = 0.0f;
            lStuntAction.miCarInFrontIndex   = -1;
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStuntAction),
                                GameStateModuleIO::E_ACTION_INPROGRESS_STUNT,
                                static_cast<s32>(sizeof(lStuntAction)));
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_INPROGRESS_STUNT, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_VEHICLE_IMPACT:
    {
        const GameStateModuleIO::VehicleImpactEvent* lpImpactEvent =
            reinterpret_cast<const GameStateModuleIO::VehicleImpactEvent*>(lpEvent);
        SendVehicleImpactMessages(lpImpactEvent, lpActionQueue);

        const BrnPhysics::Vehicle::EImpactType leImpactType =
            static_cast<BrnPhysics::Vehicle::EImpactType>(lpImpactEvent->meImpactType);
        if (lpImpactEvent->meAggressorActiveRaceCarIndex == GetPlayerActiveRaceCarIndex())
        {
            mModeManager.GetScoringSystem()->OnPlayerHitsRival(leImpactType);
            mRumbleManager.OnVehicleAggressorImpact(leImpactType);
        }
        if (lpImpactEvent->meVictimActiveRaceCarIndex == GetPlayerActiveRaceCarIndex())
        {
            mRumbleManager.OnVehicleVictimImpact(leImpactType);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING:
    {
        const GameStateModuleIO::TrafficCheckingEvent* lpVcEvent =
            reinterpret_cast<const GameStateModuleIO::TrafficCheckingEvent*>(lpEvent);
        CGS_ASSERT(lpVcEvent != 0, "lpVcEvent != NULL");

        GameStateModuleIO::TrafficCheckingAction lCheckingAction;
        lCheckingAction.muVehicleIndex = lpVcEvent->muVehicleIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCheckingAction),
                                GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING,
                                static_cast<s32>(sizeof(lCheckingAction)));
        mRumbleManager.OnTrafficCheck();
        break;
    }

    case GameStateModuleIO::E_EVENT_CRASH_COMBO_ITEM:
    {
        CrashModeScoring* lpCrashScorer = mModeManager.GetScoringSystem()->GetCrashScorer();
        CGS_ASSERT(lpCrashScorer != 0, "lpCrashScorer != NULL");

        const GameStateModuleIO::CrashComboItemEvent* lpComboItemEvent =
            reinterpret_cast<const GameStateModuleIO::CrashComboItemEvent*>(lpEvent);
        GameStateModuleIO::CrashComboAction lComboAction;
        lComboAction.meEntryType = lpComboItemEvent->meEntryType;
        lComboAction.mfValue     = lpComboItemEvent->mfValue;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lComboAction),
                                GameStateModuleIO::E_ACTION_CRASH_COMBO,
                                static_cast<s32>(sizeof(lComboAction)));
        lpCrashScorer->DealWithComboItem(lpComboItemEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_NEAR_MISS_SCORED:
    {
        const GameStateModuleIO::NearMissScoredEvent* lpNearMissEvent =
            reinterpret_cast<const GameStateModuleIO::NearMissScoredEvent*>(lpEvent);
        GameStateModuleIO::NearMissAction lNearMissAction;
        lNearMissAction.miCount        = lpNearMissEvent->miCount;
        lNearMissAction.meNearMissType = lpNearMissEvent->meNearMissType;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNearMissAction),
                                GameStateModuleIO::E_ACTION_NEAR_MISS,
                                static_cast<s32>(sizeof(lNearMissAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_NEAR_MISS:
    case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:
        mModeManager.ProcessEvent(static_cast<GameStateModuleIO::EGameEventType>(liEventType), lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_DRIFTING:
    {
        const GameStateModuleIO::DriftingEvent* lpDriftingEvent =
            reinterpret_cast<const GameStateModuleIO::DriftingEvent*>(lpEvent);
        GameStateModuleIO::DriftingAction lDriftingAction;
        lDriftingAction.mfDistance = lpDriftingEvent->mfDistance;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDriftingAction),
                                GameStateModuleIO::E_ACTION_DRIFTING,
                                static_cast<s32>(sizeof(lDriftingAction)));
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_DRIFTING, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_SPINNING:
    {
        const GameStateModuleIO::SpinningEvent* lpSpinningEvent =
            reinterpret_cast<const GameStateModuleIO::SpinningEvent*>(lpEvent);
        GameStateModuleIO::SpinningAction lSpinningAction;
        lSpinningAction.mfSpinAngle = lpSpinningEvent->mfSpinAngle;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSpinningAction),
                                GameStateModuleIO::E_ACTION_SPINNING,
                                static_cast<s32>(sizeof(lSpinningAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_IN_AIR:
    {
        const GameStateModuleIO::InAirEvent* lpInAirEvent =
            reinterpret_cast<const GameStateModuleIO::InAirEvent*>(lpEvent);
        CGS_ASSERT(lpInAirEvent, "lpInAirEvent");

        GameStateModuleIO::InAirAction lInAirAction;
        lInAirAction.mfCumulativeAirTime  = lpInAirEvent->mfCumulativeAirTime;
        lInAirAction.mfCurrentJumpAirTime = lpInAirEvent->mfCurrentJumpAirTime;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInAirAction),
                                GameStateModuleIO::E_ACTION_IN_AIR,
                                static_cast<s32>(sizeof(lInAirAction)));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");
        lpProfile->SetNewAirMaximum(lpInAirEvent->mfCurrentJumpAirTime);

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_IN_AIR, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONCOMING:
    {
        const GameStateModuleIO::OncomingEvent* lpOncomingEvent =
            reinterpret_cast<const GameStateModuleIO::OncomingEvent*>(lpEvent);
        GameStateModuleIO::OncomingAction lOncomingAction;
        lOncomingAction.mfDistance = lpOncomingEvent->mfDistance;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lOncomingAction),
                                GameStateModuleIO::E_ACTION_ONCOMING,
                                static_cast<s32>(sizeof(lOncomingAction)));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");
        lpProfile->SetNewOncomingMaximum(lOncomingAction.mfDistance);

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ONCOMING, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED, lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_TAILGATING:
    {
        const GameStateModuleIO::TailgatingEvent* lpTailgatingEvent =
            reinterpret_cast<const GameStateModuleIO::TailgatingEvent*>(lpEvent);
        GameStateModuleIO::TailgatingAction lTailgatingAction;
        lTailgatingAction.mfDistance          = lpTailgatingEvent->mfDistance;
        lTailgatingAction.meTailgatedCarIndex = static_cast< ::EActiveRaceCarIndex>(lpTailgatingEvent->meTailgatedCarIndex);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTailgatingAction),
                                GameStateModuleIO::E_ACTION_TAILGATING,
                                static_cast<s32>(sizeof(lTailgatingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_RESET_ON_TRACK:
    {
        const GameStateModuleIO::PlayerResetOnTrackAction lResetAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                GameStateModuleIO::E_ACTION_PLAYER_RESET_ON_TRACK,
                                static_cast<s32>(sizeof(lResetAction)));
        mStuntManager.ClearActiveJump();
        break;
    }

    case GameStateModuleIO::E_EVENT_TRIGGER_CRASH_BREAKER:
    {
        const GameStateModuleIO::TriggerCrashBreakerEvent* lpCrashBreakerEvent =
            reinterpret_cast<const GameStateModuleIO::TriggerCrashBreakerEvent*>(lpEvent);
        GameStateModuleIO::TriggerCrashBreakerAction lCrashBreakerAction;
        lCrashBreakerAction.mPosition        = lpCrashBreakerEvent->mPosition;
        lCrashBreakerAction.meRaceCarIndex   = lpCrashBreakerEvent->meRaceCarIndex;
        lCrashBreakerAction.mfNormMagnitude  = lpCrashBreakerEvent->mfNormMagnitude;
        lCrashBreakerAction.mfTimeUntilStart = lpCrashBreakerEvent->mfTimeUntilStart;
        lCrashBreakerAction.mfDurationTime   = lpCrashBreakerEvent->mfDurationTime;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCrashBreakerAction),
                                GameStateModuleIO::E_ACTION_TRIGGER_CRASH_BREAKER,
                                static_cast<s32>(sizeof(lCrashBreakerAction)));
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithCrashbreakerRequest(lpCrashBreakerEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_CANCEL_CRASH_BREAKER:
    {
        const GameStateModuleIO::CancelCrashBreakerEvent* lpCancelEvent =
            reinterpret_cast<const GameStateModuleIO::CancelCrashBreakerEvent*>(lpEvent);
        GameStateModuleIO::CancelCrashBreakerAction lCancelAction;
        lCancelAction.meRaceCarIndex = lpCancelEvent->meRaceCarIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCancelAction),
                                GameStateModuleIO::E_ACTION_CANCEL_CRASH_BREAKER,
                                static_cast<s32>(sizeof(lCancelAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PICKUP:
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithPickup(
            reinterpret_cast<const GameStateModuleIO::PickupEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_VEHICLE_LEAPT:
    {
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithVehicleLeaping(
            reinterpret_cast<const GameStateModuleIO::VehicleLeaptEvent*>(lpEvent));

        const GameStateModuleIO::VehicleLeaptAction lLeaptAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLeaptAction),
                                GameStateModuleIO::E_ACTION_VEHICLE_LEAPT,
                                static_cast<s32>(sizeof(lLeaptAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ENTER_NEW_ROAD:
    {
        const GameStateModuleIO::EnterNewRoadEvent* lpNewRoadEvent =
            reinterpret_cast<const GameStateModuleIO::EnterNewRoadEvent*>(lpEvent);
        GameStateModuleIO::EnterNewRoadAction lNewRoadAction;
        lNewRoadAction.mbIsJunction = lpNewRoadEvent->mbIsJunction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNewRoadAction),
                                GameStateModuleIO::E_ACTION_ENTER_NEW_ROAD,
                                static_cast<s32>(sizeof(lNewRoadAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:
    {
        const GameStateModuleIO::PowerParkResultEvent* lpPowerParkEvent =
            reinterpret_cast<const GameStateModuleIO::PowerParkResultEvent*>(lpEvent);
        if (lpPowerParkEvent->meOutcome == BrnWorld::E_PPO_SUCCESS)
        {
            CGS_ASSERT(IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0,
                       "IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0");
            mProgressionManager.OnPowerParkResult(lpPowerParkEvent->miOverallRating,
                                                  lpPowerParkEvent->miOtherPlayersInvolved >= 2);
            mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_POWER_PARK_RESULT, lpEvent, mfSimTimeStep);
            mAchievementManager.OnPowerParking(lpPowerParkEvent->miOverallRating);
        }

        GameStateModuleIO::PowerParkResultAction lPowerParkAction;
        lPowerParkAction.meOutcome       = static_cast<BrnWorld::EPowerParkOutcome>(lpPowerParkEvent->meOutcome);
        lPowerParkAction.miOverallRating = lpPowerParkEvent->miOverallRating;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPowerParkAction),
                                GameStateModuleIO::E_ACTION_POWER_PARK_RESULT,
                                static_cast<s32>(sizeof(lPowerParkAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_AFTERTOUCH:
    {
        const GameStateModuleIO::AftertouchEvent* lpAftertouchEvent =
            reinterpret_cast<const GameStateModuleIO::AftertouchEvent*>(lpEvent);
        GameStateModuleIO::AftertouchAction lAftertouchAction;
        lAftertouchAction.meRaceCarIndex       = lpAftertouchEvent->meRaceCarIndex;
        lAftertouchAction.mfForwardAftertouch  = lpAftertouchEvent->mfForwardAftertouch;
        lAftertouchAction.mfSidewaysAftertouch = lpAftertouchEvent->mfSidewaysAftertouch;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAftertouchAction),
                                GameStateModuleIO::E_ACTION_AFTERTOUCH,
                                static_cast<s32>(sizeof(lAftertouchAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_RACE_CAR_DRIVING_IN_CRASH:
    {
        const GameStateModuleIO::RaceCarDrivingInCrashEvent* lpDrivingInCrashEvent =
            reinterpret_cast<const GameStateModuleIO::RaceCarDrivingInCrashEvent*>(lpEvent);
        GameStateModuleIO::ResetRaceCarCrashingAction lResetCrashingAction;
        lResetCrashingAction.meActiveRaceCarIndex = lpDrivingInCrashEvent->meActiveRaceCarIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetCrashingAction),
                                GameStateModuleIO::E_ACTION_RESET_RACE_CAR_CRASHING,
                                static_cast<s32>(sizeof(lResetCrashingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING_CHAIN:
    {
        const GameStateModuleIO::TrafficCheckingChainEvent* lpTrafficCheckingEvent =
            reinterpret_cast<const GameStateModuleIO::TrafficCheckingChainEvent*>(lpEvent);
        CGS_ASSERT(lpTrafficCheckingEvent, "lpTrafficCheckingEvent");

        // This one goes to the output buffer's own action queue.
        GameStateModuleIO::TrafficCheckingChainAction lChainAction;
        lChainAction.miChainSize = lpTrafficCheckingEvent->miChainSize;
        lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChainAction),
                                                 GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING_CHAIN,
                                                 static_cast<s32>(sizeof(lChainAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE, lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_CAN_SKIP_CRASH:
    {
        const GameStateModuleIO::HUDMessagePlayerCanSkipCrashAction lSkipCrashAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSkipCrashAction),
                                GameStateModuleIO::E_ACTION_HUD_MESSAGE_PLAYER_CAN_SKIP_CRASH,
                                static_cast<s32>(sizeof(lSkipCrashAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING:
    {
        const GameStateModuleIO::PlayerCrashEndingSoonAction lCrashEndingAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCrashEndingAction),
                                GameStateModuleIO::E_ACTION_PLAYER_CRASH_ENDING_SOON,
                                static_cast<s32>(sizeof(lCrashEndingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE, lpEvent, mfSimTimeStep);
        break;
    }
}

}
