// GameStateModule::ProcessGameEvents -- the online player arms and the freeburn-challenge arms.
//
// The console's ProcessGameEvents is one switch over the merged pre-world game-event queue; this
// tree runs it as one extracted walk per arm family (see PreWorldUpdateStuntBringUp in
// GameStateModule_gUI_00.cpp for the walk order and the merged queue). The two walks here are
// called straight after the network game/round start arms.
//
// Console dispatcher layout the arms below read by name: gsm+0x1020 = mModeManager, gsm+0x1DD0 = its
// ScoringSystem, gsm+0x1DB4 = the current game mode type, +0x397E0 = mLastActiveRaceCarInterface;
// the arguments are the game action queue, the pre-world input buffer and the output buffer.

#include "GameSource/GameState/BrnGameStateModule.h"

#include <cstring>                                                      // std::memcpy (case 123 frame-rate request)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDCompress (case 123's fallback car)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16>
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"         // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface (case 171)
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"            // [net] witness lines (PC harness)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // PreWorldInputBuffer / OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                         // the event records
#include "GameSource/GameState/BrnGameActions.h"                        // actions 5 / 11 / 12 / 147 / 220
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // the ModeManager online handlers
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::IsOnline
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem / CarData
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::OnPowerParkResult (case 54)
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::SetBestStuntStats (case 119)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::RequestTraining (case 119)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // the three case-119 tips
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // the player-status interface (GetLocalPlayerIsHost)

