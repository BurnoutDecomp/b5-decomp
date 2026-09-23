#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                            // mpNetworkModule accessors
#include "GameSource/Network/BrnNetworkModuleIO.h"                                          // OutputBuffer::SetInvitesOpen
#include "GameSource/Network/BrnServerInterface.h"                                          // component accessors, Update
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // GetTelemetryComponent
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                         // IsLocalPlayer
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                        // OnRoundFinish
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"                   // KI_E_MESSAGE_TYPE_COUNT
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                             // SetStartFrame / GetU16FrameCount
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                                    // CgsSystem::Time
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"                    // CgsSystem::TimerStatus

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- per-frame entry, session hooks and the small queries.
//
// Every sub-object is reached through its typed accessor (GetTrafficManager(), GetTimeManager(),
// ...); the base-class session hooks are called by their qualified names.
//
// Bodied here: ProcessBeforeSimulation, SyncTimeClientReadyCallback, GetLocalConsoleFrameRate,
// OnEnterGame, OnLeaveGame, OnLogIn, OnAutoLoginProcessComplete, OnGameLaunching, OnRoundStart,
// OnRoundFinish, OnGameFinish, OnGameStart, JoinGameSession, CaptureTelemetryEvent,
// GetActiveRaceCarIndex, IsLocalPlayer, IsDoingFreeBurnLobby, GetMaxMessageSize.
//
// Not bodied here yet (each waits on a declaration in a header this file does not own):
// ProcessAfterSimulation, ProcessHLUpdateFlags, OnAutoLogin, OnFinishedBoot, DxtDecodeCallback,
// ProcessNetworkTextureDecodeEvent.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // Latch this frame's game timer (its time, frame count and base time step), apply the
    // high-level update flags, then give every sub-manager its pre-simulation tick in the
    // console order and publish the player status / results and the invite state.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessBeforeSimulation(const BrnNetworkModuleIO::PreSimulationInputBuffer* lpInputBuffer,
                                                    BrnNetworkModuleIO::OutputBuffer*                  lpOutputBuffer,
                                                    CgsSystem::TimerStatus*                            lpTimerStatus,
                                                    BrnUpdateSet                                       luUpdateSet)
    {
        mpGameTimerStatus = lpTimerStatus;
        mTime             = lpTimerStatus->GetTime();
        miFrameNum        = mpGameTimerStatus->GetFrameCount();
        mfTimeStep        = mpGameTimerStatus->GetBaseTimeStep();

        ProcessHLUpdateFlags(luUpdateSet);

        GetAggressiveDrivingManager()->ProcessBeforeSimulation(lpOutputBuffer, mfTimeStep);
        GetStatsManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetLiveRevengeManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetScoreboardManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetStandingsManager()->Update(mbDiskReadErrorThisFrame);
        GetDirtyTrickManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetNetworkImageManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetChallengeSuccessManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetGamerCardManager()->ProcessBeforeSimulation();
        // The console call also hands over mfTimeStep, which the body never reads; the
        // committed declaration takes no argument.
        GetEventScoresManager()->ProcessBeforeSimulation();
        GetAutoLoginManager()->ProcessBeforeSimulation(mfTimeStep, luUpdateSet);
        GetTeamSelectionManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetRoadRulesManager()->ProcessBeforeSimulation(lpOutputBuffer, mfTimeStep, luUpdateSet);

        OutputPlayerStatusInfo(lpOutputBuffer);
        OutputPlayerResultsInfo(lpOutputBuffer);

        lpOutputBuffer->SetInvitesOpen(GetBuddyManager()->AreAnyInvitesOpen());
    }

    // ----------------------------------------------------------------------------------------
    // Start-time manager callback: a client finished syncing its clock. While the local player
    // sits in a game that has not started yet (the free-burn lobby), restart the networked
    // traffic for that client.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::SyncTimeClientReadyCallback(NetworkPlayerID lClientReadyID, void* lpUserData)
    {
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");
        CGS_ASSERT(lpNetworkManager->GetServerInterface(), "lpNetworkManager->GetServerInterface()");
        CGS_ASSERT(lpNetworkManager->GetServerInterface()->GetGameComponent(),
                   "lpNetworkManager->GetServerInterface()->GetGameComponent()");

        if (lpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
            !lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
        {
            CGS_ASSERT(lpNetworkManager->mpNetworkModule, "lpNetworkManager->mpNetworkModule");
            lpNetworkManager->GetTrafficManager()->RestartNetworkTraffic(lClientReadyID);
        }
    }

    CgsSystem::EFrameRate BrnNetworkManager::GetLocalConsoleFrameRate()
    {
        return meLocalConsoleFrameRate;
    }

    // ----------------------------------------------------------------------------------------
    // Session edges. Each one runs the base-class hook, then the sub-managers that care, and
    // the enter/leave pair invalidates the start-frame bookmark.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OnEnterGame()
    {
        CgsNetwork::NetworkManager::OnEnterGame();
        GetRoadRulesManager()->OnEnterGame();
        GetTimeManager()->SetStartFrame(CgsNetwork::TimeManager::E_START_FRAME_INVALID, NULL, 0.0f);
    }

    void BrnNetworkManager::OnLeaveGame()
    {
        CgsNetwork::NetworkManager::OnLeaveGame();
        GetLaunchManager()->OnLeaveGame();
        GetSelectedRoutesManager()->OnLeaveGame();
        GetRoadRulesManager()->OnLeaveGame();
        GetBuddyManager()->OnLeaveGame();
        mPlayerIDStatsGet = CgsNetwork::KI_INVALID_PLAYER_ID;
        GetLiveRevengeManager()->OnLeaveGame();
        GetTrafficManager()->OnLeaveGame();
        GetEventScoresManager()->OnLeaveOnline();
        GetTimeManager()->SetStartFrame(CgsNetwork::TimeManager::E_START_FRAME_INVALID, NULL, 0.0f);
    }

    void BrnNetworkManager::OnLogIn()
    {
        GetAutoLoginManager()->Connect();
    }

    void BrnNetworkManager::OnAutoLoginProcessComplete(u32 luCompletedProcess)
    {
        GetAutoLoginManager()->AutoLoginProcessComplete(luCompletedProcess);
    }

    void BrnNetworkManager::OnGameLaunching()
    {
        GetRoadRulesManager()->OnGameLaunching();
        GetTrafficManager()->OnGameLaunching();
    }

    // The round and game edges stamp the base hook with the current network time (rebuilt from
    // its float value) and the 16-bit network frame.
    void BrnNetworkManager::OnRoundStart()
    {
        CgsSystem::Time lStartTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnRoundStart(lStartTime, mfTimeStep, GetTimeManager()->GetU16FrameCount());

        GetStandingsManager()->OnRoundStart();
        GetTrafficManager()->OnRoundStart(IsDoingFreeBurnLobby());
        GetAggressiveDrivingManager()->OnRoundStart();
        GetLiveRevengeManager()->OnRoundStart();
        GetNetworkImageManager()->OnRoundStart();
        GetMarkedManManager()->OnRoundStart();
    }

    void BrnNetworkManager::OnRoundFinish()
    {
        CgsSystem::Time lFinishTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnRoundFinish(lFinishTime, GetTimeManager()->GetU16FrameCount());

        GetTrafficManager()->OnRoundFinish();
        GetPlayerManager()->OnRoundFinish();
        GetAggressiveDrivingManager()->OnRoundFinish();
        GetLiveRevengeManager()->OnRoundFinish();
        GetRoadRulesManager()->OnRoundFinish();
    }

    void BrnNetworkManager::OnGameFinish()
    {
        CgsSystem::Time lFinishTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnGameFinish(lFinishTime, GetTimeManager()->GetU16FrameCount());

        GetRoadRulesManager()->OnGameFinish();
        GetLiveRevengeManager()->OnGameFinish();
    }

    void BrnNetworkManager::OnGameStart()
    {
        CgsSystem::Time lStartTime(GetTime().GetFloatVal());
        CgsNetwork::NetworkManager::OnGameStart(lStartTime, GetTimeManager()->GetU16FrameCount());

        GetTrafficManager()->OnGameStart();
        GetStatsManager()->OnGameStart();
        GetCamera()->OnGameStart();
    }

    void BrnNetworkManager::JoinGameSession(char* lpcSessionID)
    {
        GetStateManager()->JoinGameSession(lpcSessionID);
    }

    void BrnNetworkManager::CaptureTelemetryEvent(ETelemetryHook leHook, const char* lpcData)
    {
        CGS_ASSERT(GetServerInterface(), "GetServerInterface()");
        CGS_ASSERT(GetServerInterface()->GetTelemetryComponent(), "GetServerInterface()->GetTelemetryComponent()");
        GetServerInterface()->GetTelemetryComponent()->CaptureEvent(leHook, lpcData);
    }

    EActiveRaceCarIndex BrnNetworkManager::GetActiveRaceCarIndex(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        return mpNetworkModule->GetActiveRaceCarIndex(lPlayerID);
    }

    // The local player's id comes from the server interface's player-info component.
    bool BrnNetworkManager::IsLocalPlayer(NetworkPlayerID lPlayerID)
    {
        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        return lPlayerInfo.GetID() == lPlayerID;
    }

    // In a game whose session has not started: the free-burn lobby.
    bool BrnNetworkManager::IsDoingFreeBurnLobby()
    {
        CgsNetwork::ServerInterfaceGames* lpGameComponent = GetServerInterface()->GetGameComponent();
        return lpGameComponent->IsLocalPlayerInGame() && !lpGameComponent->IsGameStarted();
    }

    // ----------------------------------------------------------------------------------------
    // The largest message size over every message type, or over the reliable ones only.
    // Construct checks the two maxima against KI_MAX_MESSAGE_SIZE and
    // KI_MAX_RELIABLE_MESSAGE_SIZE.
    // ----------------------------------------------------------------------------------------
    s32 BrnNetworkManager::GetMaxMessageSize(bool lbReliableOnly)
    {
        struct MessageDef
        {
            s32  miMessageSize;
            bool mbReliable;
        };

        // Indexed by message type. The sizes are the console byte sizes of the message classes
        // (the same console widths KI_MAX_MESSAGE_SIZE / KI_MAX_RELIABLE_MESSAGE_SIZE are written
        // in), not host sizeof values.
        const MessageDef laMessageDefs[CgsNetwork::KI_E_MESSAGE_TYPE_COUNT] =
        {
            {    0, false }, {   40, true  }, {   56, false }, {   84, false },   //  0 ..  3
            {   40, false }, {   96, true  }, {   36, false }, {   36, false },   //  4 ..  7
            {   32, false }, {   68, false }, {   36, false }, {  176, false },   //  8 .. 11
            {   64, true  }, {   88, true  }, {   40, false }, {   72, true  },   // 12 .. 15
            { 1968, false }, {  448, false }, {   52, true  }, {  568, true  },   // 16 .. 19
            {   36, false }, {   44, true  }, {   44, true  }, {   88, true  },   // 20 .. 23
            {  160, true  }, {   48, true  }, {  288, true  }, {   56, true  },   // 24 .. 27
            {   92, true  }, {   56, true  }, {   44, true  }, {  100, true  },   // 28 .. 31
            {   36, false }, {   48, true  }, {   64, true  }, {  296, true  },   // 32 .. 35
            {   48, false }, {   56, true  }, {   88, true  }, {   44, true  },   // 36 .. 39
            {   48, false }, {   44, true  }, {   52, true  }, {  116, true  },   // 40 .. 43
        };

        s32 liMaxMsgSize = 0;
        for (s32 liMsgType = 0; liMsgType < CgsNetwork::KI_E_MESSAGE_TYPE_COUNT; ++liMsgType)
        {
            if (!lbReliableOnly || laMessageDefs[liMsgType].mbReliable)
            {
                const s32 liMessageSize = laMessageDefs[liMsgType].miMessageSize;
                liMaxMsgSize = (liMaxMsgSize > liMessageSize) ? liMaxMsgSize : liMessageSize;
            }
        }
        return liMaxMsgSize;
    }
}
