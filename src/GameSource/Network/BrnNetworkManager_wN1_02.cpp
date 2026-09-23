#include "types.hpp"

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                            // mpNetworkModule accessors
#include "GameSource/Network/BrnNetworkModuleIO.h"                                          // OutputBuffer::SetInvitesOpen
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                                   // NetworkInDxtDecodeImageEvent
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                  // NetworkOutLocalPlayerDisconnected, NetworkOutImageDxtDecodedEvent
#include "GameSource/Network/BrnServerInterface.h"                                          // component accessors, Update
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // GetTelemetryComponent
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                         // IsLocalPlayer
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"              // NetworkToGameStateInterface::SetFramesSinceStart
#include "GameSource/GameState/BrnGameEvents.h"                                             // LocalPlayerDisconnectedEvent
#include "GameSource/Resource/BrnResourceAllocator.h"                                       // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                  // the memory checks' log line
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                   // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                 // CgsGui::GuiEventNetworkDisconnected
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                        // OnRoundFinish, SetDiskAccessible
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"        // E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"                       // ProcessNetworkTextureDecodeEvent
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
// Bodied here: ProcessBeforeSimulation, ProcessAfterSimulation, ProcessHLUpdateFlags,
// SyncTimeClientReadyCallback, GetLocalConsoleFrameRate, OnEnterGame, OnLeaveGame, OnLogIn,
// OnAutoLogin, OnAutoLoginProcessComplete, OnFinishedBoot, OnGameLaunching, OnRoundStart,
// OnRoundFinish, OnGameFinish, OnGameStart, JoinGameSession, CaptureTelemetryEvent,
// GetActiveRaceCarIndex, IsLocalPlayer, IsDoingFreeBurnLobby, GetMaxMessageSize,
// DxtDecodeCallback, ProcessNetworkTextureDecodeEvent.
// ============================================================================================