namespace BrnGameState
{
namespace
{
    // The +0x9EC read every online arm makes: the pre-world input buffer's
    // player-status interface, GetLocalPlayerIsHost(). The read lock is taken around it, as the
    // case-127/128 arms in GameStateModule_gUI_00.cpp do.
    bool IsLocalPlayerHost(GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer)
    {
        lpPreWorldInputBuffer->LockForRead();
        const GameStateModuleIO::PreWorldInputBuffer* lpcPreWorldInputBuffer = lpPreWorldInputBuffer;
        const bool lbIsHost = lpcPreWorldInputBuffer->GetPlayerStatusInterface()->GetLocalPlayerIsHost();
        lpPreWorldInputBuffer->UnlockForRead();
        return lbIsHost;
    }
}

// ============================================================================
// ProcessGameEventsOnlinePlayerBringUp -- ProcessGameEvents cases 7, 121, 122, 123, 124, 125,
// 129, 139 and 140.
//
//   case 7   (88 asm)  a remote player changed car: look up its scoring record and
//            active slot, read the car's current transform, post action 5 (SetupNetworkCarAction,
//            64 bytes). The world respawns the car in the same slot.
//   case 121 (52 asm)  a remote player's connection dropped: action 11 {id, slot}
//            when the player still has a car, then ModeManager::NetworkPlayerRemoved,
//            ModeManager::SetPlayerDisconnected, ScoringSystem::ClearPlayersBurnoutSkillzData.
//   case 122 (18 asm)  the local player connected: latch its network id.
//   case 123 (99 asm)  the local player was disconnected: leave the lobby (and online
//            car select), cancel an online mode, action 12, ModeManager::LocalPlayerDisconnected,
//            forget the local id and request the single-player frame rate.
//   case 124 (39 asm)  the local player left the lobby: cancel the lobby mode, cancel
//            any challenge, restore the free-burn car, clear every skillz record.
//   case 125 (20 asm)  the lobby's parameters changed (rich-presence fields only).
//   case 129 (51 asm)  a player was removed from the game: ModeManager and skillz
//            bookkeeping, and in the lobby modes action 220 (OnlinePlayerRemovedAction) for a
//            player that still has a car, then ScoringSystem::RemovePlayer.
//   case 139 (13 asm)  a remote player's burnout skillz: ScoringSystem::SetBurnoutSkillzData.
//   case 140 (14 asm)  a new host: ModeManager::HandleNewHostEvent.
//
// PARKED STORES (no member on this build, written out and unarmed):
//   * case 124's byte store to gsm+0x2C50C and case 125's three stores into gsm+0x2C4D0 (+0x3C = 1,
//     +0x20 = the game mode word, +0x2C = mbIsRanked ? 0 : 1) land in the embedded rich-presence
//     manager (the object at gsm+0x2C4D0 that Construct / Prepare stage 21 / PreWorldUpdate /
//     Release / Destruct pass as `this`). It is not embedded in this tree's GameStateModule, so
//     there is nothing to write:
//         mRichPresenceManager.<+0x3C> = 0;                                    (case 124)
//         mRichPresenceManager.<+0x3C> = 1; .<+0x20> = meGameMode;
//         .<+0x2C> = mbIsRanked ? 0 : 1;                                        (case 125)
//   * case 124's store of 1 to gsm+0x2C4A1: the byte case 12 also sets. An image-wide scan finds no
//     reader of it (the note on the case-26 arm in GameStateModule_gUI_00.cpp covers the same
//     run of write-only bytes).
//         <gsm+0x2C4A1> = 1;
// ============================================================================
void GameStateModule::ProcessGameEventsOnlinePlayerBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue*            lpActionQueue,
        GameStateModuleIO::OutputBuffer*               lpOutputBuffer)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0 || lpOutputBuffer == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        case GameStateModuleIO::E_EVENT_CHANGE_NETWORK_CAR:   // 7
        {
            const GameStateModuleIO::ChangeNetworkCarEvent* lpChangeEvent =
                reinterpret_cast<const GameStateModuleIO::ChangeNetworkCarEvent*>(lpEvent);
            ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();

            const CarData* lpCarData = lpScoringSystem->GetCarData(lpChangeEvent->mNetworkPlayerID);
            if (lpCarData == 0)
            {
                break;
            }
            const EActiveRaceCarIndex leActiveRaceCarIndex = lpCarData->GetActiveRaceCarIndex();
            if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                break;
            }
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState*
                lpRaceCarState = mLastActiveRaceCarInterface.GetRaceCarStateMutable(leActiveRaceCarIndex);
            CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

            // The console loads the transform's at row (+0x210) and translation row (+0x220)
            // before the scoring lookup and hands them over as the position (+0x220) and the at
            // vector (+0x210); the float is the event's +0x18.
            const Vector3 lAt       = lpRaceCarState->mTransform.At();
            const Vector3 lPosition = lpRaceCarState->mTransform.Pos();

            GameStateModuleIO::SetupNetworkCarAction lSetupAction;
            lSetupAction.Construct(lpScoringSystem->GetPlayerScoringIndex(lpChangeEvent->mNetworkPlayerID),
                                   leActiveRaceCarIndex, lPosition, lAt,
                                   lpChangeEvent->mCarModelId, lpChangeEvent->mWheelModelId,
                                   lpChangeEvent->mf18);
            // The console prints "NWCC: Network car in slot <n> changed to <id>(<name>)" here when
            // the message filter's bit 0 is set -- a developer log line with no state effect; not
            // reproduced.
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetupAction),
                                    GameStateModuleIO::E_ACTION_SETUP_NETWORK_CAR,
                                    sizeof(lSetupAction));

            BrnNetHarnessPC::Witness("game", "network car change -> action 5 net=%d active=%d",
                                     static_cast<s32>(lpChangeEvent->mNetworkPlayerID),
                                     static_cast<s32>(leActiveRaceCarIndex));
            break;
        }

        case GameStateModuleIO::E_EVENT_REMOTE_PLAYER_DISCONNECTED:   // 121
        {
            const GameStateModuleIO::RemotePlayerDisconnectedEvent* lpDisconnectedEvent =
                reinterpret_cast<const GameStateModuleIO::RemotePlayerDisconnectedEvent*>(lpEvent);
            CGS_ASSERT(lpDisconnectedEvent, "lpEvent");
            CGS_ASSERT(lpDisconnectedEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                       "lpPlayerDisconnectedEvent->GetNetworkPlayerID() != CgsNetwork::K_INVALID_PLAYER_ID");

            const BrnNetwork::NetworkPlayerID lPlayerID = lpDisconnectedEvent->mNetworkPlayerID;
            const ::EActiveRaceCarIndex leActiveRaceCarIndex = GetActiveRaceCarIndex(lPlayerID);
            if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                GameStateModuleIO::RemotePlayerDisconnectedAction lDisconnectedAction;
                lDisconnectedAction.SetNetworkPlayerID(lPlayerID);
                lDisconnectedAction.SetActiveRaceCarIndex(leActiveRaceCarIndex);
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDisconnectedAction),
                                        GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED,
                                        sizeof(lDisconnectedAction));
            }

            mModeManager.NetworkPlayerRemoved(lPlayerID, lpActionQueue,
                                              IsLocalPlayerHost(mpPreWorldInputBuffer));
            mModeManager.SetPlayerDisconnected(lPlayerID, &mLastActiveRaceCarInterface, lpActionQueue);
            mModeManager.GetScoringSystem()->ClearPlayersBurnoutSkillzData(lPlayerID);

            BrnNetHarnessPC::Witness("game", "remote player disconnected net=%d -> action 11 active=%d",
                                     static_cast<s32>(lPlayerID),
                                     static_cast<s32>(leActiveRaceCarIndex));
            break;
        }

        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_CONNECTED:   // 122
        {
            const GameStateModuleIO::LocalPlayerConnectedEvent* lpConnectedEvent =
                reinterpret_cast<const GameStateModuleIO::LocalPlayerConnectedEvent*>(lpEvent);
            CGS_ASSERT(lpConnectedEvent, "lpPlayerConnectedEvent");
            // The store comes first, then the assert on the stored value.
            mLocalPlayerNetworkID = lpConnectedEvent->mNetworkPlayerID;
            CGS_ASSERT(mLocalPlayerNetworkID != CgsNetwork::K_INVALID_PLAYER_ID,
                       "mLocalPlayerNetworkID != CgsNetwork::K_INVALID_PLAYER_ID");
            break;
        }

        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED:   // 123
        {
            mModeManager.SetAbortedDueToDisconnect();

            if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
            {
                if (mOnlineCarSelectManager.IsInOnlineCarSelect())
                {
                    mOnlineCarSelectManager.ExitOnlineCarSelect(lpActionQueue);

                    // The stack ChangePlayerCarEvent: the free-burn car (or the default car when
                    // the manager holds none), no wheel id, camera not reset, reset section kept.
                    GameStateModuleIO::ChangePlayerCarEvent lChangeEvent;
                    lChangeEvent.mbResetPlayerCamera = false;
                    CgsID lFreeburnCarId = mOnlineCarSelectManager.GetFreeburnCarId();
                    if (lFreeburnCarId == 0)
                    {
                        lFreeburnCarId = CgsIDCompress("PUSMC01");
                    }
                    lChangeEvent.mCarModelId         = lFreeburnCarId;
                    lChangeEvent.mWheelModelId       = 0;
                    lChangeEvent.mbKeepResetSection  = true;
                    HandleChangePlayerCarEvent(&lChangeEvent, lpActionQueue);
                }

                mModeManager.UserCancelCurrentMode();
                OnPlayerCarChange(mActivePlayerCarId, mActivePlayerWheelId, lpActionQueue, true);
            }

            const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
            const bool lbOnlineModeRunning =
                (lpCurrentGameMode != 0) ? lpCurrentGameMode->IsOnline() : false;
            if (lbOnlineModeRunning && mbWaitingForStreaming)
            {
                mModeManager.UserCancelCurrentMode();
            }

            // The console posts one uninitialised stack byte; the record carries no payload.
            GameStateModuleIO::NotifyDirectorLocalPlayerDisconnectedAction lNotifyAction;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNotifyAction),
                                    GameStateModuleIO::E_ACTION_NOTIFY_DIRECTOR_LOCAL_PLAYER_DISCONNECTED, 1);

            mModeManager.LocalPlayerDisconnected(lpActionQueue);
            mLocalPlayerNetworkID = CgsNetwork::K_INVALID_PLAYER_ID;

            // Request frame-rate type 1: the byte store (+4) first, then the word (+0). The
            // output buffer's frame-rate request slot is still the opaque
            // 12-byte storage (BrnGameStateModuleIO.h), so the two console offsets are written
            // through it, as ModeManager::StartGameMode / ExitCurrentMode do.
            CGS_ASSERT(lpOutputBuffer, "lpOutput");
            CGS_ASSERT(lpOutputBuffer->GetFrameRateTypeRequestInterface(),
                       "lpOutput->GetFrameRateTypeRequestInterface()");
            {
                GameStateModuleIO::OutputBufferFrameRateTypeReqInterface* lpFrameRateRequest =
                    lpOutputBuffer->GetFrameRateTypeRequestInterface();
                const s32 liRequestedFrameRateType = 1;
                lpFrameRateRequest->maOpaque[4] = 1;
                std::memcpy(&lpFrameRateRequest->maOpaque[0], &liRequestedFrameRateType, sizeof(s32));
            }

            BrnNetHarnessPC::Witness("game", "local player disconnected -> action 12 mode=%d",
                                     static_cast<s32>(mModeManager.GetCurrentGameModeType()));
            break;
        }

        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_LEFT_LOBBY:   // 124
        {
            // <rich presence +0x3C> = 0 -- PARKED, see the banner.
            const GameStateModuleIO::EGameModeType leModeType = mModeManager.GetCurrentGameModeType();
            if (GameStateModuleIO::IsOnlineFreeBurnLobby(leModeType))
            {
                mModeManager.UserCancelCurrentMode();
            }
            // <gsm+0x2C4A1> = 1 -- PARKED, see the banner. Then the road rules leave online mode.
            mRoadRulesManager.SetRoadRulesMode(lpOutputBuffer, false);
            mModeManager.CancelFreeburnChallenge(lpActionQueue);
            OnPlayerCarChange(mActivePlayerCarId, mActivePlayerWheelId, lpActionQueue, true);
            mModeManager.GetScoringSystem()->ClearAllBurnoutSkillzData();

            BrnNetHarnessPC::Witness("game", "local left lobby -> cancel mode=%d",
                                     static_cast<s32>(leModeType));
            break;
        }

        case GameStateModuleIO::E_EVENT_ONLINE_GAME_PARAMS_CHANGED:   // 125
        {
            const GameStateModuleIO::OnlineGameParamsChanged* lpGameParamEvent =
                reinterpret_cast<const GameStateModuleIO::OnlineGameParamsChanged*>(lpEvent);
            CGS_ASSERT(lpGameParamEvent, "lpGameParamEvent");
            // The three rich-presence stores -- PARKED, see the banner.
            break;
        }

        case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_REMOVED:   // 129
        {
            const GameStateModuleIO::OnlinePlayerRemovedEvent* lpRemovedEvent =
                reinterpret_cast<const GameStateModuleIO::OnlinePlayerRemovedEvent*>(lpEvent);
            CGS_ASSERT(lpRemovedEvent, "lpPlayerRemovedEvent");

            const BrnNetwork::NetworkPlayerID lPlayerID = lpRemovedEvent->mNetworkPlayerID;
            mModeManager.NetworkPlayerRemoved(lPlayerID, lpActionQueue,
                                              IsLocalPlayerHost(mpPreWorldInputBuffer));
            ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
            lpScoringSystem->ClearPlayersBurnoutSkillzData(lPlayerID);

            if (!GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
            {
                break;
            }

            const CarData* lpCarData = lpScoringSystem->GetCarData(lPlayerID);
            s32 liActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            if (lpCarData != 0 && lpCarData->GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                liActiveRaceCarIndex = lpCarData->GetActiveRaceCarIndex();
                GameStateModuleIO::OnlinePlayerRemovedAction lRemovedAction;
                lRemovedAction.SetActiveRaceCarIndex(lpCarData->GetActiveRaceCarIndex());
                lRemovedAction.mbIsLocalPlayerInGame = lpRemovedEvent->mbIsLocalPlayerInGame;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRemovedAction),
                                        GameStateModuleIO::E_ACTION_ONLINE_PLAYER_REMOVED,
                                        sizeof(lRemovedAction));
            }

            lpScoringSystem->RemovePlayer(lPlayerID);

            BrnNetHarnessPC::Witness("game", "player removed net=%d -> action 220 active=%d",
                                     static_cast<s32>(lPlayerID), liActiveRaceCarIndex);
            break;
        }

        case GameStateModuleIO::E_EVENT_ONLINE_NEW_BURNOUT_SKILLZ:   // 139
        {
            const GameStateModuleIO::NewRemoteBurnoutSkillzEvent* lpBurnoutSkillzEvent =
                reinterpret_cast<const GameStateModuleIO::NewRemoteBurnoutSkillzEvent*>(lpEvent);
            CGS_ASSERT(lpBurnoutSkillzEvent, "lpBurnoutSkillzEvent");
            mModeManager.GetScoringSystem()->SetBurnoutSkillzData(lpBurnoutSkillzEvent->mPlayerID,
                                                                  &lpBurnoutSkillzEvent->mNewSkillzData);
            break;
        }

        case GameStateModuleIO::E_EVENT_ONLINE_NEW_HOST:   // 140
        {
            const GameStateModuleIO::OnlineNewHostEvent* lpHostEvent =
                reinterpret_cast<const GameStateModuleIO::OnlineNewHostEvent*>(lpEvent);
            CGS_ASSERT(lpHostEvent, "lpHostEvent");
            mModeManager.HandleNewHostEvent(lpHostEvent, &mLastActiveRaceCarInterface, lpOutputBuffer);
            break;
        }

        default:
            break;
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

