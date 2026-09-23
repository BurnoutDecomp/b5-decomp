#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include <cstring>                                                                         // strstr
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // AmIHost
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"                // (Un)RegisterHostMigrationCallback
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/Network/BrnNetworkModule.h"                                           // GetNetworkManager / GetNetworkEventQueue / GetGameStateToNetworkInterface
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOutInstantFreeburnEvent / ShowLoadingScreen / AccountUpdateComplete / LaunchEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"                               // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // the memory checks' log line
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                // CgsGui::GuiEventNetworkInGame
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                  // HasStartTimePassed / StopSyncingTime
#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"         // SetGameSendingHeadsetStatus
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetAccountSettings
#include "GameSource/GameState/BrnGameEvents.h"                                            // FinishedSyncingPlayersEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                     // IsOnlineFreeBurnLobby
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // PostSimulationInputBuffer / NetworkEventQueue
#include "GameSource/Network/BrnServerInterface.h"                                          // GetGameComponent / GetPlayerInfoComponent / GetTelemetryComponent
#include "GameSource/Network/BrnNetworkGameParams.h"                                        // BrnNetwork::GameParams
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                                    // BrnNetwork::PlayerMenuData
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                      // BrnNetwork::PlayerParams
#include "GameSource/Network/Components/BrnServerInterfaceTelemetry.h"                      // EnableTemetry
#include "GameSource/Network/Managers/BrnNetworkConnectionManager.h"                         // UpdateNATData
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"                       // Update / StartProcess
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"                        // Update / IsSuspending / Suspend
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"                         // Update
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"                            // Start
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"              // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameSource/Resource/BrnResourceAllocator.h"                                      // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK

// ===================================================================================
// BrnNetwork::StateManager -- part 1: lifecycle and the small per-frame helpers.
//
//   Construct / Prepare / Release / Destruct   put every member back to its idle value;
//                                              Prepare / Release (un)hook the host-migration
//                                              callback, Release frees the downloaded TOS /
//                                              news, Prepare empties the XUID table
//   IsInLimbo                                  hosting a lobby game while the game state is
//                                              not in an online mode
//   StripWebOfferControlCodes                  cut a web-offer text down to the part between
//                                              its "%}" and "%{" control codes
//   OnGameIDChanged                            restart the freeburn lobby on a new game id
//   Disconnected                               drop every pending request and tell the game
//                                              the instant freeburn is off
//   UpdateSyncTime                             start the round once the synchronised start
//                                              time has passed
//   Update                                     the per-frame tick: NAT data, player texture,
//                                              local car colour, queued freeburn-lobby work,
//                                              the current state's work, joining a launch
//
// The four lifecycle functions share one reset of the cached lobby data: every cache is
// invalid, the marked man and the texture-output player are the invalid id, the district
// cache holds E_DISTRICT_CRISTAL_SUMMIT and the two leave reasons hold their COUNT values.
// ===================================================================================

namespace BrnNetwork
{
    // ------------------------------------------------------------------------------------
    // Bind to the owning module and its network manager, then put every member except the
    // XUID table (Prepare fills that) into its idle state.
    // ------------------------------------------------------------------------------------
    void StateManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        CGS_ASSERT(lpNetworkModule != nullptr, "lpNetworkModule");
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        meState                  = E_STATE_COUNT;
        meOutputPlayerTexture    = 0;
        mNetworkPlayerIDToOutput = -1;
        mpTOS                    = nullptr;
        mpNews                   = nullptr;

        mNewCarData.mCarModelId        = 0;
        mNewCarData.mWheelModelId      = 0;
        mNewCarData.mfDeformAmount     = 0.0f;
        mNewCarData.mbValid            = false;
        mMarkedManData.mPlayerID       = -1;
        mMarkedManData.mbValid         = false;
        mDistrictData.meDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mDistrictData.mfLastSentTime   = 0.0f;
        mDistrictData.mbValid          = false;
        mCarColourData.mfLastSentTime  = 0.0f;
        mCarColourData.mu16CarColourIndex   = 0;
        mCarColourData.mu16PaintFinishIndex = 0;
        mCarColourData.mbValid         = false;
        mFeverData.mbHasFever          = false;
        mFeverData.mbValid             = false;