namespace GsmIO = BrnGameState::GameStateModuleIO;

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
        GetEventScoresManager()->ProcessBeforeSimulation(mfTimeStep);
        GetAutoLoginManager()->ProcessBeforeSimulation(mfTimeStep, luUpdateSet);
        GetTeamSelectionManager()->ProcessBeforeSimulation(lpOutputBuffer);
        GetRoadRulesManager()->ProcessBeforeSimulation(lpOutputBuffer, mfTimeStep, luUpdateSet);

        OutputPlayerStatusInfo(lpOutputBuffer);
        OutputPlayerResultsInfo(lpOutputBuffer);

        lpOutputBuffer->SetInvitesOpen(GetBuddyManager()->AreAnyInvitesOpen());
    }

    // ----------------------------------------------------------------------------------------
    // Post-simulation tick: apply the high-level update flags, then run every sub-manager in the
    // console order, most bracketed by a CPU monitor, and finally advance the network frame and
    // publish the frames-since-start count to the game state. The free memory read on entry is
    // re-checked after nearly every leg.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                                                   BrnUpdateSet                                        luUpdateSet)
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        const bool lbAreWePlaying = GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        ProcessHLUpdateFlags(luUpdateSet);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStateManagerPM);
        GetStateManager()->UpdateEvents(lpInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStateManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkServerInterfacePM);
        GetServerInterface()->Update(lpInputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkServerInterfacePM);

        CgsDev::PerfMonCpu::StartMonitor(miCGSNetworkAfterSimPM);
        CgsNetwork::NetworkManager::Update(mpGameTimerStatus, GetTimeManager()->GetU16FrameCount(), lbAreWePlaying);
        CgsDev::PerfMonCpu::StopMonitor(miCGSNetworkAfterSimPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkBuddyPM);
        GetBuddyManager()->Update(lpInputBuffer, mfTimeStep,
                                  GetStateManager()->IsReadyToJoinGameSession(),
                                  GetStateManager()->IsReadyToJoinGameSession());
        CgsDev::PerfMonCpu::StopMonitor(miNetworkBuddyPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkTrafficPM);
        GetTrafficManager()->Update(lbAreWePlaying);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkTrafficPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkAggressiveDrivingPM);
        GetAggressiveDrivingManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkAggressiveDrivingPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkDirtyTrickPM);
        GetDirtyTrickManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkDirtyTrickPM);

        GetNetworkImageManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miGamerPicUpdatPM);
        GetGamerPictureManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miGamerPicUpdatPM);

        CgsDev::PerfMonCpu::StartMonitor(miCameraUpdatePM);
        GetCamera()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miCameraUpdatePM);

        GetTextureCompressor()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkNotificationPM);
        GetNetworkNotificationManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkNotificationPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkInvitePM);
        GetNetworkInviteManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkInvitePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStatsPM);
        GetStatsManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStatsPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkLiveRevengePM);
        GetLiveRevengeManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkLiveRevengePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkRoutesManagerPM);
        GetSelectedRoutesManager()->Update();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkRoutesManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkStateManagerPM);
        GetStateManager()->Update(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkStateManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkRoadRulesManagerPM);
        GetRoadRulesManager()->ProcessAfterSimulation(lpInputBuffer, lbAreWePlaying);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkRoadRulesManagerPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkChallengeManagerPM);
        GetChallengeSuccessManager()->ProcessAfterSimulation(lpInputBuffer);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkChallengeManagerPM);

        // A route change made while in a game republishes the game parameters.
        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerSelectedRoutesPM);
        if (GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
            GetSelectedRoutesManager()->HaveRoutesChanged())
        {
            GetSelectedRoutesManager()->SetRoutesChanged(false);
            OutputGameParameters();
        }
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerSelectedRoutesPM);

        GetEventScoresManager()->ProcessAfterSimulation(lpInputBuffer);
        GetTeamSelectionManager()->ProcessAfterSimulation();

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerOutputPlayerStatsPM);
        OutputPlayerStatsToGui(mPlayerIDStatsGet);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerOutputPlayerStatsPM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerPostUpdatePM);
        CgsNetwork::NetworkManager::PostUpdate(mpGameTimerStatus, GetTimeManager()->GetU16FrameCount());
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerPostUpdatePM);

        CgsDev::PerfMonCpu::StartMonitor(miNetworkManagerMiscPM);
        GetTimeManager()->NextFrame(mpGameTimerStatus);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        mpGameTimerStatus = NULL;
        const s32 liFramesSinceStart = GetTimeManager()->GetFramesSinceStart();
        mpNetworkModule->GetNetworkToGameStateInterface()->SetFramesSinceStart(liFramesSinceStart);
        CgsDev::PerfMonCpu::StopMonitor(miNetworkManagerMiscPM);
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
    }

    // ----------------------------------------------------------------------------------------
    // Bit 0x200 of the update set is the disk-read-error flag. While it is raised the manager
    // revokes every invite, marks the disk inaccessible and drops the server connection; once it
    // clears, the disk is accessible again and, if a disconnect was caused by the error, the GUI,
    // the game state and the network event queue are told the local player disconnected.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessHLUpdateFlags(BrnUpdateSet luUpdateSet)
    {
        CGS_ASSERT(GetPlayerManager(), "GetPlayerManager()");

        mbDiskReadErrorThisFrame = (luUpdateSet & 0x200) != 0;
        if (mbDiskReadErrorThisFrame)
        {
            CGS_ASSERT(GetServerInterface(), "GetServerInterface()");
            CGS_ASSERT(GetServerInterface()->GetConnectionComponent(), "GetServerInterface()->GetConnectionComponent()");

            mbNotifyGameOfDisconnectAfterDiskError = true;
            GetBuddyManager()->RevokeAllInvites();
            GetPlayerManager()->SetDiskAccessible(false);
            GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
        }
        else
        {
            GetPlayerManager()->SetDiskAccessible(true);
            if (mbNotifyGameOfDisconnectAfterDiskError)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.meLastError  = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT;
                lDisconnectedEvent.macReason[0] = '\0';
                mpNetworkModule->AddOutputGuiEvent(lDisconnectedEvent);

                // One empty record goes onto both queues.
                GsmIO::LocalPlayerDisconnectedEvent lGameDisconnectedEvent;
                mpNetworkModule->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lGameDisconnectedEvent),
                    GsmIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED, sizeof(lGameDisconnectedEvent));

                BrnNetworkModuleIO::NetworkOutLocalPlayerDisconnected lNetworkDisconnectedEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkDisconnectedEvent),
                    lNetworkDisconnectedEvent.GetEventType(), sizeof(lNetworkDisconnectedEvent));

                mbNotifyGameOfDisconnectAfterDiskError = false;
            }
        }
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

    void BrnNetworkManager::OnAutoLogin()
    {
        GetRoadRulesManager()->OnAutoLogin();
        GetEventScoresManager()->OnAutoLogin();
        GetLiveRevengeManager()->SendLiveRevengeRivalsToServer();
        GetStatsManager()->UploadOfflineProgression();
    }

    void BrnNetworkManager::OnAutoLoginProcessComplete(u32 luCompletedProcess)
    {
        GetAutoLoginManager()->AutoLoginProcessComplete(luCompletedProcess);
    }

    void BrnNetworkManager::OnFinishedBoot()
    {
        GetAutoLoginManager()->OnFinishedBoot();
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

    // ----------------------------------------------------------------------------------------
    // Texture compressor callback: a DXT decode finished. Queue the decoded pixels, tagged with
    // the player the image belongs to, onto the module's network event queue.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::DxtDecodeCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT(lpPixels, "lpPixels");
        CGS_ASSERT(lpUserData, "lpUserData");
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        BrnNetworkModuleIO::NetworkOutImageDxtDecodedEvent lNetworkOutImageDxtDecodedEvent;
        lNetworkOutImageDxtDecodedEvent.mpDecodedPixels = lpPixels;
        lNetworkOutImageDxtDecodedEvent.mPlayerName     = lpNetworkManager->mExportPlayerName;

        CGS_ASSERT(lpNetworkManager->mpNetworkModule, "lpNetworkManager->mpNetworkModule");
        CGS_ASSERT(lpNetworkManager->mpNetworkModule->GetNetworkEventQueue(),
                   "lpNetworkManager->mpNetworkModule->GetNetworkEventQueue()");
        lpNetworkManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lNetworkOutImageDxtDecodedEvent),
            lNetworkOutImageDxtDecodedEvent.GetEventType(), sizeof(lNetworkOutImageDxtDecodedEvent));
    }

    // ----------------------------------------------------------------------------------------
    // A received player image needs decoding: remember whose it is and hand the DXT1 pixels to
    // the texture compressor, which calls DxtDecodeCallback when the decode job completes.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::ProcessNetworkTextureDecodeEvent(
            const BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent* lpDxtDecodeRequestEvent)
    {
        CGS_ASSERT(lpDxtDecodeRequestEvent, "lpDxtDecodeRequestEvent");
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == lpDxtDecodeRequestEvent->mpTextureToDecode->GetFormat(),
                   "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpDxtDecodeRequestEvent->mpTextureToDecode->GetFormat()");

        mExportPlayerName = lpDxtDecodeRequestEvent->mPlayerName;

        const CgsNetwork::NetworkTexture* lpTexture = lpDxtDecodeRequestEvent->mpTextureToDecode;
        GetTextureCompressor()->SetNewTextureToDecompress(lpTexture->GetTexture(),
                                                          lpTexture->GetTextureSize(),
                                                          lpTexture->GetWidth(),
                                                          lpTexture->GetHeight(),
                                                          DxtDecodeCallback,
                                                          this);
    }
}