// ============================================================================
// ProcessGameEventsFreeburnChallengeBringUp -- ProcessGameEvents' freeburn-challenge arms and the
// ModeManager::ProcessEvent feed.
//
//   case 162 (64 asm)  the local player chose / cancelled / showed / hid a challenge
//            (the event's selector action at +8): 0 -> HandleLocalStartFreeburnChallengeMessage
//            (.., false) + TriggerFreeburnChallenge; 1 -> CancelFreeburnChallenge; 2 -> selector
//            visible + HandleLocalStartFreeburnChallengeMessage(.., true); 3 -> selector hidden.
//   case 163 / 164  the host started / triggered a challenge remotely.
//   case 168  a remote challenge ended (status at +8).
//   case 169  trigger the current challenge.
//   case 170  ChallengeManager::OutputFreeburnChallengeEveryPlayerStatusEvent.
//   case 171 / 172  the per-player success update / success.
//   case 54  (47 asm)  a power park: when it scored, the progression record, the
//            ModeManager feed and the achievement tally; then action 147 in every case.
//   case 119 (235 asm)  a completed stunt, outside the junkyard and online car
//            select: action 15 (CompletedStuntAction, 32 bytes), the ProcessEvent feed, the
//            developer challenges, the barrel-roll / flat-spin / handbrake-turn training tips,
//            achievements and barrel-roll tally, and the profile's best stunt stats.
//   cases 55, 65, 66, 71, 165, 166, 167, 173: ModeManager::ProcessEvent(type, event, delta) and
//            nothing else (173 passes its id as an immediate, the others the event type).
// Cases 67 / 69 / 70 feed ProcessEvent from inside the boost-ticker walk. Case 120 (the
// in-progress stunt, action 16) is not extracted: its convoy leg adds into the action's +0x18
// float without ever writing it first, so the console posts whatever that stack slot held and
// the value cannot be recovered. Its ProcessEvent feed does not run on this build.
//
// ProcessEvent's float is the console's cached game timestep (gsm+0x475BC); lfDelta is the same
// frame value, handed down by PreWorldUpdateStuntBringUp.
//
// Case 119's three training requests are the console's inlined TrainingManager::RequestTraining
// (state inactive, not in Picture Paradise, IsTipAllowedInGameMode, not yet seen, 5 s since the
// last message -> pending), called through the method here as the case-127 arm does.
// ============================================================================
namespace
{
    // Rodata 57.29578f: radians to degrees for the flat-spin angle.
    const f32 KF_RADIANS_TO_DEGREES = 57.29578f;
    // Rodata 90.0f: the flat spin a FLAT_SPIN training tip needs.
    const f32 KF_FLAT_SPIN_TIP_MIN_DEGREES = 90.0f;
}