        meLeftReason = E_LEFT_GAME_REASON_COUNT;
        meKickReason = CgsNetwork::E_KICKREASON_COUNT;

        mbSetNotPlaying                     = false;
        mbDoInviteAfterCreate               = false;
        mbSuspendAfterSignIn                = false;
        mbCreatedFromMenus                  = false;
        mbForceStartFreeburnLobby           = false;
        mbInstantFreeburn                   = false;
        mbReadyToJoinGameSession            = false;
        mbLoadingScreenVisible              = false;
        mbAreWeAutosaving                   = false;
        mbCloseLimboGameWhenIdle            = false;
        mbStartFreeburnLobbyThisFrame       = false;
        mbStartingAfterJoinThisFrame        = false;
        mbStartingAfterOnlineEventThisFrame = false;
        mbForceStartFreeburnLobbyThisFrame  = false;
        mbRefreshingFreeburnLobbyThisFrame  = false;
    }

    // ------------------------------------------------------------------------------------
    // Back to the idle state (the per-frame freeburn-lobby requests are left alone), hook
    // the host-migration callback and empty the XUID table. Always done in one step.
    // ------------------------------------------------------------------------------------
    bool StateManager::Prepare()
    {
        meState                  = E_STATE_COUNT;
        meOutputPlayerTexture    = 0;
        mNetworkPlayerIDToOutput = -1;
        mpTOS                    = nullptr;
        mpNews                   = nullptr;

        mNewCarData.mCarModelId        = 0;
        mNewCarData.mWheelModelId      = 0;
        mNewCarData.mfDeformAmount     = 0.0f;
        mNewCarData.mbValid            = false;
        mMarkedManData.mPlayerID       = -1;
        mMarkedManData.mbValid         = false;
        mDistrictData.meDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mDistrictData.mfLastSentTime   = 0.0f;
        mDistrictData.mbValid          = false;
        mCarColourData.mfLastSentTime  = 0.0f;
        mCarColourData.mu16CarColourIndex   = 0;
        mCarColourData.mu16PaintFinishIndex = 0;
        mCarColourData.mbValid         = false;
        mFeverData.mbHasFever          = false;
        mFeverData.mbValid             = false;

        mbSetNotPlaying          = false;
        mbDoInviteAfterCreate    = false;
        mbSuspendAfterSignIn     = false;
        mbCreatedFromMenus       = false;
        mbInstantFreeburn        = false;
        mbReadyToJoinGameSession = false;
        mbLoadingScreenVisible   = false;

        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetHostMigrationManager() != nullptr,
                   "mpNetworkManager->GetHostMigrationManager()");
        mpNetworkManager->GetHostMigrationManager()->RegisterHostMigrationCallback(&HostChangedCallback, this);

        mbForceStartFreeburnLobby = false;
        mbAreWeAutosaving         = false;
        mbCloseLimboGameWhenIdle  = false;
        meLeftReason = E_LEFT_GAME_REASON_COUNT;
        meKickReason = CgsNetwork::E_KICKREASON_COUNT;

        for (s32 liSlot = 0; liSlot < CurrentPlayerXUIDs::KI_NUM_SLOTS; ++liSlot)
        {
            mCurrentPlayerXUIDs.maSlots[liSlot].miPlayerId = CurrentPlayerXUIDs::KI_FREE_SLOT;
            mCurrentPlayerXUIDs.maSlots[liSlot].mu64XUID   = 0;
        }

        return true;
    }

    // ------------------------------------------------------------------------------------
    // Unhook the host-migration callback, go back to the idle state (the per-frame
    // freeburn-lobby requests are left alone) and free the downloaded TOS and news.
    // ------------------------------------------------------------------------------------
    bool StateManager::Release()
    {
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetHostMigrationManager() != nullptr,
                   "mpNetworkManager->GetHostMigrationManager()");
        mpNetworkManager->GetHostMigrationManager()->UnregisterHostMigrationCallback(&HostChangedCallback);

        mNewCarData.mCarModelId        = 0;
        mNewCarData.mWheelModelId      = 0;
        mNewCarData.mfDeformAmount     = 0.0f;
        mNewCarData.mbValid            = false;
        mMarkedManData.mPlayerID       = -1;
        mMarkedManData.mbValid         = false;
        mDistrictData.meDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mDistrictData.mfLastSentTime   = 0.0f;
        mDistrictData.mbValid          = false;
        mCarColourData.mfLastSentTime  = 0.0f;
        mCarColourData.mu16CarColourIndex   = 0;
        mCarColourData.mu16PaintFinishIndex = 0;
        mCarColourData.mbValid         = false;
        mFeverData.mbHasFever          = false;
        mFeverData.mbValid             = false;

        mbSetNotPlaying          = false;
        mbDoInviteAfterCreate    = false;
        mbSuspendAfterSignIn     = false;
        mbCreatedFromMenus       = false;
        mbInstantFreeburn        = false;
        mbReadyToJoinGameSession = false;
        mbLoadingScreenVisible   = false;

        meState                  = E_STATE_COUNT;
        meOutputPlayerTexture    = 0;
        mNetworkPlayerIDToOutput = -1;

        mbForceStartFreeburnLobby = false;
        mbAreWeAutosaving         = false;
        mbCloseLimboGameWhenIdle  = false;
        meLeftReason = E_LEFT_GAME_REASON_COUNT;
        meKickReason = CgsNetwork::E_KICKREASON_COUNT;

        if (mpTOS != nullptr)
        {
            CgsNetwork::ServerInterfaceDirtySock::MemFree(mpTOS, 0, 0);
            mpTOS = nullptr;
        }
        if (mpNews != nullptr)
        {
            CgsNetwork::ServerInterfaceDirtySock::MemFree(mpNews, 0, 0);
            mpNews = nullptr;
        }

        return true;
    }

    // ------------------------------------------------------------------------------------
    // Drop the module and manager bindings and put every other member except the XUID
    // table back into its idle state.
    // ------------------------------------------------------------------------------------
    void StateManager::Destruct()
    {
        mNewCarData.mCarModelId        = 0;
        mNewCarData.mWheelModelId      = 0;
        mNewCarData.mfDeformAmount     = 0.0f;
        mNewCarData.mbValid            = false;
        mMarkedManData.mPlayerID       = -1;
        mMarkedManData.mbValid         = false;
        mDistrictData.meDistrict       = BrnWorld::E_DISTRICT_CRISTAL_SUMMIT;
        mDistrictData.mfLastSentTime   = 0.0f;
        mDistrictData.mbValid          = false;
        mCarColourData.mfLastSentTime  = 0.0f;
        mCarColourData.mu16CarColourIndex   = 0;
        mCarColourData.mu16PaintFinishIndex = 0;
        mCarColourData.mbValid         = false;
        mFeverData.mbHasFever          = false;
        mFeverData.mbValid             = false;

        mNetworkPlayerIDToOutput = -1;
        mbSetNotPlaying          = false;
        meState                  = E_STATE_COUNT;
        meOutputPlayerTexture    = 0;
        mpNetworkManager         = nullptr;
        mpNetworkModule          = nullptr;
        mpTOS                    = nullptr;
        mpNews                   = nullptr;

        mbStartFreeburnLobbyThisFrame       = false;
        mbStartingAfterJoinThisFrame        = false;
        mbStartingAfterOnlineEventThisFrame = false;
        mbForceStartFreeburnLobbyThisFrame  = false;
        mbRefreshingFreeburnLobbyThisFrame  = false;

        mbCreatedFromMenus        = false;
        mbInstantFreeburn         = false;
        mbDoInviteAfterCreate     = false;
        mbSuspendAfterSignIn      = false;
        mbReadyToJoinGameSession  = false;
        mbLoadingScreenVisible    = false;
        mbForceStartFreeburnLobby = false;
        mbAreWeAutosaving         = false;
        meLeftReason = E_LEFT_GAME_REASON_COUNT;
        meKickReason = CgsNetwork::E_KICKREASON_COUNT;
        mbCloseLimboGameWhenIdle  = false;
    }

    // ------------------------------------------------------------------------------------
    // "In limbo": we host a game we are in, but the game state is not running an online
    // mode (the host backed out to offline play with the session still open).
    // ------------------------------------------------------------------------------------
    bool StateManager::IsInLimbo()
    {
        return mpNetworkManager->GetPlayerManager()->AmIHost()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
            && !mpNetworkModule->GetGameStateToNetworkInterface()->GetIsInOnlineGameMode();
    }

    // ------------------------------------------------------------------------------------
    // A downloaded web-offer text carries its payload between a "%}" and a "%{" control
    // code. Returns the payload (skipping a UTF-8 byte-order mark right after "%}") with the
    // closing code cut off in place, or null when there is no "%}".
    // ------------------------------------------------------------------------------------
    char* StateManager::StripWebOfferControlCodes(char* lpcBuffer)
    {
        lpcBuffer = strstr(lpcBuffer, "%}");
        if (lpcBuffer != nullptr)
        {
            lpcBuffer += 2;

            const u8* lpu8Text = reinterpret_cast<const u8*>(lpcBuffer);
            if (lpu8Text[0] == 0xEF && lpu8Text[1] == 0xBB && lpu8Text[2] == 0xBF)
            {
                lpcBuffer += 3;
            }

            char* lpcEndTag = strstr(lpcBuffer, "%{");
            if (lpcEndTag != nullptr)
            {
                *lpcEndTag = '\0';
            }
        }
        return lpcBuffer;
    }

    // ------------------------------------------------------------------------------------
    // The server moved us to a new game id. With other players in the game the freeburn
    // lobby restarts (forced); while matchmaking is still busy the restart is only flagged.
    // ------------------------------------------------------------------------------------
    void StateManager::OnGameIDChanged()
    {
        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame() > 1)
        {
            if (meState == E_STATE_WAIT_MATCHMAKING)
            {
                mbForceStartFreeburnLobby = true;
            }
            else
            {
                StartFreeBurnLobbyGameMode(false, false, true, false);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // The connection to the server was lost: go idle, forget every cached lobby change and
    // pending request, drop the downloads and any invite, and tell the game the instant
    // freeburn is off.
    // ------------------------------------------------------------------------------------
    void StateManager::Disconnected()
    {
        mbCreatedFromMenus        = false;
        meState                   = E_STATE_COUNT;
        mbInstantFreeburn         = false;
        mMarkedManData.mbValid    = false;
        mNewCarData.mbValid       = false;
        mDistrictData.mbValid     = false;
        mCarColourData.mbValid    = false;
        mFeverData.mbValid        = false;
        mbForceStartFreeburnLobby = false;
        mbSetNotPlaying           = false;

        ReleaseNewsAndTOSDownload();
        mpNetworkManager->GetNetworkInviteManager()->LogInComplete(false);

        mbCloseLimboGameWhenIdle = false;

        BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
        lInstantFreeburnEvent.mbIsDoingInstantFreeburn = false;
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lInstantFreeburnEvent,
                                                          lInstantFreeburnEvent.GetEventType());
    }

    const f32 StateManager::CachedDistrictData::KF_DISTRICT_UPDATE_INTERVAL     = 15.0f;
    const f32 StateManager::CachedCarColourData::KF_CAR_COLOUR_UPDATE_INTERVAL = 15.0f;

    // ------------------------------------------------------------------------------------
    // Waiting for the synchronised start. Once the start time has passed the round starts,
    // the game state hears that every player is synced, the game takes over the headset
    // status and we go racing (or idle, when the "game" is the freeburn lobby).
    // ------------------------------------------------------------------------------------
    void StateManager::UpdateSyncTime()
    {
        if (mpNetworkManager->GetStartTimeManager()->HasStartTimePassed())
        {
            mpNetworkManager->OnRoundStart();

            BrnGameState::GameStateModuleIO::FinishedSyncingPlayersEvent lEvent;
            mpNetworkModule->GetGameEventQueue()->AddEvent(
                &lEvent, BrnGameState::GameStateModuleIO::E_EVENT_FINISHED_SYNCING_PLAYERS);

            const bool lbDoingFreeBurnLobby = mpNetworkManager->IsDoingFreeBurnLobby();
            mpNetworkManager->GetVoIPManager()->SetGameSendingHeadsetStatus(true);
            meState = lbDoingFreeBurnLobby ? E_STATE_COUNT : E_STATE_RACING;
        }
    }

    // ------------------------------------------------------------------------------------
    // The per-frame tick. Keeps the NAT data and the player-texture output going, pushes a
    // changed local car colour into the lobby parameters, services the queued freeburn-lobby
    // restart / refresh, runs the current state's work and finally joins a game the host
    // started under us. Every step is followed by the available-memory check against the
    // reading taken on entry.
    // ------------------------------------------------------------------------------------
    void StateManager::Update(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        mpNetworkManager->GetConnectionManager()->UpdateNATData();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        OutputPlayerTexture();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        {
            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                mpNetworkManager->GetServerInterface()->GetGameComponent();
            PlayerParams lPlayerParams;

            CGS_ASSERT(lpGamesComponent != nullptr, "lpGamesComponent");
            CGS_ASSERT(lpInput != nullptr, "lpInput");
            CGS_ASSERT(lpInput->GetActiveRaceCarInterface() != nullptr, "lpInput->GetActiveRaceCarInterface()");

            if (lpInput->GetActiveRaceCarInterface()->IsPlayerCarActive())
            {
                const EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
                    lpInput->GetActiveRaceCarInterface()->GetPlayerActiveRaceCarIndex();

                if (lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0
                    && lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)
                {
                    const u32 luColourIndex =
                        lpInput->GetActiveRaceCarInterface()->GetActiveRaceCarColourIndex(lePlayerActiveRaceCarIndex);
                    const u32 luPaintFinishIndex = static_cast<u32>(
                        lpInput->GetActiveRaceCarInterface()->GetActiveRaceCarPaintFinishIndex(lePlayerActiveRaceCarIndex));

                    if (luColourIndex != static_cast<u32>(-1) && luPaintFinishIndex != static_cast<u32>(-1)
                        && (luColourIndex != mpNetworkManager->GetCurrentCarColourIndex()
                            || luPaintFinishIndex != mpNetworkManager->GetCurrentPaintFinishIndex()))
                    {
                        const u16 lu16CarColourIndex   = static_cast<u16>(luColourIndex);
                        const u16 lu16PaintFinishIndex = static_cast<u16>(luPaintFinishIndex);

                        if (lpGamesComponent->IsLocalPlayerInGame() && !lpGamesComponent->IsGameStarted())
                        {
                            lPlayerParams.Prepare();

                            NetworkPlayerID lLocalNetworkPlayerID = -1;
                            if (mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID))
                            {
                                lpGamesComponent->GetPlayerParametersByPlayerID(lLocalNetworkPlayerID, &lPlayerParams);

                                if (!(lu16PaintFinishIndex < 4))
                                {
                                    CgsDev::Assert::BeginAssert();
                                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                                    lStrStream << "Invalid Paint Finish: " << static_cast<s32>(lu16PaintFinishIndex) << "\n";
                                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                                    CgsDev::Assert::EndAssert();
                                }

                                mCarColourData.mbValid = lu16CarColourIndex != lPlayerParams.GetCarColourIndex()
                                                      || lu16PaintFinishIndex != lPlayerParams.GetPaintFinishIndex();
                                mCarColourData.mu16CarColourIndex   = lu16CarColourIndex;
                                mCarColourData.mu16PaintFinishIndex = lu16PaintFinishIndex;
                            }
                        }

                        mpNetworkManager->SetCurrentCarColourIndex(lu16CarColourIndex);
                        mpNetworkManager->SetCurrentPaintFinishIndex(lu16PaintFinishIndex);
                    }
                }
            }
        }
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        if (mbStartFreeburnLobbyThisFrame)
        {
            StartFreeBurnLobbyGameMode(mbStartingAfterJoinThisFrame, mbStartingAfterOnlineEventThisFrame,
                                       mbForceStartFreeburnLobbyThisFrame, false);
            mbStartFreeburnLobbyThisFrame       = false;
            mbStartingAfterJoinThisFrame        = false;
            mbStartingAfterOnlineEventThisFrame = false;
            mbForceStartFreeburnLobbyThisFrame  = false;
        }

        if (mbRefreshingFreeburnLobbyThisFrame)
        {
            RefreshFreeBurnLobbyGameMode();
            mbRefreshingFreeburnLobbyThisFrame = false;
        }

        switch (meState)
        {
        case E_STATE_LOGIN:
            UpdateLogin(lpInput);
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_LAUNCHING:
            UpdateLaunching();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_GAME_MODE_START:
        case E_STATE_RACING:
        case E_STATE_WAIT_TEAM_SELECTION:
        case E_STATE_PREPARING_FOR_INVITE:
        case E_STATE_PREPARED_FOR_INVITE:
            break;

        case E_STATE_WAIT_CAR_SELECT:
            if (HaveAllSelectedACar())
            {
                BrnNetworkModuleIO::NetworkOutShowLoadingScreen lShowLoadingScreen;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lShowLoadingScreen,
                                                                  lShowLoadingScreen.GetEventType());
                meState = E_STATE_WAIT_FOR_LOADING_SCREEN;

                NetworkPlayerID lPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(
                        mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                    CGS_ASSERT(lpMenuData != nullptr, "lpMenuData");
                    lpMenuData->mbFinalCarSelection = false;
                }
            }
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_SYNC_TIME:
            UpdateSyncTime();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_MATCHMAKING:
            mpNetworkManager->GetMatchMakingManager()->Update();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND:
            mpNetworkManager->GetSuspensionManager()->Update();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            if (!mpNetworkManager->GetSuspensionManager()->IsSuspending())
            {
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended())
                {
                    meState = E_STATE_COUNT;
                }
                else
                {
                    meState = E_STATE_WAIT_SUSPENSION;
                    // FLAG: Suspend is expanded inline here and both suspension types yield the
                    // same interface flags, so the type argument cannot be read back; OFFLINE is
                    // assumed.
                    mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                     &SuspensionFinishedCallback, this);
                }
            }
            break;

        case E_STATE_WAIT_SUSPENSION_IDLE_LEAVING_GAME:
            mpNetworkManager->GetSuspensionManager()->Update();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            if (!mpNetworkManager->GetSuspensionManager()->IsSuspending())
            {
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended())
                {
                    meState = E_STATE_COUNT;
                }
                else
                {
                    meState = E_STATE_WAIT_SUSPENSION_LEAVING_GAME;
                    // FLAG: suspension type not recoverable (see above); OFFLINE is assumed.
                    mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                     &SuspensionFinishedCallback, this);
                }
            }
            break;

        case E_STATE_WAIT_SUSPENSION:
        case E_STATE_WAIT_SUSPENSION_LEAVING_GAME:
            mpNetworkManager->GetSuspensionManager()->Update();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_POST_ROUND:
            mpNetworkManager->GetPostRoundManager()->Update();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_MODIFY_GAME:
            if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY)
            {
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                    == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
                {
                    mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
                }

                CgsGui::GuiEventNetworkInGame lInGameEvent;
                lInGameEvent.maData[0] = true;
                mpNetworkModule->AddOutputGuiEvent(lInGameEvent);
                meState = E_STATE_COUNT;
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }
            break;

        case E_STATE_WAIT_SERVER_INTERFACE_ACTION:
            if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY)
            {
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                    == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
                {
                    mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
                }
                meState = E_STATE_COUNT;
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }
            break;

        case E_STATE_WAIT_FOR_LOADING_SCREEN:
        {
            BrnNetworkModuleIO::NetworkOutShowLoadingScreen lShowLoadingScreen;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lShowLoadingScreen,
                                                              lShowLoadingScreen.GetEventType());
            break;
        }

        case E_STATE_WAIT_DOWNLOAD_NEWS_IDLE:
        case E_STATE_WAIT_DOWNLOAD_TOS_IDLE:
        case E_STATE_WAIT_DOWNLOAD_NEWS:
        case E_STATE_WAIT_DOWNLOAD_TOS:
            UpdateNewsAndTOSDownload();
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        case E_STATE_WAIT_UPDATE_ACCOUNT:
            switch (mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetStatus())
            {
            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY:
                break;

            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR:
            {
                BrnNetworkModuleIO::NetworkOutAccountUpdateComplete lNetworkUpdateAccountCompleteEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lNetworkUpdateAccountCompleteEvent,
                                                                  lNetworkUpdateAccountCompleteEvent.GetEventType());
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->ClearLastError();
                meState = E_STATE_COUNT;
                break;
            }

            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE:
            {
                BrnNetworkModuleIO::NetworkOutAccountUpdateComplete lNetworkUpdateAccountCompleteEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lNetworkUpdateAccountCompleteEvent,
                                                                  lNetworkUpdateAccountCompleteEvent.GetEventType());

                bool lbAgreeToShareInfoEA;
                bool lbAgreeToShareInfoPartners;
                bool lbTelemetryEnable;
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetAccountSettings(
                    &lbAgreeToShareInfoEA, &lbAgreeToShareInfoPartners, &lbTelemetryEnable);
                mpNetworkManager->GetServerInterface()->GetTelemetryComponent()->EnableTemetry(lbTelemetryEnable);
                meState = E_STATE_COUNT;
                break;
            }

            default:
                CGS_ASSERT(false, "Invalid component status\n");
                break;
            }
            break;

        case E_STATE_COUNT:
            if (mbCloseLimboGameWhenIdle)
            {
                mbCloseLimboGameWhenIdle = false;
                if (IsInLimbo())
                {
                    mpNetworkManager->GetMatchMakingManager()->StartProcess(
                        MatchMakingManager::E_PROCESS_LEAVE_GAME, &LeaveGameForOfflineGameFinishedCallback, this);
                    meState = E_STATE_WAIT_MATCHMAKING;
                    break;
                }
            }

            if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                && mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                       == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE
                && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
            {
                if (mMarkedManData.mbValid
                    || mNewCarData.mbValid
                    || mFeverData.mbValid
                    || (mDistrictData.mbValid
                        && mDistrictData.mfLastSentTime + CachedDistrictData::KF_DISTRICT_UPDATE_INTERVAL
                               < mpNetworkManager->GetTime().GetFloatVal())
                    || (mCarColourData.mbValid
                        && mCarColourData.mfLastSentTime + CachedCarColourData::KF_CAR_COLOUR_UPDATE_INTERVAL
                               < mpNetworkManager->GetTime().GetFloatVal())
                    || mbSetNotPlaying)
                {
                    PlayerParams    lLocalPlayerParams;
                    NetworkPlayerID lLocalNetworkPlayerID = -1;
                    bool            lbParamsUpdated       = false;

                    if (mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID))
                    {
                        lLocalPlayerParams.Prepare();
                        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                            lLocalNetworkPlayerID, &lLocalPlayerParams);

                        if (mMarkedManData.mbValid)
                        {
                            if (lLocalPlayerParams.GetMarkedPlayerID() != mMarkedManData.mPlayerID)
                            {
                                lLocalPlayerParams.SetMarkedPlayerID(mMarkedManData.mPlayerID);
                                lbParamsUpdated = true;
                            }
                            mMarkedManData.mbValid = false;
                        }

                        if (mFeverData.mbValid)
                        {
                            if (lLocalPlayerParams.HasFever() != mFeverData.mbHasFever)
                            {
                                lLocalPlayerParams.SetHasFever(mFeverData.mbHasFever);
                                lbParamsUpdated = true;
                            }
                            mFeverData.mbValid = false;
                        }

                        if (mNewCarData.mbValid)
                        {
                            CgsID lFreeBurnCarID;
                            lLocalPlayerParams.GetFreeBurnCarID(&lFreeBurnCarID);
                            CGS_ASSERT(mNewCarData.mCarModelId != 0, "mNewCarData.mCarModelId != kCGSID_NULL");
                            if (lFreeBurnCarID != mNewCarData.mCarModelId)
                            {
                                lLocalPlayerParams.SetFreeBurnCarID(mNewCarData.mCarModelId);
                                lLocalPlayerParams.SetIsCarDeformed(!(mNewCarData.mfDeformAmount < 0.85f));
                                lbParamsUpdated = true;
                            }
                            mNewCarData.mbValid = false;
                        }

                        if (mCarColourData.mbValid)
                        {
                            if (lLocalPlayerParams.GetCarColourIndex() != mCarColourData.mu16CarColourIndex)
                            {
                                lLocalPlayerParams.SetCarColourIndex(mCarColourData.mu16CarColourIndex);
                                lbParamsUpdated = true;
                                if (!(lLocalPlayerParams.GetPaintFinishIndex() < 4))
                                {
                                    CgsDev::Assert::BeginAssert();
                                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                                    lStrStream << "Invalid Paint Finish: "
                                               << static_cast<s32>(lLocalPlayerParams.GetPaintFinishIndex()) << "\n";
                                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                                    CgsDev::Assert::EndAssert();
                                }
                            }
                            if (lLocalPlayerParams.GetPaintFinishIndex() != mCarColourData.mu16PaintFinishIndex)
                            {
                                lLocalPlayerParams.SetPaintFinishIndex(mCarColourData.mu16PaintFinishIndex);
                                lbParamsUpdated = true;
                                if (!(lLocalPlayerParams.GetPaintFinishIndex() < 4))
                                {
                                    CgsDev::Assert::BeginAssert();
                                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                                    lStrStream << "Invalid Paint Finish: "
                                               << static_cast<s32>(lLocalPlayerParams.GetPaintFinishIndex()) << "\n";
                                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                                    CgsDev::Assert::EndAssert();
                                }
                            }
                            mCarColourData.mbValid        = false;
                            mCarColourData.mfLastSentTime = mpNetworkManager->GetTime().GetFloatVal();
                        }

                        if (mbSetNotPlaying)
                        {
                            if (lLocalPlayerParams.IsPlaying())
                            {
                                lLocalPlayerParams.SetPlaying(false);
                                lbParamsUpdated = true;
                            }
                            mbSetNotPlaying = false;
                        }

                        if (lbParamsUpdated)
                        {
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdatePlayerParameters(
                                lLocalNetworkPlayerID, &lLocalPlayerParams);
                            meState = E_STATE_WAIT_SERVER_INTERFACE_ACTION;
                        }
                    }
                }
            }
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            break;

        default:
        {
            CgsDev::Assert::BeginAssert();
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unknown state " << static_cast<s32>(meState) << "\n";
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        // A client whose host started the game while we idled in the lobby (or while syncing
        // for a freeburn-lobby / showtime round) joins the launch.
        if (mpNetworkModule != nullptr && mpNetworkModule->GetGameStateToNetworkInterface() != nullptr)
        {
            if (meState == E_STATE_COUNT
                || (meState == E_STATE_SYNC_TIME
                    && BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                           mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode())))
            {
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                    && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost()
                    && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                {
                    GameParams lGameParams;

                    if (meState == E_STATE_SYNC_TIME)
                    {
                        mpNetworkManager->GetStartTimeManager()->StopSyncingTime();
                    }

                    lGameParams.Prepare();
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

                    BrnNetworkModuleIO::NetworkOutLaunchEvent lNetworkLaunchEvent;
                    lNetworkLaunchEvent.meGameModeType =
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(lGameParams.GameMode());
                    lNetworkLaunchEvent.mHostPlayerID = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(&lNetworkLaunchEvent,
                                                                      lNetworkLaunchEvent.GetEventType());

                    mpNetworkManager->GetLaunchManager()->Start();
                    mpNetworkManager->OnGameLaunching();
                    mpNetworkManager->GetVoIPManager()->SetGameSendingHeadsetStatus(false);
                    meState = E_STATE_LAUNCHING;
                    // FLAG: the console also writes "Setting state to launching: starting
                    // launch\n" to a network debug stream that has no home.
                }
            }
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
        }
    }
} // namespace BrnNetwork