void GameStateModule::ProcessGameEventsFreeburnChallengeBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue*            lpActionQueue,
        const CgsSystem::TimerStatusInterface&         lrTimerStatusInterface,
        f32                                            lfDelta)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:   // 54
        {
            const GameStateModuleIO::PowerParkResultEvent* lpPowerParkEvent =
                reinterpret_cast<const GameStateModuleIO::PowerParkResultEvent*>(lpEvent);
            if (lpPowerParkEvent->meOutcome == BrnWorld::E_PPO_SUCCESS)
            {
                CGS_ASSERT(IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0,
                           "IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0");
                mProgressionManager.OnPowerParkResult(lpPowerParkEvent->miOverallRating,
                                                      !(lpPowerParkEvent->miOtherPlayersInvolved < 2));
                mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_POWER_PARK_RESULT, lpEvent, lfDelta);
                mAchievementManager.OnPowerParking(lpPowerParkEvent->miOverallRating);
            }

            GameStateModuleIO::PowerParkResultAction lPowerParkAction;
            lPowerParkAction.meOutcome       = static_cast<BrnWorld::EPowerParkOutcome>(lpPowerParkEvent->meOutcome);
            lPowerParkAction.miOverallRating = lpPowerParkEvent->miOverallRating;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPowerParkAction),
                                    GameStateModuleIO::E_ACTION_POWER_PARK_RESULT,
                                    sizeof(lPowerParkAction));
            break;
        }

        case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:   // 119
        {
            // Not while the junkyard or online car select owns the player car.
            if (mCarSelectManager.GetJunkyardId() != 0 || mOnlineCarSelectManager.IsInOnlineCarSelect())
            {
                break;
            }
            const GameStateModuleIO::CompletedStuntEvent* lpCompletedStuntEvent =
                reinterpret_cast<const GameStateModuleIO::CompletedStuntEvent*>(lpEvent);
            CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");

            GameStateModuleIO::CompletedStuntAction lStuntAction;
            lStuntAction.muStuntActionComplete         = lpCompletedStuntEvent->muStuntActionComplete;
            lStuntAction.mfCompletedBarrelRollAngle    = lpCompletedStuntEvent->mfCompletedBarrelRollAngle;
            lStuntAction.mfCompletedAirSpinAngle       = lpCompletedStuntEvent->mfCompletedAirSpinAngle;
            lStuntAction.mfCompletedHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
            lStuntAction.mfCompletedDriftTime          = lpCompletedStuntEvent->mfCompletedDriftTime;
            lStuntAction.mfCompletedDriftDistance      = lpCompletedStuntEvent->mfCompletedDriftDistance;
            lStuntAction.miCompletedBarrelRolls        = lpCompletedStuntEvent->miCompletedBarrelRolls;
            lStuntAction.mbSuccessfulLanding           = lpCompletedStuntEvent->mbSuccessfulLanding;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStuntAction),
                                    GameStateModuleIO::E_ACTION_COMPLETED_STUNT, sizeof(lStuntAction));

            mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_COMPLETED_STUNT, lpEvent, lfDelta);
            mDeveloperChallengeManager.OnStuntOffenceComplete(reinterpret_cast<const u8*>(lpEvent));

            BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
            CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");

            const u32 luStuntActionComplete = lpCompletedStuntEvent->muStuntActionComplete;
            s32 liCompletedBarrelRolls      = 0;
            f32 lfFlatSpinDegrees           = 0.0f;   // the rodata 0.0f the console seeds all three from
            f32 lfHandbreakTurnAngle        = 0.0f;
            f32 lfDriftDistance             = 0.0f;

            if ((luStuntActionComplete & 1u) == 1u)
            {
                liCompletedBarrelRolls = lpCompletedStuntEvent->miCompletedBarrelRolls;
                mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_BARREL_ROLL);
                mAchievementManager.OnBarrelRoll(liCompletedBarrelRolls);
                mModeManager.GetScoringSystem()->PlayerPerformedBarrelRolls(GetPlayerActiveRaceCarIndex(),
                                                                            liCompletedBarrelRolls);
            }
            if ((luStuntActionComplete & 2u) == 2u)
            {
                lfFlatSpinDegrees = lpCompletedStuntEvent->mfCompletedAirSpinAngle * KF_RADIANS_TO_DEGREES;
                mAchievementManager.OnFlatSpin(lfFlatSpinDegrees);
                mDeveloperChallengeManager.OnFlatSpin(lfFlatSpinDegrees);
                if (lfFlatSpinDegrees > KF_FLAT_SPIN_TIP_MIN_DEGREES)
                {
                    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_FLAT_SPIN);
                }
            }
            if ((luStuntActionComplete & 4u) == 4u)
            {
                mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_USES_E_BRAKE);
                lfHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
            }
            if ((luStuntActionComplete & 0x40u) == 0x40u)
            {
                lfDriftDistance = lpCompletedStuntEvent->mfCompletedDriftDistance;
            }
            lpProfile->SetBestStuntStats(liCompletedBarrelRolls, lfFlatSpinDegrees, lfHandbreakTurnAngle,
                                         lfDriftDistance);
            break;
        }

        case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:                  // 55
        case GameStateModuleIO::E_EVENT_NEAR_MISS:                            // 65
        case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:            // 66
        case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:                   // 71
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:    // 165
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:             // 166
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS: // 167
        case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:            // 173
            mModeManager.ProcessEvent(static_cast<GameStateModuleIO::EGameEventType>(liType),
                                      lpEvent, lfDelta);
            break;

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED:   // 162
        {
            const GameStateModuleIO::FreeburnChallengeSelectedEvent* lpChallengeSelectedEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeSelectedEvent*>(lpEvent);
            CGS_ASSERT(lpChallengeSelectedEvent, "lpChallengeSelectedEvent");

            switch (lpChallengeSelectedEvent->meAction)
            {
            case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_CHOSEN:
                mModeManager.HandleLocalStartFreeburnChallengeMessage(
                    lpChallengeSelectedEvent->mChallengeID, lpActionQueue, &mLastActiveRaceCarInterface,
                    IsLocalPlayerHost(mpPreWorldInputBuffer), false);
                mModeManager.TriggerFreeburnChallenge(lpChallengeSelectedEvent->mChallengeID, lpActionQueue,
                                                      IsLocalPlayerHost(mpPreWorldInputBuffer));
                break;
            case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_CANCELED:
                mModeManager.CancelFreeburnChallenge(lpActionQueue);
                break;
            case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_SHOWN:
                mbFreeburnChallengeSelectorVisible = true;
                mModeManager.HandleLocalStartFreeburnChallengeMessage(
                    lpChallengeSelectedEvent->mChallengeID, lpActionQueue, &mLastActiveRaceCarInterface,
                    IsLocalPlayerHost(mpPreWorldInputBuffer), true);
                break;
            case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_HIDDEN:
                mbFreeburnChallengeSelectorVisible = false;
                break;
            default:
                break;
            }

            BrnNetHarnessPC::Witness("game", "fburn select id=%llX sub=%d",
                                     static_cast<unsigned long long>(lpChallengeSelectedEvent->mChallengeID),
                                     static_cast<s32>(lpChallengeSelectedEvent->meAction));
            break;
        }

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED_REMOTELY:   // 163
        {
            const GameStateModuleIO::FreeburnChallengeSelectedRemotelyEvent* lpSelectedEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeSelectedRemotelyEvent*>(lpEvent);
            CGS_ASSERT(lpSelectedEvent, "lpSelectedEvent");
            mModeManager.HandleRemoteStartFreeburnChallengeMessage(lpSelectedEvent->mChallengeID, lpActionQueue,
                                                                   IsLocalPlayerHost(mpPreWorldInputBuffer));
            BrnNetHarnessPC::Witness("game", "fburn remote start id=%llX",
                                     static_cast<unsigned long long>(lpSelectedEvent->mChallengeID));
            break;
        }

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_TRIGGERED_REMOTELY:   // 164
        {
            const GameStateModuleIO::FreeburnChallengeTriggeredRemotelyEvent* lpTriggeredEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeTriggeredRemotelyEvent*>(lpEvent);
            CGS_ASSERT(lpTriggeredEvent, "lpTriggeredEvent");
            mModeManager.HandleRemoteTriggeredFreeburnChallengeMessage(lpTriggeredEvent->mChallengeID, lpActionQueue,
                                                                       IsLocalPlayerHost(mpPreWorldInputBuffer));
            BrnNetHarnessPC::Witness("game", "fburn remote trigger id=%llX",
                                     static_cast<unsigned long long>(lpTriggeredEvent->mChallengeID));
            break;
        }

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ENDED:   // 168
        {
            const GameStateModuleIO::FreeburnChallengeEndedEvent* lpEndEvent =
                reinterpret_cast<const GameStateModuleIO::FreeburnChallengeEndedEvent*>(lpEvent);
            CGS_ASSERT(lpEndEvent, "lpEndEvent");
            mModeManager.HandleOnlineEndFreeburnChallengeMessage(lpActionQueue, lpEndEvent->meChallengeStatus,
                                                                 IsLocalPlayerHost(mpPreWorldInputBuffer));
            BrnNetHarnessPC::Witness("game", "fburn remote end status=%d",
                                     static_cast<s32>(lpEndEvent->meChallengeStatus));
            break;
        }

        case GameStateModuleIO::E_EVENT_TRIGGER_FREEBURN_CHALLENGE:   // 169
        {
            const bool lbIsHost = IsLocalPlayerHost(mpPreWorldInputBuffer);
            mModeManager.TriggerFreeburnChallenge(mModeManager.GetCurrentFreeburnChallengeID(), lpActionQueue,
                                                  lbIsHost);
            break;
        }

        case GameStateModuleIO::E_EVENT_REQUEST_EVERY_PLAYER_COMPLETION_STATUS:   // 170
            mModeManager.OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
            break;

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE:   // 171
        {
            const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpSuccessUpdateEvent =
                reinterpret_cast<const GameStateModuleIO::FburnChallengeSuccessUpdateEvent*>(lpEvent);
            CGS_ASSERT(lpSuccessUpdateEvent, "lpSuccessUpdateEvent");
            mModeManager.HandleSuccessUpdateEvent(&lrTimerStatusInterface, lpSuccessUpdateEvent);
            break;
        }

        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS:   // 172
        {
            const GameStateModuleIO::FburnChallengeSuccessEvent* lpChallengeSuccessEvent =
                reinterpret_cast<const GameStateModuleIO::FburnChallengeSuccessEvent*>(lpEvent);
            CGS_ASSERT(lpChallengeSuccessEvent, "lpChallengeSuccessEvent");
            mModeManager.HandleChallengeSuccessEvent(lpChallengeSuccessEvent);
            break;
        }

        default:
            break;
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}
}
