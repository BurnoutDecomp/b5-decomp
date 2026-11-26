#include "GameSource/Network/Managers/BrnNetworkStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkStateManager.cpp"
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::StateManager::CurrentPlayerXUIDs::SetXUID  @ 0x82542540
//     (called by BrnNetwork::StateManager::StartGameMode)
//   BrnNetwork::StateManager::CurrentPlayerXUIDs::GetXUID  @ 0x825425C0
//     (called by BrnNetwork::StateManager::ProcessGuiEvents)
//
// An 8-slot, 16-byte-stride table mapping a network player id to that player's XUID.
// SetXUID inserts into the first free (miPlayerId == -1) slot; GetXUID looks a player up
// and copies out the stored 64-bit XUID. Both walk maSlots linearly and assert on
// overflow / not-found exactly as the X360 bodies do (the asm scans up to 8 slots before
// firing). Each store matches the asm width: the player id is a 32-bit `stw`, the XUID a
// single 64-bit `std` (the Hex-Rays high/low-half aliases are one std, not two stores).

namespace BrnNetwork
{
    void StateManager::CurrentPlayerXUIDs::SetXUID(s32 liPlayerId, u64 lqXUID)
    {
        s32 liSlot = 0;
        while (maSlots[liSlot].miPlayerId != KI_FREE_SLOT)
        {
            if (++liSlot >= KI_NUM_SLOTS)
            {
                CGS_FAIL("No free slots!\n");
                return;
            }
        }

        maSlots[liSlot].miPlayerId = liPlayerId;   // stw r4, 0(slot)
        maSlots[liSlot].mu64XUID   = lqXUID;        // std r5, 8(slot)
    }

    void StateManager::CurrentPlayerXUIDs::GetXUID(s32 liPlayerId, u64* lpXUID)
    {
        CGS_ASSERT(lpXUID != 0, "lpXUID");

        s32 liSlot = 0;
        while (maSlots[liSlot].miPlayerId != liPlayerId)
        {
            if (++liSlot >= KI_NUM_SLOTS)
            {
                CGS_FAIL("Couldn't find player!\n");
                return;
            }
        }

        *lpXUID = maSlots[liSlot].mu64XUID;          // ld 8(slot); std 0(lpXUID)
    }

    // StateManager::TeamSelectionFinishedCallback
    //
    // Completion callback handed to the team-selection flow. Asserts that selection
    // succeeded and that the user data is the state manager, then moves the state machine
    // on to the car-select wait. The asserts are non-fatal, so the store runs on every path.
    void StateManager::TeamSelectionFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT(lbSuccess, "Team selection has failed somehow\n");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != 0, "lpStateManager");

        lpStateManager->meState = E_STATE_WAIT_CAR_SELECT;
    }
} // namespace BrnNetwork

// FOLDED FROM BrnNetworkStateManager_wN1_01.cpp
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

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
    typedef CgsNetwork::ServerInterfaceDirtySock NativeServerInterface;
    // ------------------------------------------------------------------------------------
    // Bind to the owning module and its network manager, then put every member except the
    // XUID table (Prepare fills that) into its idle state.
    // ------------------------------------------------------------------------------------
    void StateManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        CGS_ASSERT_AT(lpNetworkModule != 0,
            "lpNetworkModule",
            __FILE__, 75 /* ARTIST 0x82548F80 */);
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT_AT(mpNetworkManager != 0,
            "mpNetworkManager",
            __FILE__, 78 /* ARTIST 0x82548FB4 */);

        meState                  = E_STATE_COUNT;
        meOutputPlayerTexture    = 0;
        mNetworkPlayerIDToOutput = -1;
        mpTOS                    = 0;
        mpNews                   = 0;

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
        mpTOS                    = 0;
        mpNews                   = 0;

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

        CGS_ASSERT_AT(mpNetworkManager != 0,
            "mpNetworkManager",
            __FILE__, 180 /* ARTIST 0x8256907C */);
        CGS_ASSERT_AT(mpNetworkManager->GetHostMigrationManager() != 0,
            "mpNetworkManager->GetHostMigrationManager()",
            __FILE__, 181 /* ARTIST 0x825690A4 */);
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
        CGS_ASSERT_AT(mpNetworkManager != 0,
            "mpNetworkManager",
            __FILE__, 212 /* ARTIST 0x82569150 */);
        CGS_ASSERT_AT(mpNetworkManager->GetHostMigrationManager() != 0,
            "mpNetworkManager->GetHostMigrationManager()",
            __FILE__, 213 /* ARTIST 0x82569178 */);
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

        if (mpTOS != 0)
        {
            CgsNetwork::ServerInterfaceDirtySock::MemFree(mpTOS, 0, 0);
            mpTOS = 0;
        }
        if (mpNews != 0)
        {
            CgsNetwork::ServerInterfaceDirtySock::MemFree(mpNews, 0, 0);
            mpNews = 0;
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
        mpNetworkManager         = 0;
        mpNetworkModule          = 0;
        mpTOS                    = 0;
        mpNews                   = 0;

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
        if (lpcBuffer != 0)
        {
            lpcBuffer += 2;

            const u8* lpu8Text = reinterpret_cast<const u8*>(lpcBuffer);
            if (lpu8Text[0] == 0xEF && lpu8Text[1] == 0xBB && lpu8Text[2] == 0xBF)
            {
                lpcBuffer += 3;
            }

            char* lpcEndTag = strstr(lpcBuffer, "%{");
            if (lpcEndTag != 0)
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
        mpNetworkModule->GetNetworkEventQueue()->AddEvent(lInstantFreeburnEvent);
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
                lEvent, BrnGameState::GameStateModuleIO::E_EVENT_FINISHED_SYNCING_PLAYERS);

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

            CGS_ASSERT(lpGamesComponent != 0, "lpGamesComponent");
            CGS_ASSERT(lpInput != 0, "lpInput");
            CGS_ASSERT(lpInput->GetActiveRaceCarInterface() != 0, "lpInput->GetActiveRaceCarInterface()");

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
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lShowLoadingScreen);
                meState = E_STATE_WAIT_FOR_LOADING_SCREEN;

                NetworkPlayerID lPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(
                        mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                    CGS_ASSERT(lpMenuData != 0, "lpMenuData");
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
                if (mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended())
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
                if (mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended())
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
            if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY)
            {
                const auto leModifyStatus = mpNetworkManager->GetServerInterface()
                    ->NativeServerInterface::GetStatus(CgsNetwork::E_COMPONENTS_GAMES);
                if (leModifyStatus == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
                {
                    mpNetworkManager->GetServerInterface()->NativeServerInterface::ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
                }

                CgsGui::GuiEventNetworkInGame lInGameEvent;
                lInGameEvent.maData[0] = true;
                mpNetworkModule->AddOutputGuiEvent(lInGameEvent);
                meState = E_STATE_COUNT;
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }
            break;

        case E_STATE_WAIT_SERVER_INTERFACE_ACTION:
            if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY)
            {
                if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
                    == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR)
                {
                    mpNetworkManager->GetServerInterface()->NativeServerInterface::ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
                }
                meState = E_STATE_COUNT;
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }
            break;

        case E_STATE_WAIT_FOR_LOADING_SCREEN:
        {
            BrnNetworkModuleIO::NetworkOutShowLoadingScreen lShowLoadingScreen;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lShowLoadingScreen);
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
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkUpdateAccountCompleteEvent);
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->ClearLastError();
                meState = E_STATE_COUNT;
                break;
            }

            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE:
            {
                BrnNetworkModuleIO::NetworkOutAccountUpdateComplete lNetworkUpdateAccountCompleteEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkUpdateAccountCompleteEvent);

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
                CGS_FAIL("Invalid component status\n");
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
                && mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(CgsNetwork::E_COMPONENTS_GAMES)
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
        if (mpNetworkModule != 0 && mpNetworkModule->GetGameStateToNetworkInterface() != 0)
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
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkLaunchEvent);

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

// FOLDED FROM BrnNetworkStateManager_wN1_02.cpp
#include <xtl.h>
// ===================================================================================
// BrnNetwork::StateManager -- part 2: log-in, launching, news/TOS download, the
// free-burn lobby helpers and the matchmaking / suspension completion callbacks.
//
// Every manager and component is reached through the owning BrnNetworkManager's
// accessors; every StateManager member by name.
//
// Network-out records come from BrnNetworkOutEventTypeDefs.h, game events from
// BrnGameEvents.h.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                                // CgsContainers::BitArray (scalp slots)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // the memory checks' log line
#include "GameShared/GameClasses/Development/CgsStrStream.h"                              // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                               // GuiEventNetworkConnected / InGame / InGameFailed / Launched
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"                // CgsGui::GuiEventAddLocalisedTextPointer
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                          // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h"                      // CgsNetwork::VersionDisplay
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                      // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                      // CgsNetwork::NetworkPlayer
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"               // CgsNetwork::HostMigrationManager
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                 // CgsNetwork::StartTimeManager
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"         // EComponents
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"   // CgsNetwork::EServerInterfaceError
#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"   // CgsNetwork::BuddyManagerBase::CancelInvites
#include "GameSource/GameState/BrnGameEvents.h"                                           // ChangePlayerCarEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                    // EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                     // GuiEventNetworkCustomMatchResults / NetworkConnect / ...
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"                           // BrnGui::GuiEventNetworkCreateGame
#include "GameSource/Network/BrnNetworkModule.h"                                          // BrnNetworkModule
#include "GameSource/Network/BrnNetworkModuleIO.h"                                        // NetworkEventQueue
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                // NetworkOut* records
#include "GameSource/Network/BrnNetworkManager.h"                                         // accessors + the login / camera / buddy manager types
#include "GameSource/Network/BrnServerInterface.h"                                        // BrnServerInterface
#include "GameSource/Network/BrnNetworkGameParams.h"                                      // BrnNetwork::GameParams
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                                  // BrnNetwork::PlayerMenuData
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"           // TOSBufferSize
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"
#include "GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h"
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                       // BrnNetwork::PlayerInfoData
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                    // BrnNetwork::PlayerParams
#include "GameSource/Resource/BrnResourceAllocator.h"                                     // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"          // GameStateToNetworkInterface
#include <cstring>   // std::memcpy / std::strlen / std::strncpy

// Platform sign-in state query (provided by the platform layer).

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkStateManager.cpp".

namespace BrnNetwork
{
    namespace
    {
        namespace GsmIO = BrnGameState::GameStateModuleIO;

        // The compressed camera picture is a 160 x 120 DXT1 image: 160 * 120 / 2 bytes.
        const s32 KI_COMPRESSED_CAMERA_PIC_SIZE = 9600;

        // XUserGetSigninState: signed in to the online service.
        const u32 KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE = 2;

        // ConnApiControl selector for the downloaded connection mangle setting.
        const s32 KI_CONNAPI_CONTROL_MANGLE = 'mngl';

        // ---- GUI event payload values ------------------------------------------------------
        // The committed GUI event types for these ids are opaque byte records; the values are
        // the reference enumerators of the payload word.

        // GuiEventNetworkLaunched::ELaunchedStatus.
        const s32 KI_LAUNCHED_STATUS_SUCCESS = 0;
        const s32 KI_LAUNCHED_STATUS_FAILED  = 6;

        // GuiEventNetworkInGameFailed::EInGameFailReason.
        const s32 KI_IN_GAME_FAIL_REASON_FULL                          = 0;
        const s32 KI_IN_GAME_FAIL_REASON_STARTED                       = 1;
        const s32 KI_IN_GAME_FAIL_REASON_UNKNOWN_GAME                  = 2;
        const s32 KI_IN_GAME_FAIL_REASON_CREATE_GAME_FOR_INVITE_FAILED = 3;
        const s32 KI_IN_GAME_FAIL_REASON_COUNT                         = 4;

        template <typename TRecord>
        const CgsModule::Event* AsEvent(const TRecord* lpRecord)
        {
            return reinterpret_cast<const CgsModule::Event*>(lpRecord);
        }

        template <typename TGuiEvent>
        void SetPayloadWord(TGuiEvent* lpEvent, s32 liValue)
        {
            CGS_STATIC_ASSERT((sizeof(lpEvent->maData) == sizeof(s32)), "4-byte GUI payload");
            std::memcpy(lpEvent->maData, &liValue, sizeof(s32));
        }
    }

    // -----------------------------------------------------------------------------------
    // SearchFinishedCallback
    // A failed custom-match search clears the games component's error and publishes an
    // empty result list.
    // -----------------------------------------------------------------------------------
    void StateManager::SearchFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);

        if (!lbSuccess)
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkCustomMatchResults lResultsEvent;
            lResultsEvent.miNumGames = 0;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lResultsEvent);
        }
    }

    // -----------------------------------------------------------------------------------
    // ResumeFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::ResumeFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT_AT(lbSuccess,
            "lbSuccess", __FILE__, 3612 /* ARTIST 0x825632D8 */);

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->meState = E_STATE_COUNT;
        lpStateManager->OnFullConnection();
    }

    // -----------------------------------------------------------------------------------
    // SuspensionFinishedCallback
    // Tell the GUI the suspension is done; when it was a suspend to leave the game, also
    // publish why the player left.
    // -----------------------------------------------------------------------------------
    void StateManager::SuspensionFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT_AT(lbSuccess,
            "lbSuccess",
            __FILE__, 3579 /* ARTIST 0x82569520 */);

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);

        CgsGui::GuiEvent<46> lSuspendedEvent;
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lSuspendedEvent);

        if (lpStateManager->meState == E_STATE_WAIT_SUSPENSION_LEAVING_GAME)
        {
            BrnNetworkModuleIO::NetworkOutPlayerLeftGame lLeftGameEvent;
            lLeftGameEvent.meLeftGameReason = lpStateManager->meLeftReason;
            lLeftGameEvent.meKickReason     = lpStateManager->meKickReason;
            lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lLeftGameEvent);
        }

        lpStateManager->meState = E_STATE_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // HostChangedCallback
    // Registered with the host-migration flow: publish whether the local player is the new
    // host and whether this is the first host of the session (no previous host).
    // -----------------------------------------------------------------------------------
    void StateManager::HostChangedCallback(const CgsSystem::TimerStatus* lpTimerStatus,
                                           NetworkPlayerID lOldHostID,
                                           NetworkPlayerID lNewHostID,
                                           void* lpUserData)
    {
        (void)lpTimerStatus;

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT_AT(lpStateManager != 0,
            "lpStateManager",
            __FILE__, 5589 /* ARTIST 0x82563354 */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkManager != 0,
            "lpStateManager->mpNetworkManager",
            __FILE__, 5590 /* ARTIST 0x8256337C */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkManager->GetPlayerManager() != 0,
            "lpStateManager->mpNetworkManager->GetPlayerManager()",
            __FILE__, 5591 /* ARTIST 0x825633A4 */);

        const NetworkPlayerID lLocalPlayerID =
            lpStateManager->mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
        const bool lbHaveLocalPlayer = (lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID);

        BrnNetworkModuleIO::NetworkOutHostChangedEvent lHostChangedEvent;
        lHostChangedEvent.mbIsLocalPlayerNowHost = lbHaveLocalPlayer && (lNewHostID == lLocalPlayerID);
        lHostChangedEvent.mbIsFirstHost          = (lOldHostID == CgsNetwork::K_INVALID_PLAYER_ID);

        CGS_ASSERT_AT(lpStateManager->mpNetworkModule != 0,
            "lpStateManager->mpNetworkModule",
            __FILE__, 5611 /* ARTIST 0x82563420 */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkModule->GetNetworkEventQueue() != 0,
            "lpStateManager->mpNetworkModule->GetNetworkEventQueue()",
            __FILE__, 5612 /* ARTIST 0x8256344C */);
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lHostChangedEvent);
    }

    // -----------------------------------------------------------------------------------
    // GetCompressedCameraPicCallback
    // The compressor finished the local camera picture: hand the DXT1 pixels on.
    // -----------------------------------------------------------------------------------
    void StateManager::GetCompressedCameraPicCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT_AT(lpPixels != 0,
            "lpPixels",
            __FILE__, 5632 /* ARTIST 0x825634B0 */);
        CGS_ASSERT_AT(lpUserData != 0,
            "lpUserData",
            __FILE__, 5633 /* ARTIST 0x825634D4 */);

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT_AT(lpStateManager != 0,
            "lpStateManager",
            __FILE__, 5635 /* ARTIST 0x825634F0 */);

        BrnNetworkModuleIO::NetworkOutCamPicCompressedEvent lCompressedEvent;
        lCompressedEvent.miCompressedPixelSize = KI_COMPRESSED_CAMERA_PIC_SIZE;
        lCompressedEvent.meCompressedFormat    = renderengine::PIXELFORMAT_DXT1;
        lCompressedEvent.mpcCompressedPixels   = static_cast<char*>(lpPixels);
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lCompressedEvent);
    }

    // -----------------------------------------------------------------------------------
    // CreateInviteGame
    // Create a game for an outgoing invite: the default create-game parameters in the
    // online free-burn lobby mode (15), security 2, unranked.
    // -----------------------------------------------------------------------------------
    void StateManager::CreateInviteGame()
    {
        BrnGui::GuiEventNetworkCreateGame lCreateGameEvent;
        lCreateGameEvent.Construct();
        lCreateGameEvent.meGameMode = 15;
        lCreateGameEvent.meSecurity = 2;
        lCreateGameEvent.mbRanked   = false;

        CreateGame(&lCreateGameEvent, false);
    }

    // -----------------------------------------------------------------------------------
    // DoInstantFreeburn
    // Quick-join an unranked free-burn game (leaving the current one first if in one).
    // -----------------------------------------------------------------------------------
    void StateManager::DoInstantFreeburn()
    {
        mbForceStartFreeburnLobby = false;
        mpNetworkManager->GetMatchMakingManager()->QuickJoinGame(false, true, QuickJoinFinishedCallback, this);
        meState = E_STATE_WAIT_MATCHMAKING;
    }

    // -----------------------------------------------------------------------------------
    // RefreshFreeBurnLobbyGameMode
    // -----------------------------------------------------------------------------------
    void StateManager::RefreshFreeBurnLobbyGameMode()
    {
        CGS_ASSERT_AT(mbRefreshingFreeburnLobbyThisFrame,
            "mbRefreshingFreeburnLobbyThisFrame", __FILE__, 4647 /* ARTIST 0x8256A6E4 */);


        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        const bool lbInLobby = lpGames->IsLocalPlayerInGame() && !lpGames->IsGameStarted();
        if (lbInLobby && mpNetworkManager->GetPlayerManager()->GetNumberLocalPlayers() > 0)
        {
            StartFreeBurnLobbyGameMode(false, false, false, true);
        }
    }

    // -----------------------------------------------------------------------------------
    // StartTOSDownload
    // Download the terms of service from the server-configured URL (falling back to the
    // built-in one).
    // -----------------------------------------------------------------------------------
    void StateManager::StartTOSDownload()
    {
        const char* lpcUrlToUse =
            "http://sdevbesl01.online.ea.com/easo/editorial/common/2006/tos/tos.jsp?style=accept&lang=%25s&platform=xbl2";

        char lacTosURL[256] = {};

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        lpServerInterface->GetServerInfoComponent()->GetTosUrl(lacTosURL, sizeof(lacTosURL));
        if (lacTosURL[0] != '\0')
        {
            lpcUrlToUse = lacTosURL;
        }

        lpServerInterface->GetHttpComponent()->StartHttpsDownload(
            lpcUrlToUse,
            lpServerInterface->GetDownloadableConfigComponent()->TOSBufferSize(),
            CgsNetwork::ServerInterfaceHttp::KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT);

        meState = E_STATE_WAIT_DOWNLOAD_TOS;
    }

    // -----------------------------------------------------------------------------------
    // ReleaseNewsAndTOSDownload
    // Abort any download in flight and re-publish whichever texts are still held.
    // -----------------------------------------------------------------------------------
    void StateManager::ReleaseNewsAndTOSDownload()
    {
        if (meState == E_STATE_WAIT_DOWNLOAD_NEWS || meState == E_STATE_WAIT_DOWNLOAD_TOS)
        {
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->StopHttpDownload();
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->DestroyHttpsDownload();
            meState = E_STATE_COUNT;
        }

        CgsGui::GuiEventAddLocalisedTextPointer lTextEvent;

        if (mpTOS != 0)
        {
            lTextEvent.miMarker = 1;
            std::strncpy(lTextEvent.macId, "TOS_TEXT", CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
            lTextEvent.mpText = mpTOS;
            mpNetworkModule->AddOutputGuiEvent(lTextEvent);
        }

        if (mpNews != 0)
        {
            lTextEvent.miMarker = 1;
            std::strncpy(lTextEvent.macId, "NEWS_TEXT", CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
            lTextEvent.mpText = mpNews;
            mpNetworkModule->AddOutputGuiEvent(lTextEvent);
        }
    }

    // -----------------------------------------------------------------------------------
    // HaveAllSelectedACar
    // True once every player has either made a final car selection or is paused.
    // -----------------------------------------------------------------------------------
    bool StateManager::HaveAllSelectedACar()
    {
        GameParams lGameParams;
        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();

        NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lPlayerID));
            CGS_ASSERT_AT(lpMenuData != 0,
                "lpMenuData",
                __FILE__, 4242 /* ARTIST 0x82569E04 */);

            CgsNetwork::NetworkPlayer* lpPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
            if (!lpMenuData->mbFinalCarSelection
                && (lpPlayer == 0 || !lpPlayer->HasConnectionFailed()))
            {
                return false;
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------------------
    // StartLogIn
    // Kick the login manager off with the requested sign-in flavour.
    // -----------------------------------------------------------------------------------
    void StateManager::StartLogIn(LoginManagerBase::ESignInType leSignInType)
    {
        CGS_ASSERT(!mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                       || mpNetworkManager->GetLoginManager()->IsSigningIn()
                       || mpNetworkManager->GetServerInterface()->IsSuspended(),
                   "!mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn() || mpNetworkManager->GetLoginManager()->IsSigningIn() || mpNetworkManager->GetServerInterface()->IsSuspended()");

        if (meState != E_STATE_COUNT && meState != E_STATE_LOGIN)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "meState=" << static_cast<s32>(meState);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        CGS_ASSERT(( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() )
                       || ( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted() ),
                   "( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted() )");

        {
            CgsDev::DebugInterface lDebugInterface;
            lDebugInterface.GetDebugManager().ActivateComponent(mpNetworkManager->GetVersionDisplay());

            mpNetworkManager->GetLoginManager()->Start(leSignInType);
            meState = E_STATE_LOGIN;
        }
    }

    // -----------------------------------------------------------------------------------
    // HandleConnectEvent
    // The GUI asked to go online. Already logged in (and not mid sign-in or suspended):
    // report the connection straight back and carry on with any pending invite or
    // instant free-burn. Otherwise start the log-in.
    // -----------------------------------------------------------------------------------
    void StateManager::HandleConnectEvent(const BrnGui::GuiEventNetworkConnect* lpConnectEvent)
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        if (lpServerInterface->GetConnectionComponent()->IsLoggedIn()
            && !mpNetworkManager->GetLoginManager()->IsSigningIn()
            && !lpServerInterface->IsSuspended())
        {
            // FLAG: the console also logs "HandleConnectEvent() - Already connected.
            // SignInType=" <type> " Still Signing In=" <bool> "\n" to a network debug stream
            // that has no home in this tree; the log line is not reproduced.

            CgsGui::GuiEventNetworkConnected lConnectedEvent;
            lConnectedEvent.maData[0] = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsFirstLogin();
            lConnectedEvent.maData[1] = mpNetworkManager->GetServerInterface()->GetServerInfoComponent()->IsNewsUpdated();
            mpNetworkModule->AddOutputGuiEvent(lConnectedEvent);

            mpNetworkManager->GetNetworkInviteManager()->LogInComplete(true);
            mpNetworkManager->OnLogIn();

            if (mbDoInviteAfterCreate)
            {
                CreateInviteGame();
            }
            else if (mbInstantFreeburn)
            {
                DoInstantFreeburn();
            }
            return;
        }

        CGS_ASSERT_AT(lpConnectEvent != 0,
            "lpConnectEvent",
            __FILE__, 5785 /* ARTIST 0x82576034 */);

        // FLAG: the console logs "HandleConnectEvent() - NOT connected. SignInType=" <type>
        // " Still Signing In=" "true"/"false" "\n" to the same unhomed debug stream here.

        StartLogIn(static_cast<LoginManagerBase::ESignInType>(lpConnectEvent->miConnectType));
    }

    // -----------------------------------------------------------------------------------
    // CreateGameFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::CreateGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        GameParams lGameParams;

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != 0, "lpStateManager");

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT(lpStateManager->mpNetworkManager != 0, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetServerInterface() != 0,
                   "lpStateManager->mpNetworkManager->GetServerInterface()");

        CgsNetwork::ServerInterfaceGames* lpGames =
            lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent();
        const bool lbInGame = lbSuccess && lpGames->IsLocalPlayerInGame();

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            lGameParams.Prepare();
            lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            if (!GsmIO::IsOnlineFreeBurnLobby(static_cast<GsmIO::EGameModeType>(lGameParams.GameMode()))
                || lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame() > 1
                || lpStateManager->mbCreatedFromMenus)
            {
                lpStateManager->StartFreeBurnLobbyGameMode(false, false, false, false);
            }

            lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_CREATED, 0);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            lpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
        }
        else
        {
            s32 liFailReason;
            const s32 liErrorCode =
                lpStateManager->mpNetworkManager->GetServerInterface()->GetAndClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            if (liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL)
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            }
            else if (liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED || liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED)
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_STARTED;
            }
            else
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_COUNT;
            }

            if (lpStateManager->mbDoInviteAfterCreate)
            {
                lpStateManager->mbDoInviteAfterCreate = false;

                CGS_ASSERT(lpStateManager->mpNetworkManager != 0, "lpStateManager->mpNetworkManager");
                CGS_ASSERT(lpStateManager->mpNetworkManager->GetBuddyManager() != 0,
                           "lpStateManager->mpNetworkManager->GetBuddyManager()");
                lpStateManager->mpNetworkManager->GetBuddyManager()->CancelInvites();

                CGS_ASSERT(lpStateManager->mpNetworkManager != 0, "lpStateManager->mpNetworkManager");
                CGS_ASSERT(lpStateManager->mpNetworkManager->GetSuspensionManager() != 0,
                           "lpStateManager->mpNetworkManager->GetSuspensionManager()");
                SuspensionManager* lpSuspensionManager = lpStateManager->mpNetworkManager->GetSuspensionManager();
                if (!lpSuspensionManager->IsSuspending())
                {
                    lpStateManager->meState = E_STATE_WAIT_SUSPENSION;
                    // FLAG: the suspension type is folded away on the console (both types
                    // request the same server-interface suspend flags).
                    lpSuspensionManager->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                 SuspensionFinishedCallback, lpStateManager);
                }
                else
                {
                    lpStateManager->meState = E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND;
                }

                liFailReason = KI_IN_GAME_FAIL_REASON_CREATE_GAME_FOR_INVITE_FAILED;
            }

            CgsGui::GuiEventNetworkInGameFailed lInGameFailedEvent;
            SetPayloadWord(&lInGameFailedEvent, liFailReason);
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameFailedEvent);

            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            lpStateManager->mpNetworkManager->GetBuddyManager()->CancelInvites();

            // FLAG: the console logs "Invite sending might have failed as we failed to create
            // a game\n" to the unhomed network debug stream here.
        }

        lpStateManager->mbDoInviteAfterCreate = false;
    }

    // -----------------------------------------------------------------------------------
    // JoinGameFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::JoinGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT_AT(lpStateManager != 0,
            "lpStateManager",
            __FILE__, 3305 /* ARTIST 0x8256E5A0 */);

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT_AT(lpStateManager->mpNetworkManager != 0,
            "lpStateManager->mpNetworkManager",
            __FILE__, 3310 /* ARTIST 0x8256E5D0 */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkManager->GetServerInterface() != 0,
            "lpStateManager->mpNetworkManager->GetServerInterface()",
            __FILE__, 3311 /* ARTIST 0x8256E5F8 */);

        const bool lbInGame =
            lbSuccess && lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        lpStateManager->mpNetworkManager->GetNetworkInviteManager()->JoinGameComplete(lbInGame);

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            lpStateManager->StartFreeBurnLobbyGameMode(false, false, lpStateManager->mbForceStartFreeburnLobby, false);
            lpStateManager->mbForceStartFreeburnLobby = false;

            lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_JOINED, 0);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            lpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
            return;
        }

        s32 liFailReason;
        switch (lpStateManager->mpNetworkManager->GetServerInterface()->GetAndClearLastError(CgsNetwork::E_COMPONENTS_GAMES))
        {
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME:
            liFailReason = KI_IN_GAME_FAIL_REASON_UNKNOWN_GAME;
            break;
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL:
            liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            break;
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED:
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED:
            liFailReason = KI_IN_GAME_FAIL_REASON_STARTED;
            break;
        default:
            liFailReason = KI_IN_GAME_FAIL_REASON_COUNT;
            break;
        }

        CgsGui::GuiEventNetworkInGameFailed lInGameFailedEvent;
        SetPayloadWord(&lInGameFailedEvent, liFailReason);
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameFailedEvent);
    }

    // -----------------------------------------------------------------------------------
    // LeaveGameForOfflineGameFinishedCallback
    // Left the online game to go offline: clear the player images, drop the requested
    // camera feed, put the free-burn car back when leaving an online event, and suspend
    // the server interface. On failure tell the GUI and go idle.
    // -----------------------------------------------------------------------------------
    void StateManager::LeaveGameForOfflineGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT_AT(lpStateManager != 0,
            "lpStateManager",
            __FILE__, 3401 /* ARTIST 0x8256E7A0 */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkManager != 0,
            "lpStateManager->mpNetworkManager",
            __FILE__, 3402 /* ARTIST 0x8256E7C8 */);

        if (!lbSuccess)
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkLeavingGameFailed lLeavingFailedEvent;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lLeavingFailedEvent);

            lpStateManager->meState = E_STATE_COUNT;
            return;
        }

        lpStateManager->mpNetworkManager->OnLeaveGame();

        BrnGui::GuiEventNetworkPlayerImage lNetworkPlayerImageEvent;
        lNetworkPlayerImageEvent.mpTexture      = 0;
        lNetworkPlayerImageEvent.miTextureIndex = -1;
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lNetworkPlayerImageEvent);

        lpStateManager->mpNetworkManager->GetCamera()->RequestFeed(CgsNetwork::K_INVALID_PLAYER_ID);

        CGS_ASSERT_AT(lpStateManager->mpNetworkModule != 0,
            "lpStateManager->mpNetworkModule",
            __FILE__, 3418 /* ARTIST 0x8256E834 */);
        CGS_ASSERT_AT(lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface() != 0,
            "lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()",
            __FILE__, 3419 /* ARTIST 0x8256E860 */);

        if (!GsmIO::IsOnlineFreeBurnLobby(lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode()))
        {
            const GsmIO::EGameModeType leGameMode =
                lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();
            if (leGameMode >= GsmIO::E_MODE_ONLINE_MODE_START && leGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
            {
                GsmIO::ChangePlayerCarEvent lChangeCarEvent;
                lChangeCarEvent.mCarModelId         = lpStateManager->mpNetworkManager->GetFreeBurnCarID();
                lChangeCarEvent.mWheelModelId       = lpStateManager->mpNetworkManager->GetFreeBurnWheelID();
                lChangeCarEvent.mbResetPlayerCamera = false;
                lChangeCarEvent.mbKeepResetSection  = true;
                lpStateManager->mpNetworkModule->GetGameEventQueue()->AddGameEvent(lChangeCarEvent);
            }
        }

        lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_LEFT, 0);
        lpStateManager->mpNetworkManager->GetVersionDisplay()->SetGameServerGame(false);
        lpStateManager->mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

        lpStateManager->SuspendToLeaveGame(E_LEFT_GAME_REASON_LEFT, CgsNetwork::E_KICKREASON_COUNT);
    }

    // -----------------------------------------------------------------------------------
    // QuickJoinFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::QuickJoinFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != 0, "lpStateManager");

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT(lpStateManager->mpNetworkManager != 0, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetServerInterface() != 0,
                   "lpStateManager->mpNetworkManager->GetServerInterface()");

        const bool lbInGame =
            lbSuccess && lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            CGS_ASSERT(lpNetworkManager != 0, "lpNetworkManager");

            lpStateManager->StartFreeBurnLobbyGameMode(false, false, lpStateManager->mbForceStartFreeburnLobby, false);
            lpStateManager->mbForceStartFreeburnLobby = false;

            lpStateManager->mpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
        }
        else
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkCustomMatchResults lResultsEvent;
            lResultsEvent.miNumGames = 0;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lResultsEvent);
        }

        if (lpStateManager->mbInstantFreeburn)
        {
            lpStateManager->mbInstantFreeburn = false;

            BrnNetworkModuleIO::NetworkOutInstantFreeburnComplete lCompleteEvent;
            lCompleteEvent.mbSuccess = lbInGame;
            lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lCompleteEvent);

            if (!lbInGame)
            {
                lpStateManager->meState = E_STATE_WAIT_SUSPENSION;
                // FLAG: the suspension type is folded away on the console (both types
                // request the same server-interface suspend flags).
                lpStateManager->mpNetworkManager->GetSuspensionManager()->Suspend(
                    SuspensionManager::E_SUSPENSION_TYPE_OFFLINE, SuspensionFinishedCallback, lpStateManager);
            }
        }

        BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
        lInstantFreeburnEvent.mbIsDoingInstantFreeburn = false;
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(lInstantFreeburnEvent);
    }

    // -----------------------------------------------------------------------------------
    // UpdateLaunching
    // Tick the launch manager. Once launched: stop the time sync, start the game, arm host
    // migration, start team selection, tell the GUI / game state / network side, cache the
    // player ids, drop the per-lobby cached data and fix each player's car choice. A failed
    // launch goes idle and reports the failure.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateLaunching()
    {
        switch (mpNetworkManager->GetLaunchManager()->Update())
        {
        case LaunchManager::E_STATUS_IDLE:
        case LaunchManager::E_STATUS_BUSY:
        case LaunchManager::E_STATUS_WAITING_FOR_LAUNCH:
        case LaunchManager::E_STATUS_LAUNCHING:
            break;

        case LaunchManager::E_STATUS_LAUNCHED:
        {
            GameParams              lGameParams;
            BrnNetworkModuleIO::NetworkOutLaunchedEvent lNetworkLaunchedEvent;
            GsmIO::OnlineCarSelectEvent lCarSelectEvent;
            NetworkPlayerID         lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;

            CGS_ASSERT_AT(mpNetworkManager != 0,
                "mpNetworkManager",
                __FILE__, 4887 /* ARTIST 0x8256A83C */);

            lGameParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            mpNetworkManager->GetStartTimeManager()->StopSyncingTime();
            mpNetworkManager->OnGameStart();
            mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

            mpNetworkManager->GetHostMigrationManager()->Enable(
                mpNetworkManager->GetPlayerManager()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                mpNetworkManager->GetTimeManager()->GetU16FrameCount());

            CGS_ASSERT_AT(mpNetworkManager->GetTeamSelectionManager() != 0,
                "mpNetworkManager->GetTeamSelectionManager()",
                __FILE__, 4903 /* ARTIST 0x8256A8F4 */);
            meState = E_STATE_WAIT_TEAM_SELECTION;
            mpNetworkManager->GetTeamSelectionManager()->StartTeamSelection(
                lGameParams.GameMode(), TeamSelectionFinishedCallback, this);

            CgsGui::GuiEventNetworkLaunched lLaunchedEvent;
            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_SUCCESS);
            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);

            mpNetworkModule->GetGameEventQueue()->AddGameEvent(lCarSelectEvent);

            lNetworkLaunchedEvent.mbSuccessfulLaunch  = true;
            lNetworkLaunchedEvent.miVehicleClassLimit = lGameParams.VehicleLevelLimit();
            lNetworkLaunchedEvent.mbHostChoiceCarAndNotHost =
                lGameParams.VehicleChoice() == 1
                && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost();
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkLaunchedEvent);

            mpNetworkManager->GetLaunchManager()->Stop();
            mpNetworkModule->CacheNetworkPlayerIDsAtGameStart();

            mMarkedManData.mbValid  = false;
            mNewCarData.mbValid     = false;
            mDistrictData.mbValid   = false;
            mCarColourData.mbValid  = false;
            mFeverData.mbValid      = false;
            mbSetNotPlaying         = false;

            while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
            {
                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT_AT((lpMenuData->mFreeBurnCarId != 0),
                    "(lpMenuData->mFreeBurnCarId != 0)",
                    __FILE__, 4934 /* ARTIST 0x8256AA40 */);

                if (lpMenuData->mCarId == 0)
                {
                    lpMenuData->mCarId   = lpMenuData->mFreeBurnCarId;
                    lpMenuData->mWheelId = lpMenuData->mFreeBurnWheelId;
                }
            }
            break;
        }

        case LaunchManager::E_STATUS_ERROR:
        {
            BrnNetworkModuleIO::NetworkOutLaunchedEvent lNetworkLaunchedEvent;

            meState = E_STATE_COUNT;

            CgsGui::GuiEventNetworkLaunched lLaunchedEvent;
            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_FAILED);
            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);

            // mbHostChoiceCarAndNotHost is not written on this path.
            lNetworkLaunchedEvent.mbSuccessfulLaunch  = false;
            lNetworkLaunchedEvent.miVehicleClassLimit = 9;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkLaunchedEvent);

            mpNetworkManager->GetLaunchManager()->Stop();
            mpNetworkModule->ClearCachedNetworkPlayerIDsAtGameEnd();
            mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            break;
        }

        default:
            CGS_FAIL_AT(
                "Invalid launch state",
                __FILE__, 4981 /* ARTIST 0x8256AB38 */);
            break;
        }
    }

    // -----------------------------------------------------------------------------------
    // StartNewsDownload
    // Download the news from the server-configured URL. The built-in fallback is the same
    // literal the terms-of-service download uses.
    // -----------------------------------------------------------------------------------
    void StateManager::StartNewsDownload()
    {
        const char* lpcUrlToUse =
            "http://sdevbesl01.online.ea.com/easo/editorial/common/2006/tos/tos.jsp?style=accept&lang=%25s&platform=xbl2";

        char lacNewsURL[256] = {};

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        lpServerInterface->GetServerInfoComponent()->GetNewsUrl(lacNewsURL, sizeof(lacNewsURL));
        if (lacNewsURL[0] != '\0')
        {
            lpcUrlToUse = lacNewsURL;
        }

        lpServerInterface->GetHttpComponent()->StartHttpsDownload(
            lpcUrlToUse,
            lpServerInterface->GetDownloadableConfigComponent()->NewsBufferSize(),
            CgsNetwork::ServerInterfaceHttp::KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT);

        meState = E_STATE_WAIT_DOWNLOAD_NEWS;
    }

    // -----------------------------------------------------------------------------------
    // UpdateNewsAndTOSDownload
    // Start whichever download is queued once the HTTP component is idle, or collect the
    // one in flight: a finished download is copied into mpNews / mpTOS and handed to the
    // GUI as localised text; a failure is reported as the matching FAILED event.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateNewsAndTOSDownload()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        if (meState == E_STATE_WAIT_DOWNLOAD_NEWS_IDLE)
        {
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
            {
                StartNewsDownload();
            }
            return;
        }

        if (meState == E_STATE_WAIT_DOWNLOAD_TOS_IDLE)
        {
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
            {
                StartTOSDownload();
            }
            return;
        }

        typedef BrnGui::GuiEventNetworkNewsAndTOS NewsAndTOSEvent;
        NewsAndTOSEvent::EEventType leEventType = NewsAndTOSEvent::E_EVENT_TYPE_COUNT;
        bool                        lbFailed    = false;

        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
        {
            char* lpcBuffer = reinterpret_cast<char*>(lpServerInterface->GetHttpComponent()->GetHttpsDownloadBuffer());
            if (meState == E_STATE_WAIT_DOWNLOAD_NEWS)
            {
                lpcBuffer = StripWebOfferControlCodes(lpcBuffer);
                lbFailed  = (lpcBuffer == 0);
            }

            if (!lbFailed)
            {
                while (*lpcBuffer == '\n')
                {
                    ++lpcBuffer;
                }

                CgsUnicode::CgsUtf8** lppText;
                const char*           lpcTextID;
                if (meState == E_STATE_WAIT_DOWNLOAD_NEWS)
                {
                    lppText     = &mpNews;
                    lpcTextID   = "NEWS_TEXT";
                    leEventType = NewsAndTOSEvent::E_EVENT_TYPE_RETRIEVED_NEWS;
                }
                else
                {
                    lppText     = &mpTOS;
                    lpcTextID   = "TOS_TEXT";
                    leEventType = NewsAndTOSEvent::E_EVENT_TYPE_RETRIEVED_TOS;
                }

                if (*lppText != 0)
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(*lppText, 0, 0);
                    *lppText = 0;
                }
                *lppText = static_cast<CgsUnicode::CgsUtf8*>(CgsNetwork::ServerInterfaceDirtySock::MemAlloc(
                    mpNetworkManager->GetServerInterface()->GetHttpComponent()->GetHttpDownloadSize(), 0, 0));
                CgsUnicode::Copy(*lppText, reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpcBuffer));

                CgsGui::GuiEventAddLocalisedTextPointer lTextEvent;
                lTextEvent.miMarker = 0;
                // The bounded copy's own "String too long" check (inlined from the string utilities).
                if (!(std::strlen(lpcTextID) < static_cast<u32>(CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH)))
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "String too long: " << lpcTextID;
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                std::strncpy(lTextEvent.macId, lpcTextID, CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
                lTextEvent.mpText = *lppText;
                mpNetworkModule->AddOutputGuiEvent(lTextEvent);
            }
        }
        else if (mpNetworkManager->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_ERROR)
        {
            mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_HTTP);
            lbFailed = true;
        }

        if (lbFailed)
        {
            leEventType = (meState == E_STATE_WAIT_DOWNLOAD_NEWS) ? NewsAndTOSEvent::E_EVENT_TYPE_FAILED_NEWS
                                                                 : NewsAndTOSEvent::E_EVENT_TYPE_FAILED_TOS;
        }

        if (leEventType != NewsAndTOSEvent::E_EVENT_TYPE_COUNT)
        {
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->DestroyHttpsDownload();

            NewsAndTOSEvent lNewsAndTOSEvent;
            lNewsAndTOSEvent.meEventType = leEventType;
            mpNetworkModule->AddOutputGuiEvent(lNewsAndTOSEvent);

            meState = E_STATE_COUNT;
        }
    }

    // -----------------------------------------------------------------------------------
    // OnFullConnection
    // Fully online: request the local player's stats, pass the downloaded mangle setting to
    // the connection API and drop any requested camera feed.
    // -----------------------------------------------------------------------------------
    void StateManager::OnFullConnection()
    {
        // The console's PlayerInfoData constructor runs Prepare itself; the tree's does not yet.
        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        CGS_ASSERT_AT(lPlayerInfo.GetID() > 0,
            "lPlayerInfo.GetID() > 0",
            __FILE__, 5045 /* ARTIST 0x82560E1C */);

        mpNetworkManager->GetStatsManager()->RequestPlayerStats(lPlayerInfo.GetName());

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        ConnApiControl(lpServerInterface->GetConnAPIRef(), KI_CONNAPI_CONTROL_MANGLE,
                       (lpServerInterface->GetDownloadableConfigComponent()->NumDemanglePlayers() != 0) ? 1 : 0,
                       0, 0);

        mpNetworkManager->GetCamera()->RequestFeed(CgsNetwork::K_INVALID_PLAYER_ID);
    }

    // -----------------------------------------------------------------------------------
    // StartFreeBurnLobbyGameMode
    // Post the start-network-game event for the free-burn lobby (every player's free-burn
    // car, colours, team, host slot and fever flag), then the round start unless this is a
    // refresh. A host that starts the lobby because a player joined, while not already in
    // the lobby, also tells the network side.
    // FLAG: when the local player id is invalid the console dumps the game component and
    // player manager state to the unhomed network debug stream just before the assert; the
    // dump is not reproduced.
    // -----------------------------------------------------------------------------------
    void StateManager::StartFreeBurnLobbyGameMode(bool lbStartingAfterJoin, bool lbStartingAfterOnlineEvent,
                                                  bool lbForceStartFreeburnLobby, bool lbRefreshOnly)
    {
        CGS_ASSERT(!lbRefreshOnly || (lbStartingAfterJoin && lbStartingAfterOnlineEvent && lbForceStartFreeburnLobby)==false,
                   "!lbRefreshOnly || (lbStartingAfterJoin && lbStartingAfterOnlineEvent && lbForceStartFreeburnLobby)==false");

        PlayerParams                 lPlayerParams;
        GameParams                   lGameParams;
        GsmIO::StartNetworkGameEvent lEvent;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.Clear();
        lEvent.muRandomSeedForGame        = 0;
        lEvent.meGameMode                 = GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
        lEvent.miNumRaceCars              = mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                                                CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS);
        lEvent.mbRefreshOnly              = lbRefreshOnly;
        lEvent.mbForceStartFreeburnLobby  = lbForceStartFreeburnLobby;
        lEvent.mbIsRanked                 = false;
        lEvent.mfTimeLimit                = 0.0f;
        lEvent.miNumRounds                = 1;
        lEvent.mbIsTrafficOn              = true;
        lEvent.mbIsTrafficCheckingOn      = true;
        lEvent.mbRedTeamHaveInfiniteBoost = false;
        lEvent.meBoostType                = 0;
        lEvent.miNumRunnerCrashes         = 3;

        // The reference's inline StartNetworkGameEvent::SetLocalNetworkPlayerID (assert, then
        // store); BrnGameEvents.h does not declare it yet.
        const NetworkPlayerID lLocalNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
        CGS_ASSERT(lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        lEvent.mLocalNetworkPlayerID = lLocalNetworkPlayerID;

        s32             liGridPosition = 0;
        NetworkPlayerID lPlayerID      = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
        {
            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);

            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT((lpMenuData->mFreeBurnCarId != 0), "(lpMenuData->mFreeBurnCarId != 0)");

            lEvent.SetPlayerData(liGridPosition, lPlayerID,
                                 lpMenuData->mFreeBurnCarId, lpMenuData->mFreeBurnWheelId,
                                 lpMenuData->mu16FreeburnCarColourIndex, lpMenuData->mu16FreeburnPaintFinishIndex,
                                 lpMenuData->mfUnknown4C,
                                 lPlayerParams.GetPlayerTeam(),
                                 lPlayerID == mpNetworkManager->GetPlayerManager()->GetHostPlayerID(),
                                 lPlayerParams.HasFever());
            ++liGridPosition;
        }

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        lEvent.mbIsStartingGameAfterPlayerJoin =
            lpGames->IsLocalPlayerInGame() && lpGames->IsLocalPlayerHost() && lbStartingAfterJoin;
        lEvent.mbIsStartingFreeburnLobbyAfterOnlineEvent = lbStartingAfterOnlineEvent;
        mpNetworkModule->GetGameEventQueue()->AddGameEvent(lEvent);

        if (!lbRefreshOnly)
        {
            GsmIO::StartNetworkRoundEvent lRoundEvent;
            lRoundEvent.miNumLandmarksInRound = 0;
            mpNetworkModule->GetGameEventQueue()->AddGameEvent(lRoundEvent);
        }

        CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface() != 0,
                   "mpNetworkModule->GetGameStateToNetworkInterface()");

        if (lEvent.mbIsStartingGameAfterPlayerJoin
            && !GsmIO::IsOnlineFreeBurnLobby(mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode()))
        {
            BrnNetworkModuleIO::NetworkOutStartingGameDueToPlayerJoin lStartingGameDueToPlayerJoin;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lStartingGameDueToPlayerJoin);
        }
    }

    // -----------------------------------------------------------------------------------
    // OutputPlayerTexture
    // Push the pictures the GUI asked for (meOutputPlayerTexture) through the network
    // manager's texture slots: the requested camera picture or gamer picture, the round
    // winner's photo finish, the per-car takedown scalps, or a gamer picture for the
    // photo booth / licence. Every mode except the player image turns the game camera off
    // afterwards.
    // -----------------------------------------------------------------------------------
    void StateManager::OutputPlayerTexture()
    {
        typedef BrnGui::GuiEventNetworkOutputPlayerTexture OutputPlayerTextureEvent;

        if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_PLAYER_IMAGE)
        {
            // The console evaluates IsLocalPlayerInGame here and enables the camera either way.
            mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();
            mpNetworkManager->GetCamera()->SetGameEnabled(true);

            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetCamera()->GetRequestedCameraPicture();
            if (lpTextureToDisplay == 0)
            {
                lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                    mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            }
            mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
            for (s32 liPlayerIndex = 1; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_WINNER_IMAGE)
        {
            CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
            CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");
            CgsNetwork::ServerInterfaceGames* lpGamesComponent = mpNetworkManager->GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != 0, "lpGamesComponent");

            s32 liDisplayedImageIndex = 0;
            if (lpGamesComponent->IsLocalPlayerInGame())
            {
                bool                              lbAlreadyShowingPhotoFinish = false;
                const CgsNetwork::NetworkTexture* lpTextureToDisplay          = 0;
                if (mNetworkPlayerIDToOutput != CgsNetwork::K_INVALID_PLAYER_ID)
                {
                    CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
                    if (lpPlayerManager->GetPlayerByID(mNetworkPlayerIDToOutput) != 0
                        || lpPlayerManager->IsLocalPlayer(mNetworkPlayerIDToOutput))
                    {
                        lpTextureToDisplay = mpNetworkManager->GetNetworkImageManager()->GetPhotoFinishImageByRoundWinner(
                            mNetworkPlayerIDToOutput, &lbAlreadyShowingPhotoFinish);
                    }
                    if (lpTextureToDisplay == 0)
                    {
                        lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(mNetworkPlayerIDToOutput);
                    }
                }
                if (!lbAlreadyShowingPhotoFinish)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
                }
                liDisplayedImageIndex = 1;
            }
            else
            {
                const CgsNetwork::NetworkTexture* lpTextureToDisplay =
                    mpNetworkManager->GetGamerPictureManager()->GetLocalPlayerGamerPictureTexture();
                if (lpTextureToDisplay != 0)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
                    liDisplayedImageIndex = 1;
                }
            }

            for (s32 liPlayerIndex = liDisplayedImageIndex; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_SCALP_IMAGE)
        {
            // BitArray's own index tripwires (inlined on the console) cannot fire here: the set
            // index is asserted below E_ACTIVE_RACE_CAR_INDEX_COUNT and the clear loop stops at 8.
            CgsContainers::BitArray<8u> lRaceCarsWithScalps;
            lRaceCarsWithScalps.UnSetAll();

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = mpNetworkManager->GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != 0, "lpGamesComponent");

            if (lpGamesComponent->IsLocalPlayerInGame())
            {
                NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
                {
                    if (!lpGamesComponent->IsPlayerInGame(lPlayerID))
                    {
                        continue;
                    }

                    const CgsNetwork::NetworkTexture* lpTextureToDisplay = 0;
                    const NetworkPlayerID lVictimID = mpNetworkManager->GetNetworkImageManager()->GetMugshotVictimID(lPlayerID);
                    if (lVictimID != CgsNetwork::K_INVALID_PLAYER_ID)
                    {
                        lpTextureToDisplay = mpNetworkManager->GetNetworkImageManager()->GetMugshotImageByAggressor(lPlayerID);
                        if (lpTextureToDisplay == 0)
                        {
                            lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(lVictimID);
                        }
                    }

                    const EActiveRaceCarIndex leActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lPlayerID);
                    if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    {
                        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                        lRaceCarsWithScalps.SetBit(static_cast<u32>(leActiveRaceCarIndex));
                        mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, leActiveRaceCarIndex);
                    }
                }
            }

            for (s32 liPlayerIndex = 0; liPlayerIndex < 8; ++liPlayerIndex)
            {
                if (!lRaceCarsWithScalps.IsBitSet(static_cast<u32>(liPlayerIndex)))
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, liPlayerIndex);
                }
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_GAMERPIC)
        {
            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            if (lpTextureToDisplay != 0)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
            }
            else
            {
                CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0, "mpNetworkModule->GetNetworkEventQueue()");
                BrnNetworkModuleIO::NetworkOutNoPhotoBoothGamerPic lNoPhotoBoothGamerPicEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNoPhotoBoothGamerPicEvent);
            }
            for (s32 liPlayerIndex = 1; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_GAMERPIC_LICENCE)
        {
            for (s32 liPlayerIndex = 0; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, liPlayerIndex);
            }

            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            if (lpTextureToDisplay != 0)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 1);
            }
            else
            {
                CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0, "mpNetworkModule->GetNetworkEventQueue()");
                BrnNetworkModuleIO::NetworkOutNoPhotoBoothGamerPic lNoPhotoBoothGamerPicEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNoPhotoBoothGamerPicEvent);
            }
        }

        if (meOutputPlayerTexture != OutputPlayerTextureEvent::E_OUTPUT_PLAYER_IMAGE)
        {
            mpNetworkManager->GetCamera()->SetGameEnabled(false);
        }
    }

    // -----------------------------------------------------------------------------------
    // UpdateLogin
    // Tick the login manager. When it finishes, go idle and tell the invite manager.
    // Signed in: prepare VoIP (the game sends the headset status), tell the GUI and the
    // network side, apply the account's telemetry choice, register the local gamer picture,
    // check the downloaded fever list, then carry on with the stats request, the pending
    // invite game or the instant free-burn, and suspend again when the sign-in asked for it.
    // Not signed in: report the disconnection (a silent sign-in stays quiet), cancel the
    // pending invites and tell the network side instant free-burn is off. Memory checks
    // bracket every step.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateLogin(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        if (mpNetworkManager->GetLoginManager()->Update(lpInput))
        {
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

            const bool lbLoggedIn = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn();
            const LoginManagerBase::ESignInType leSignInType = mpNetworkManager->GetLoginManager()->GetCompletedSignInType();

            meState = E_STATE_COUNT;
            mpNetworkManager->GetNetworkInviteManager()->LogInComplete(lbLoggedIn);

            if (lbLoggedIn)
            {
                // FLAG: the console logs "Finished Signing In. SignInType=" <type> "\n" to the
                // unhomed network debug stream here; the log line is not reproduced.
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CGS_ASSERT(mpNetworkManager->GetVoIPManager()->Prepare(mpNetworkManager->GetPlayerManager(), mpNetworkManager->GetTimeManager(), mpNetworkManager->GetServerInterface(), mpNetworkManager->GetBuddyManager(), true ),
                           "mpNetworkManager->GetVoIPManager()->Prepare(mpNetworkManager->GetPlayerManager(), mpNetworkManager->GetTimeManager(), mpNetworkManager->GetServerInterface(), mpNetworkManager->GetBuddyManager(), true )");
                mpNetworkManager->GetVoIPManager()->SetGameSendingHeadsetStatus(true);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CgsGui::GuiEventNetworkConnected lConnectedEvent;
                lConnectedEvent.maData[0] = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsFirstLogin();
                lConnectedEvent.maData[1] = mpNetworkManager->GetServerInterface()->GetServerInfoComponent()->IsNewsUpdated();
                mpNetworkModule->AddOutputGuiEvent(lConnectedEvent);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                // The console's PlayerInfoData constructor runs Prepare itself; the tree's does not yet.
                PlayerInfoData lPlayerInfo;
                lPlayerInfo.Prepare();
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CGS_ASSERT(lPlayerInfo.Prepare(), "lPlayerInfo.Prepare()");
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                BrnNetworkModuleIO::NetworkOutLocalPlayerConnected lLocalPlayerConnectedEvent;
                lLocalPlayerConnectedEvent.mPlayerID = lPlayerInfo.GetID();
                CGS_ASSERT(lPlayerInfo.GetID() != CgsNetwork::K_INVALID_PLAYER_ID,
                           "lPlayerInfo.GetID() != CgsNetwork::K_INVALID_PLAYER_ID");
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lLocalPlayerConnectedEvent);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                bool lbAgreeToShareInfoEA;
                bool lbAgreeToShareInfoPartners;
                bool lbTelemetryEnable;
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetAccountSettings(
                    &lbAgreeToShareInfoEA, &lbAgreeToShareInfoPartners, &lbTelemetryEnable);
                mpNetworkManager->GetServerInterface()->GetTelemetryComponent()->EnableTemetry(lbTelemetryEnable);

                lPlayerInfo.Prepare();
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                CGS_ASSERT(lPlayerInfo.GetID() > 0, "lPlayerInfo.GetID() > 0");
                mpNetworkManager->GetGamerPictureManager()->AddPlayer(lPlayerInfo.GetID());

                if (mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()->IsGamertagInFeverList(lPlayerInfo.GetName()))
                {
                    BrnNetworkModuleIO::NetworkOutCaughtFever lCaughtFeverEvent;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lCaughtFeverEvent);
                    mpNetworkManager->SetIsDeveloper(true);
                }

                mpNetworkManager->OnLogIn();

                if (leSignInType == LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
                {
                    mpNetworkManager->GetStatsManager()->RequestPlayerStats(lPlayerInfo.GetName());
                }
                else if (leSignInType == LoginManagerBase::E_SIGN_IN_TYPE_FREE_BURN_LOBBY && mbDoInviteAfterCreate)
                {
                    CreateInviteGame();
                    OnFullConnection();
                }
                else
                {
                    if (mbInstantFreeburn)
                    {
                        DoInstantFreeburn();
                    }
                    OnFullConnection();
                }
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                if (mbSuspendAfterSignIn)
                {
                    // FLAG: the suspension type is folded away on the console (both types
                    // request the same server-interface suspend flags).
                    mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                      SuspensionFinishedCallback, this);
                    mbSuspendAfterSignIn = false;
                    meState              = E_STATE_WAIT_SUSPENSION;
                }
            }
            else if (leSignInType != LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.macReason[0] = '\0';
                lDisconnectedEvent.meLastError  = mpNetworkManager->GetServerInterface()->GetLastError(CgsNetwork::E_COMPONENTS_CONNECTION);
                if (lDisconnectedEvent.meLastError == CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE
                    && XUserGetSigninState(static_cast<u32>(mpNetworkManager->GetActiveControllerPort())) != KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE)
                {
                    lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_NOT_SIGNED_IN;
                }
                mpNetworkModule->AddOutputGuiEvent(lDisconnectedEvent);

                mpNetworkManager->GetBuddyManager()->CancelInvites();
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }

            mbSuspendAfterSignIn = false;
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

            if (!lbLoggedIn)
            {
                BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
                lInstantFreeburnEvent.mbIsDoingInstantFreeburn = false;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lInstantFreeburnEvent);
            }
        }

        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
    }
} // namespace BrnNetwork

// FOLDED FROM BrnNetworkStateManager_wN1_03.cpp
// ===================================================================================
// BrnNetwork::StateManager -- part 3: the network-event and game-action processors.
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStateManager_wN1_03.cpp
//
// Both are called by UpdateEvents once per frame:
//   ProcessNetworkEvents     drains the network-IN event queue (the requests the game and
//                            the GUI post to the network module: invites, account settings,
//                            showtime / skillz / freeburn-challenge broadcasts, camera and
//                            gamer-card requests, ...) and routes each one to the manager
//                            that owns it.
//   ProcessGameStateActions  drains the game-action queue the game state hands the network
//                            (mode start / stop, results, car choices, invites) and moves the
//                            session state machine accordingly.
// Every event type the switch does not name is ignored by ProcessNetworkEvents; an action
// type ProcessGameStateActions does not name fires the "Unknown GameAction" assert.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT, BeginAssert / FireAssert / EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                // CgsDev::StrStream (streamed assert messages)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                            // GetFirstEvent / GetNextEvent / AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                                    // CgsSystem::Time(f32)
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                        // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                   // CgsNetwork::StartTimeManager
#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"          // CgsNetwork::VoIPManager
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"             // EComponents / EStatus
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // ServerInterfacePlayerInfo
#include "GameSource/GameState/BrnGameActions.h"                                            // the game-action records
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                      // EGameModeType, EPlayerTeam
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"                   // GameModeParams::GetGameModeType
#include "GameSource/Network/BrnNetworkModule.h"                                            // BrnNetworkModule, BrnNetworkManager
#include "GameSource/Network/BrnNetworkModuleIO.h"                                          // NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                                   // network IN records
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                  // network OUT records
#include "GameSource/Network/BrnNetworkPlayer.h"                                            // BrnNetworkPlayer
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                                    // BrnNetwork::PlayerMenuData
#include "GameSource/Network/BrnNetworkGameParams.h"                                        // GameParams
#include "GameSource/Network/BrnServerInterface.h"                                          // BrnServerInterface
#include "GameSource/Network/BrnServerInterfaceBase.h"                                      // GetDownloadableConfigComponent
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"             // the sync-time settings
#include "GameSource/Network/Parameters/BrnNetworkPlayerParams.h"                           // PlayerParamsBase
#include "GameSource/Network/Messages/BrnCollectableMessage.h"                              // CollectableMessage::ECollectableType
#include "GameSource/Network/Managers/BrnNetworkLoginManagerBase.h"                         // LoginManagerBase::CancelLogin
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"                       // MatchMakingManager
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"                         // PostRoundManager
#include "GameSource/Network/Managers/BrnNetworkStandingsManager.h"                         // StandingsManager
#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"                           // TrafficManager
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"                       // NetworkPlayerStatsManager
#include "GameSource/Network/Managers/BrnNetworkScoreboardManager.h"                        // ScoreboardManager
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"                       // LiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"                             // NetworkImageManager
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"                            // NetworkInviteManager

namespace BrnNetwork
{
    typedef CgsNetwork::ServerInterfaceDirtySock NativeServerInterface;
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    // ---------------------------------------------------------------------------------
    // ProcessNetworkEvents
    //
    // Walk the frame's network-IN queue. Every handled event type has its own case (the
    // console jump table gives each one a distinct target); the rest are ignored.
    // ---------------------------------------------------------------------------------
    void StateManager::ProcessNetworkEvents(const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case BrnNetworkModuleIO::NetworkInFriendsUtilShut::KI_EVENT_TYPE:
                // The Guide's friends UI closed: re-read the buddy list.
                mpNetworkManager->GetBuddyManager()->RefreshBuddyList();
                break;

            case BrnNetworkModuleIO::NetworkInSendInvite::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSendInvite* lpBuddyEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSendInvite*>(lpEvent);

                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");
                CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent() != 0,
                           "mpNetworkManager->GetServerInterface()->GetGameComponent()");

                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    // Already in a game: invite straight away, never into a ranked game.
                    GameParams lGameParams;
                    lGameParams.Prepare();
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                    CGS_ASSERT(!lGameParams.IsRankedGame(), "!lGameParams.IsRankedGame()");
                    if (!lGameParams.IsRankedGame())
                    {
                        mpNetworkManager->GetBuddyManager()->SendInvite(&lpBuddyEvent->mBuddyToSendTo);
                    }
                }
                else
                {
                    // Not online yet: sign in first, then create a game and invite from it.
                    BrnNetworkModuleIO::NetworkOutShowSignInGui lShowSignInGui;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lShowSignInGui);

                    if (lpBuddyEvent->mBuddyToSendTo.macName[0] != 0)
                    {
                        mpNetworkManager->GetBuddyManager()->SendInvite(&lpBuddyEvent->mBuddyToSendTo);
                        mbDoInviteAfterCreate = true;
                        mbInstantFreeburn     = false;
                        mbSuspendAfterSignIn  = false;
                    }
                    else
                    {
                        mbSuspendAfterSignIn  = true;
                        mbInstantFreeburn     = false;
                        mbDoInviteAfterCreate = false;
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInRevokeInvite::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRevokeInvite* lpRevokeInviteEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRevokeInvite*>(lpEvent);
                CGS_ASSERT(lpRevokeInviteEvent != 0, "lpRevokeInviteEvent");

                if (!lpRevokeInviteEvent->mbHasOnlineGameBeenStarted)
                {
                    CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetBuddyManager() != 0, "mpNetworkManager->GetBuddyManager()");

                    // The last open invite went away before the game started: leave the
                    // unlocked game we created for it.
                    if (!mpNetworkManager->GetBuddyManager()->AreAnyInvitesOpen())
                    {
                        CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");
                        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent() != 0,
                                   "mpNetworkManager->GetServerInterface()->GetGameComponent()");

                        if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                                CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE &&
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                            !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameLocked())
                        {
                            mpNetworkManager->GetMatchMakingManager()->StartProcess(
                                MatchMakingManager::E_PROCESS_LEAVE_GAME, LeaveGameForOfflineGameFinishedCallback, this);
                            meState = E_STATE_WAIT_MATCHMAKING;
                        }
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInInstantFreeburn::KI_EVENT_TYPE:
            {
                // Instant freeburn from the menus: sign in, then go straight to a freeburn.
                CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0, "mpNetworkModule->GetNetworkEventQueue()");

                BrnNetworkModuleIO::NetworkOutShowSignInGui lShowSignInGui;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lShowSignInGui);

                BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
                lInstantFreeburnEvent.mbIsDoingInstantFreeburn = true;
                mbInstantFreeburn     = true;
                mbSuspendAfterSignIn  = false;
                mbDoInviteAfterCreate = false;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lInstantFreeburnEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInVoipEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInVoipEvent* lpVoipEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInVoipEvent*>(lpEvent);
                // The option screen's 0..10 volume step as a percentage.
                mpNetworkManager->GetVoIPManager()->SetVoipVolume(lpVoipEvent->miVoipVolume * 10);
                break;
            }

            case BrnNetworkModuleIO::NetworkInSettingsUpdateEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSettingsUpdateEvent* lpSettingsUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSettingsUpdateEvent*>(lpEvent);
                mpNetworkManager->GetCamera()->SetUserSetting(lpSettingsUpdateEvent->meCameraFeedSetting);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded* lpLoadedEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded*>(lpEvent);
                CGS_ASSERT(lpLoadedEvent != 0, "lpLoadedEvent");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != 0, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandleLiveRevengeProfileLoadedEvent(lpLoadedEvent->mpLiveRevengeProfile);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLoadingScreenShown::KI_EVENT_TYPE:
                // The loading screen is up: the mode can start now.
                if (meState == E_STATE_WAIT_FOR_LOADING_SCREEN)
                {
                    StartGameMode();
                    meState = E_STATE_WAIT_GAME_MODE_START;
                }
                break;

            case BrnNetworkModuleIO::NetworkInLeftPostEvent::KI_EVENT_TYPE:
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    mbSetNotPlaying = true;
                }
                else
                {
                    BrnNetworkModuleIO::NetworkOutPlayerLeftGame lLeftGameEvent;
                    lLeftGameEvent.meLeftGameReason = E_LEFT_GAME_REASON_GAME_DELETED;
                    lLeftGameEvent.meKickReason     = CgsNetwork::E_KICKREASON_COUNT;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lLeftGameEvent);
                }
                break;

            case BrnNetworkModuleIO::NetworkInCancelLogin::KI_EVENT_TYPE:
                CGS_ASSERT(meState == E_STATE_LOGIN, "meState == E_STATE_LOGIN");
                mpNetworkManager->GetLoginManager()->CancelLogin();
                break;

            case BrnNetworkModuleIO::NetworkInLeavingJunkyard::KI_EVENT_TYPE:
                mpNetworkManager->OnFinishedBoot();
                break;

            case BrnNetworkModuleIO::NetworkInPaybackIntialised::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInPaybackIntialised* lpPaybackEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInPaybackIntialised*>(lpEvent);
                CGS_ASSERT(lpPaybackEvent != 0, "lpPaybackEvent");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != 0, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandlePaybackInitialisedEvent(lpPaybackEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInPaybackSucceeded::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInPaybackSucceeded* lpPaybackEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInPaybackSucceeded*>(lpEvent);
                CGS_ASSERT(lpPaybackEvent != 0, "lpPaybackEvent");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != 0, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandlePaybackSucceededEvent(lpPaybackEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner* lpSwitchRunnerEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner*>(lpEvent);
                CGS_ASSERT(lpSwitchRunnerEvent != 0, "lpSwitchRunnerEvent");
                CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager() != 0, "mpNetworkModule->GetNetworkManager()");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetPlayerManager() != 0,
                           "mpNetworkModule->GetNetworkManager()->GetPlayerManager()");

                // Only the host picks the runner: tell every player, then the local game.
                if (mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                {
                    NetworkPlayerID lPlayerID = -1;
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetPlayer =
                            static_cast<BrnNetworkPlayer*>(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                        if (lpNetPlayer != 0)
                        {
                            lpNetPlayer->SendSwitchBurningHomeRunRunnerMessage(lpSwitchRunnerEvent->mNewRunnerID);
                        }
                    }

                    BrnNetworkModuleIO::NetworkOutSwitchBurningHomeRunRunner lSwitchRunnerEvent;
                    lSwitchRunnerEvent.mNewRunnerID = lpSwitchRunnerEvent->mNewRunnerID;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lSwitchRunnerEvent);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent* lpBurnoutSkillzEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent*>(lpEvent);
                CGS_ASSERT(lpBurnoutSkillzEvent != 0, "lpBurnoutSkillzEvent");
                CGS_ASSERT(lpBurnoutSkillzEvent->mPlayerID != -1,
                           "lpBurnoutSkillzEvent->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

                switch (lpBurnoutSkillzEvent->meEventType)
                {
                case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_ALL_PLAYERS:
                {
                    // The local player's skillz changed: send them to every remote player.
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpBurnoutSkillzEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                            CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                            lpNetworkPlayer->SendNewBurnoutSkillzData(&lpBurnoutSkillzEvent->mNewSkillzData, false);
                        }
                    }
                    break;
                }

                case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_A_SPECIFIC_PLAYER:
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpBurnoutSkillzEvent->mPlayerID));
                    if (lpNetworkPlayer != 0)
                    {
                        lpNetworkPlayer->SendNewBurnoutSkillzData(&lpBurnoutSkillzEvent->mNewSkillzData, true);
                    }
                    break;
                }

                default:
                {
                    CgsDev::Assert::BeginAssert();
                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unhandled burnout skillz network event type: "
                               << static_cast<s32>(lpBurnoutSkillzEvent->meEventType);
                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent* lpShowtimeUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent*>(lpEvent);
                CGS_ASSERT(lpShowtimeUpdateEvent != 0, "lpShowtimeUpdateEvent");

                if (lpShowtimeUpdateEvent->mPlayerID != -1)
                {
                    // The local score goes out round-robin, one remote player per turn.
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpShowtimeUpdateEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            if (mpNetworkManager->GetPlayerManager()->IsPlayerTurnToSendRoundRobinMessage(
                                    lNetworkPlayerID, true, 0))
                            {
                                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                                CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                                lpNetworkPlayer->SendShowtimeUpdateData(lpShowtimeUpdateEvent->miShowtimeScore);
                            }
                        }
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent* lpShowtimeModeSwitchEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent*>(lpEvent);
                CGS_ASSERT(lpShowtimeModeSwitchEvent != 0, "lpShowtimeModeSwitchEvent");

                if (lpShowtimeModeSwitchEvent->mPlayerID != -1)
                {
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpShowtimeModeSwitchEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                            CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                            lpNetworkPlayer->SendShowtimeModeSwitchMessage(lpShowtimeModeSwitchEvent->miFinalShowtimeScore,
                                                                           lpShowtimeModeSwitchEvent->mbEnteringShowtime);
                        }
                    }

                    // Leaving showtime in an online game: the host restarts the network traffic.
                    if (!lpShowtimeModeSwitchEvent->mbEnteringShowtime &&
                        mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                        mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                    {
                        mpNetworkModule->GetNetworkManager()->GetTrafficManager()->RestartNetworkTrafficForHost();
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent* lpFreeburnChallengeEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent*>(lpEvent);
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(lpFreeburnChallengeEvent != 0, "lpFreeburnChallengeEvent");

                // The host relays the challenge to every player.
                if (mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                {
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                        if (lpNetworkPlayer != 0)
                        {
                            lpNetworkPlayer->SendFreeburnChallengeMessage(lpFreeburnChallengeEvent->mChallengeID,
                                                                          lpFreeburnChallengeEvent->meEventType,
                                                                          lpFreeburnChallengeEvent->meChallengeStatus,
                                                                          lpFreeburnChallengeEvent->miActionIndex);
                        }
                    }
                }

                mpNetworkManager->GetStatsManager()->HandleFreeburnChallengeEvent(lpFreeburnChallengeEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent* lpFreeburnChallengeStatusEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent*>(lpEvent);
                CGS_ASSERT(lpFreeburnChallengeStatusEvent != 0, "lpFreeburnChallengeStatusEvent");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpFreeburnChallengeStatusEvent->mPlayerID));
                CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                if (lpNetworkPlayer != 0)
                {
                    lpNetworkPlayer->SendFburnChallengeStatusMessage(&lpFreeburnChallengeStatusEvent->mCompletedChallenges);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent*>(lpEvent);
                CGS_ASSERT(lpActiveFreeburnChallengeEvent != 0, "lpActiveFreeburnChallengeEvent");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpActiveFreeburnChallengeEvent->mPlayerToSendToID));
                CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                if (lpNetworkPlayer != 0)
                {
                    // Translate the challengers' race-car slots into network player ids.
                    NetworkPlayerID laPlayerInChallengeIDs[7];
                    for (s32 liIndex = 0; liIndex < lpActiveFreeburnChallengeEvent->miNumPlayersInChallenge; ++liIndex)
                    {
                        laPlayerInChallengeIDs[liIndex] = mpNetworkModule->GetNetworkPlayerID(
                            lpActiveFreeburnChallengeEvent->maePlayersInChallengeARCI[liIndex]);
                    }
                    lpNetworkPlayer->SendActiveFburnChallengeMessage(lpActiveFreeburnChallengeEvent, laPlayerInChallengeIDs);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInChangeDistrictEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInChangeDistrictEvent* lpChangeDistrictEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInChangeDistrictEvent*>(lpEvent);
                CGS_ASSERT(lpChangeDistrictEvent != 0, "lpChangeDistrictEvent");

                mpNetworkManager->SetCurrentDistrict(lpChangeDistrictEvent->mNewWorldRegion.GetDistrict());

                // Waiting in a lobby: push the new district with the next player parameters.
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                    !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                {
                    mDistrictData.mbValid    = true;
                    mDistrictData.meDistrict = lpChangeDistrictEvent->mNewWorldRegion.GetDistrict();
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint* lpLandmarkEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint*>(lpEvent);
                CGS_ASSERT(lpLandmarkEvent != 0, "lpEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                    lpNetworkPlayer->SendLandmarkTriggeredMessage(lpLandmarkEvent->miCheckpointIndex);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent* lpStuntScoreUpdatedEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent*>(lpEvent);
                CGS_ASSERT(lpStuntScoreUpdatedEvent != 0, "lpStuntScoreUpdatedEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                    lpNetworkPlayer->SendStuntScoreUpdatedMessage(lpStuntScoreUpdatedEvent->miStuntScore);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInStuntMultiplierEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInStuntMultiplierEvent* lpStuntMultiplierEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInStuntMultiplierEvent*>(lpEvent);
                CGS_ASSERT(lpStuntMultiplierEvent != 0, "lpStuntMultiplierEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != 0, "lpNetworkPlayer");
                    lpNetworkPlayer->SendStuntMultiplierMessage(*lpStuntMultiplierEvent);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowGamerCard::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowGamerCard* lpShowGamerCard =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowGamerCard*>(lpEvent);
                CGS_ASSERT(lpShowGamerCard != 0, "lpShowGamerCard");
                mpNetworkManager->GetGamerCardManager()->ShowGamerCard(mpNetworkManager->GetLocalUserControllerPort(),
                                                                       &lpShowGamerCard->mPlayerName);
                break;
            }

            case BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent::KI_EVENT_TYPE:
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                mpNetworkManager->ProcessNetworkTextureDecodeEvent(
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent*>(lpEvent));
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSelectScoreboardEvent* lpScoreboardEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSelectScoreboardEvent*>(lpEvent);
                CGS_ASSERT(lpScoreboardEvent != 0, "lpScoreboardEvent");
                mpNetworkManager->GetScoreboardManager()->HandleScoreboardEvent(lpScoreboardEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInScoreTargetEvent::KI_EVENT_TYPE:
            {
                const EvScoreTargetEvent* lpScoreTargetEvent = reinterpret_cast<const EvScoreTargetEvent*>(lpEvent);
                CGS_ASSERT(lpScoreTargetEvent != 0, "lpScoreTargetEvent");
                mpNetworkManager->GetScoreboardManager()->HandleEvScoreTargetEvent(lpScoreTargetEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInRequestAccountSettings::KI_EVENT_TYPE:
            {
                BrnNetworkModuleIO::NetworkOutAccountSettings lNetworkAccountSettings;
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetAccountSettings(
                    &lNetworkAccountSettings.mbAgreeToShareInfoEA,
                    &lNetworkAccountSettings.mbAgreeToShareInfoPartners,
                    &lNetworkAccountSettings.mbTelemetryEnable);
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkAccountSettings);
                break;
            }

            case BrnNetworkModuleIO::NetworkInAccountUpdate::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInAccountUpdate* lpAccountUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInAccountUpdate*>(lpEvent);

                if (mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetStatus() ==
                    CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    CGS_ASSERT(lpAccountUpdateEvent != 0, "lpAccountUpdateEvent");
                    mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->EditAccount(
                        lpAccountUpdateEvent->mbAgreeToShareInfoEA,
                        lpAccountUpdateEvent->mbAgreeToShareInfoPartners,
                        lpAccountUpdateEvent->mbTelemetryEnable);
                    meState = E_STATE_WAIT_UPDATE_ACCOUNT;
                }
                else
                {
                    // The player-info component is busy: report the update as done (unchanged).
                    BrnNetworkModuleIO::NetworkOutAccountUpdateComplete lNetworkUpdateAccountCompleteEvent;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkUpdateAccountCompleteEvent);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInReqCamPicEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInReqCamPicEvent* lpReqCompPicEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInReqCamPicEvent*>(lpEvent);
                CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == lpReqCompPicEvent->meCompressedFormat,
                           "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpReqCompPicEvent->meCompressedFormat");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetCamera() != 0, "mpNetworkManager->GetCamera()");
                mpNetworkManager->GetCamera()->GetCompressedLocalCameraPicture(
                    lpReqCompPicEvent->mpTextureToCompressedInto, lpReqCompPicEvent->miQualitySetting,
                    GetCompressedCameraPicCallback, this);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent* lpPlayerCrashesEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent*>(lpEvent);
                CGS_ASSERT(lpPlayerCrashesEvent != 0, "lpPlayerCrashesEvent");

                // Stamp who the local player crashed into on every player.
                NetworkPlayerID lNetworkPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    if (lpNetworkPlayer != 0)
                    {
                        NetworkPlayerID lCrasherEntityNetworkID;
                        if (lpPlayerCrashesEvent->meCrasherRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                            lpPlayerCrashesEvent->meCrasherRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)
                        {
                            CGS_ASSERT(mpNetworkModule != 0, "mpNetworkModule");
                            lCrasherEntityNetworkID = mpNetworkModule->GetNetworkPlayerID(lpPlayerCrashesEvent->meCrasherRaceCarIndex);
                        }
                        else
                        {
                            lCrasherEntityNetworkID = -1;
                        }
                        lpNetworkPlayer->SetLocalPlayerCrasherID(lCrasherEntityNetworkID);
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent* lpShowGamerCard =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent*>(lpEvent);
                CGS_ASSERT(lpShowGamerCard != 0, "lpShowGamerCard");
                mpNetworkManager->GetGamerCardManager()->ShowGamerCardForXuid(mpNetworkManager->GetLocalUserControllerPort(),
                                                                              lpShowGamerCard->mu64Xuid);
                break;
            }

            default:
                break;
            }

            liEventType = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }

    // ---------------------------------------------------------------------------------
    // ProcessGameStateActions
    //
    // Walk the frame's game-action queue. The game-state bridge forwards only the actions
    // the network acts on, so any other type is reported as unknown.
    // ---------------------------------------------------------------------------------
    void StateManager::ProcessGameStateActions(const CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
    {
        const CgsModule::Event* lpAction = 0;
        s32 liActionSize = 0;
        s32 liActionType = lpGameActionQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            switch (liActionType)
            {
            case GsmIO::E_ACTION_RESET_PLAYER_CAR:
            {
                const GsmIO::ResetPlayerCarAction* lpResetPlayerCarAction =
                    reinterpret_cast<const GsmIO::ResetPlayerCarAction*>(lpAction);

                if (lpResetPlayerCarAction->mCarModelId != 0)
                {
                    CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
                    if (!lpGames->IsLocalPlayerInGame() || !lpGames->IsGameStarted())
                    {
                        mpNetworkManager->SetFreeBurnCar(lpResetPlayerCarAction->mCarModelId,
                                                         lpResetPlayerCarAction->mWheelModelId,
                                                         lpResetPlayerCarAction->mfDeformationAmount);
                    }

                    // In a lobby that has not started: push the car with the next player parameters.
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                        !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        CGS_ASSERT(lpResetPlayerCarAction->mCarModelId != 0,
                                   "lpResetPlayerCarAction->GetCarModelId() != kCGSID_NULL");
                        mNewCarData.mCarModelId     = lpResetPlayerCarAction->mCarModelId;
                        mNewCarData.mWheelModelId   = lpResetPlayerCarAction->mWheelModelId;
                        mNewCarData.mfDeformAmount  = lpResetPlayerCarAction->mfDeformationAmount;
                        mNewCarData.mbValid         = true;
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
            {
                const GsmIO::OnStuntElementCompleteAction* lpStuntElementAction =
                    reinterpret_cast<const GsmIO::OnStuntElementCompleteAction*>(lpAction);
                CGS_ASSERT(lpStuntElementAction != 0, "lpStuntElementAction");
                NetworkPlayerID lNetworkPlayerID = -1;

                CollectableMessage::ECollectableType leCollectableType;
                switch (lpStuntElementAction->meStuntElementType)
                {
                case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_JUMP;
                    break;
                case BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_SMASH;
                    break;
                case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_BILLBOARD;
                    break;
                default:
                    CGS_FAIL("Unknown stunt element type\n");
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_JUMP;
                    break;
                }

                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    if (lpNetworkPlayer != 0)
                    {
                        lpNetworkPlayer->SendCollectableDone(lpStuntElementAction->mID, leCollectableType);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_PREPARE_FOR_MODE:
            {
                const GsmIO::PrepareForModeAction* lpPrepareForModeAction =
                    reinterpret_cast<const GsmIO::PrepareForModeAction*>(lpAction);
                CGS_ASSERT(lpPrepareForModeAction != 0, "lpPrepareForModeAction");

                // First prepare of an online mode: load the sync-time settings from the
                // downloaded config, unless the players are just moving between the two
                // lobby modes.
                if (lpPrepareForModeAction->IsFirstPrepareForMode() &&
                    lpPrepareForModeAction->GetGameModeParams()->GetGameModeType() >= GsmIO::E_MODE_ONLINE_MODE_START)
                {
                    const GsmIO::EGameModeType leGameMode = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                    const bool lbLobbyModeSwitch =
                        lpPrepareForModeAction->IsMovingBetweenOnlineLobbyModes() &&
                        (leGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY || leGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME);

                    if (!lbLobbyModeSwitch)
                    {
                        mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

                        BrnServerInterfaceDownloadableConfig* lpDownloadableConfig =
                            mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent();

                        mpNetworkManager->GetStartTimeManager()->SetSyncTimeTimeouts(
                            CgsSystem::Time(lpDownloadableConfig->MinSyncTime()),
                            CgsSystem::Time(lpDownloadableConfig->MaxSyncTime()));
                        mpNetworkManager->GetStartTimeManager()->SetWaitForClientReadyTimeouts(
                            CgsSystem::Time(lpDownloadableConfig->TimeToWaitForSilentClientReady()),
                            CgsSystem::Time(lpDownloadableConfig->TimeToWaitForCommunicatingClientReady()));
                        mpNetworkManager->GetStartTimeManager()->SetWaitForStartTimeTimeout(
                            CgsSystem::Time(lpDownloadableConfig->WaitForStartTime()));

                        // The lobby modes start at once; every other mode leaves the configured gap.
                        const GsmIO::EGameModeType leStartMode = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                        if (leStartMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY || leStartMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                        {
                            mpNetworkManager->GetStartTimeManager()->SetGapTillStartTime(CgsSystem::Time(0.0f));
                        }
                        else
                        {
                            mpNetworkManager->GetStartTimeManager()->SetGapTillStartTime(
                                CgsSystem::Time(lpDownloadableConfig->TimeGapBeforeStartTime()));
                        }
                    }
                }

                // An online race starts every player on the start line.
                if (lpPrepareForModeAction->GetGameModeParams()->GetGameModeType() == GsmIO::E_MODE_ONLINE_RACE)
                {
                    NetworkPlayerID lPlayerID = -1;
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                        if (lpNetPlayer != 0)
                        {
                            lpNetPlayer->SetOnStartLine(true);
                        }
                    }
                }

                // Online showtime holds the traffic restart; every other mode releases it.
                const GsmIO::EGameModeType leModeType = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                if ((leModeType == GsmIO::E_MODE_OFFLINE_SHOWTIME || leModeType == GsmIO::E_MODE_ONLINE_SHOWTIME) &&
                    leModeType >= GsmIO::E_MODE_ONLINE_MODE_START && leModeType <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(true);
                }
                else
                {
                    mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);
                }
                break;
            }

            case GsmIO::E_ACTION_START_MODE_INTRO:
            {
                const GsmIO::StartModeIntroAction* lpStartModeIntro =
                    reinterpret_cast<const GsmIO::StartModeIntroAction*>(lpAction);
                CGS_ASSERT(lpStartModeIntro != 0, "lpStartModeIntro");

                // Online stunt run: players only talk to their own team.
                if (lpStartModeIntro->meGameMode == GsmIO::E_MODE_ONLINE_FUGITIVE)
                {
                    CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetVoIPManager() != 0, "mpNetworkManager->GetVoIPManager()");
                    CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

                    const NetworkPlayerID lLocalPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lLocalPlayerID != -1)
                    {
                        PlayerParamsBase lPlayerParams;
                        CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");
                        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
                        CGS_ASSERT(lpGames != 0, "lpGames");

                        lPlayerParams.PreparePattern();
                        lpGames->GetPlayerParametersByPlayerID(lLocalPlayerID, &lPlayerParams);
                        const GsmIO::EPlayerTeam leLocalTeam = lPlayerParams.GetPlayerTeam();
                        if (leLocalTeam == GsmIO::E_PLAYER_TEAM_NONE)
                        {
                            CgsDev::Assert::BeginAssert();
                            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStrStream << "Starting online stunt run but the local player is not on a team\n";
                            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                        }

                        NetworkPlayerID lPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
                        {
                            if (lpGames->IsPlayerInGame(lPlayerID) && lPlayerID != lLocalPlayerID)
                            {
                                lPlayerParams.PreparePattern();
                                lpGames->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
                                if (lPlayerParams.GetPlayerTeam() != leLocalTeam)
                                {
                                    mpNetworkManager->GetVoIPManager()->DisableCommsWithPlayer(lPlayerID);
                                }
                            }
                        }
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_STOP_MODE_INTRO:
            {
                const GsmIO::StopModeIntroAction* lpStopIntroAction =
                    reinterpret_cast<const GsmIO::StopModeIntroAction*>(lpAction);
                if (lpStopIntroAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START &&
                    lpStopIntroAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    IntroStopped(lpStopIntroAction);
                }
                break;
            }

            case GsmIO::E_ACTION_MARKED_MAN_LOADED:
            {
                const GsmIO::MarkedManLoadedAction* lpMarkedManLoadedAction =
                    reinterpret_cast<const GsmIO::MarkedManLoadedAction*>(lpAction);
                if (lpMarkedManLoadedAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START &&
                    lpMarkedManLoadedAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    MarkedManLoaded(lpMarkedManLoadedAction);
                }
                break;
            }

            case GsmIO::E_ACTION_START_PLAYING_MODE:
            {
                // The mode is running: nobody is on the start line any more.
                NetworkPlayerID lPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                    if (lpNetPlayer != 0)
                    {
                        lpNetPlayer->SetOnStartLine(false);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_FINISHED_MODE:
            {
                const GsmIO::FinishedModeAction* lpFinishedModeAction =
                    reinterpret_cast<const GsmIO::FinishedModeAction*>(lpAction);
                CGS_ASSERT(lpFinishedModeAction != 0, "lpModeResultsAction");

                // The console also writes two progress lines to the network debug log here;
                // that log stream has no home in the tree, so only the behaviour is kept.
                if (lpFinishedModeAction->mbIsOnlineGameMode &&
                    lpFinishedModeAction->meFinishedGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpFinishedModeAction->meFinishedGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    const NetworkPlayerID lLocalNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lLocalNetworkPlayerID != -1)
                    {
                        mpNetworkManager->GetStandingsManager()->HandlePlayerFinishedMode(lLocalNetworkPlayerID,
                                                                                         lpFinishedModeAction);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_SHOW_MODE_RESULTS:
            {
                const GsmIO::ShowModeResultsAction* lpShowModeResultsAction =
                    reinterpret_cast<const GsmIO::ShowModeResultsAction*>(lpAction);
                CGS_ASSERT(lpShowModeResultsAction != 0, "lpShowModeResultsAction");

                if (lpShowModeResultsAction->mbIsOnlinePostEvent &&
                    lpShowModeResultsAction->meGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpShowModeResultsAction->meGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    meState = E_STATE_WAIT_POST_ROUND;
                    mpNetworkManager->GetPostRoundManager()->StartProcess(PostRoundManager::E_PROCESS_END_ROUND,
                                                                          PostRoundFinishedCallback, this);
                }

                mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);

                // Stunt run over: everybody may talk to everybody again.
                if (lpShowModeResultsAction->meGameModeType == GsmIO::E_MODE_ONLINE_FUGITIVE)
                {
                    CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetVoIPManager() != 0, "mpNetworkManager->GetVoIPManager()");
                    mpNetworkManager->GetVoIPManager()->EnableAllComms();
                }
                break;
            }

            case GsmIO::E_ACTION_STOP_MODE:
            {
                const GsmIO::StopModeAction* lpStopModeAction = reinterpret_cast<const GsmIO::StopModeAction*>(lpAction);
                CGS_ASSERT(lpStopModeAction != 0, "lpStopModeAction");

                if (lpStopModeAction->mu8Field10 &&
                    lpStopModeAction->meGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpStopModeAction->meGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    if (lpStopModeAction->mu8Field12 || lpStopModeAction->mu8Field11 || lpStopModeAction->mu8Field13)
                    {
                        // Last round played (or the game is being left): end the game.
                        meState = E_STATE_WAIT_POST_ROUND;
                        mpNetworkManager->GetPostRoundManager()->StartEndOfGame(
                            lpStopModeAction->mu8Field11 != 0,
                            lpStopModeAction->mu8Field11 != 0 || lpStopModeAction->mu8Field13 != 0,
                            PostGameFinishedCallback, this);
                        mpNetworkModule->GetNetworkManager()->SetRound(-1);
                    }
                    else
                    {
                        // More rounds to play: start the next one.
                        SendStartNextRoundMessage(lpStopModeAction->miField08 + 1);
                        mpNetworkModule->GetNetworkManager()->SetRound(lpStopModeAction->miField08 + 1);
                        mpNetworkModule->GetNetworkManager()->GetStandingsManager()->ClearStandingsData();
                    }

                    mpNetworkManager->GetNetworkImageManager()->HandlePlayerStoppedMode();
                    mpNetworkManager->GetStartTimeManager()->StopSyncingTime();
                }

                mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);
                break;
            }

            case GsmIO::E_ACTION_CAR_SELECT_ONLINE_SELECT_CAR:
            {
                const GsmIO::CarSelectOnlineSelectCarAction* lpSelectCarAction =
                    reinterpret_cast<const GsmIO::CarSelectOnlineSelectCarAction*>(lpAction);

                NetworkPlayerID lPlayerID = -1;
                const NetworkPlayerID lLocalPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                CGS_ASSERT(lLocalPlayerID != -1,
                           "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");

                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalPlayerID));
                CGS_ASSERT(lpMenuData != 0, "lpMenuData");

                // Record the choice on the local menu data, then tell every player.
                lpMenuData->mWheelId             = 0;
                lpMenuData->mCarId               = lpSelectCarAction->mCarID;
                lpMenuData->mu16CarColourIndex   = lpSelectCarAction->mu16ColourIndex;
                lpMenuData->mu16PaintFinishIndex = lpSelectCarAction->mu16PaintFinishIndex;
                lpMenuData->mfUnknown48          = lpSelectCarAction->mfDeformationAmount;
                lpMenuData->mbFinalCarSelection  = lpSelectCarAction->mbFinalSelection;

                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                    if (lpNetPlayer != 0)
                    {
                        lpNetPlayer->SendSelectedCar(lpMenuData->mCarId, lpMenuData->mWheelId,
                                                     lpMenuData->mu16CarColourIndex, lpMenuData->mu16PaintFinishIndex,
                                                     lpMenuData->mfUnknown48, lpSelectCarAction->mbFinalSelection);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                PrepareForInvite();
                break;

            case GsmIO::E_ACTION_PERFORM_INVITE:
            {
                const GsmIO::PerformInviteAction* lpPerformInviteAction =
                    reinterpret_cast<const GsmIO::PerformInviteAction*>(lpAction);
                CGS_ASSERT(lpPerformInviteAction != 0, "lpPerformInviteAction");

                // A different local user is taking the invite: drop the current sign-in first.
                if (lpPerformInviteAction->mInviteParams.mbIsLocalUserChangeNeeded &&
                    mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
                }

                mpNetworkManager->GetNetworkInviteManager()->StartInviteOrJoin(lpPerformInviteAction->mInviteParams);
                meState = E_STATE_COUNT;
                break;
            }

            case GsmIO::E_ACTION_PLAYER_ELIMINATED:
            {
                const GsmIO::PlayerEliminatedAction* lpPlayerEliminatedAction =
                    reinterpret_cast<const GsmIO::PlayerEliminatedAction*>(lpAction);
                const NetworkPlayerID lEliminatedPlayerID =
                    mpNetworkModule->GetNetworkPlayerID(lpPlayerEliminatedAction->meActiveRaceCarIndex);
                BrnNetworkPlayer* lpPlayer =
                    static_cast<BrnNetworkPlayer*>(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lEliminatedPlayerID));
                if (lpPlayer != 0)
                {
                    lpPlayer->SetEliminated(true);
                }
                break;
            }

            case GsmIO::E_ACTION_ONLINE_GAME_RESULT:
            {
                const GsmIO::OnlineGameResults* lpResults = reinterpret_cast<const GsmIO::OnlineGameResults*>(lpAction);
                CGS_ASSERT(lpResults != 0, "lpResults");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetStatsManager() != 0, "mpNetworkManager->GetStatsManager()");

                if (lpResults->miEventType == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                    lpResults->miEventType == GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    mpNetworkManager->GetStatsManager()->UploadFreeBurnLobbyStats(lpResults);
                }
                else
                {
                    CGS_ASSERT(mpNetworkManager->GetPostRoundManager() != 0, "mpNetworkManager->GetPostRoundManager()");
                    CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != 0, "mpNetworkManager->GetLiveRevengeManager()");
                    mpNetworkManager->GetPostRoundManager()->ProcessRaceResults(lpResults);
                    mpNetworkManager->GetStatsManager()->HandleGameResults(lpResults);
                }
                break;
            }

            case GsmIO::E_ACTION_ONLINE_ROUND_RESULT:
            {
                const GsmIO::OnlineRoundResults* lpRoundResults = reinterpret_cast<const GsmIO::OnlineRoundResults*>(lpAction);
                CGS_ASSERT(lpRoundResults != 0, "lpRoundResults");
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != 0, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandleRoundResults(lpRoundResults);
                mpNetworkManager->GetNetworkImageManager()->HandleRoundResults(lpRoundResults);
                break;
            }

            case GsmIO::E_ACTION_NETWORK_CAUGHT_FEVER:
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                mpNetworkManager->SetHasFever(true);
                mFeverData.mbValid    = true;
                mFeverData.mbHasFever = true;
                break;

            case GsmIO::E_ACTION_ACHIEVEMENTS_EARNED:
            {
                const GsmIO::AchievementsEarnedAction* lpAchievementAction =
                    reinterpret_cast<const GsmIO::AchievementsEarnedAction*>(lpAction);
                CGS_ASSERT(lpAchievementAction != 0, "lpAchievementAction");
                mpNetworkManager->GetStatsManager()->HandleNewNumberOfAchievements(lpAchievementAction->miAchivementCount);
                break;
            }

            default:
            {
                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unknown GameAction coming into NetworkManager: " << liActionType << "\n";
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                break;
            }
            }

            liActionType = lpGameActionQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }
    }
} // namespace BrnNetwork

// FOLDED FROM BrnNetworkStateManager_wN1_04.cpp
#include <xtl.h>
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include <cstring>                                                                         // memset / memcpy / strlen / strncpy
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // the memory checks' log line
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                           // GetFirstEvent / GetNextEvent / AddEvent
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                        // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                // login questions, loading screen, launched
#include "GameShared/GameClasses/Gui/CgsGuiEventLocalisedTextPointerRemoved.h"             // CgsGui::GuiEventLocalisedTextPointerRemoved
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"                   // CgsGui::GuiEventNetworkSuspension
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                            // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                       // CgsNetwork::KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // GetNextPlayerID / GetMenuDataByID
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"            // AreAllConnectionsSuccessful
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"                // HostMigrationManager::Disable
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"             // EComponents / EStatus / MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetLocalPlayerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"   // ServerInterfaceUsersets
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"        // SetSessionFlags / GetPlayerXUIDByID
#include "GameSource/GameState/BrnGameEvents.h"                                            // the game-event records
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                     // EGameModeType / EPlayerTeam
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                      // the network GUI payload types
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"                            // BrnGui::GuiEventNetworkCreateGame
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"                            // BrnGui::GuiEventNetworkGameParams
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"                           // BrnGui::GuiEventNetworkPlayerStats
#include "GameSource/Network/BrnNetworkGameParams.h"                                       // BrnNetwork::GameParams
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // PostSimulationInputBuffer
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOut* records
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"
#include "GameSource/Network/BrnServerInterface.h"                                         // BrnServerInterface
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"                            // EnableMugshotOutput
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"                           // LaunchManager::Start
#include "GameSource/Network/Managers/BrnNetworkMarkedManManager.h"                        // SendMarkedManDataToAll
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"                      // create / join / search processes
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"                      // GetLocalPlayerStats
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"                        // StartEndOfGame
#include "GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h"                   // route data
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"                       // Suspend / Resume
#include "GameSource/Network/Parameters/BrnNetworkGameSearchParams.h"                      // BrnNetwork::GameSearchParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                        // BrnNetwork::PlayerInfoData
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                     // BrnNetwork::PlayerParams
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"                             // BrnNetwork::NetworkPlayerStats
#include "GameSource/Resource/BrnResourceAllocator.h"                                      // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameSource/GameState/BrnGameActions.h"

// ===================================================================================
// BrnNetwork::StateManager -- part 4: GUI-driven session changes, game-mode team
// checks and the round / game completion hand-offs.
//
//   PlayerAdded / PlayerRemoved        raise the per-frame freeburn-lobby refresh flags
//   UpdateMarkedMan                    cache the marked man until the game starts
//   GameModeHasTeams                   is the current game mode a team mode?
//   GameModeHasEnoughTeams             do the players in the game cover enough teams?
//   SuspendToLeaveGame                 record why we leave, then suspend the interface
//   PostRoundFinishedCallback /
//   PostGameFinishedCallback           post-round manager completion hand-offs
//   IntroStopped / MarkedManLoaded     start (or, offline, force) the network start time
//   PostRoundProcessingFinished        reset the marked men, stop host migration
//   SendStartNextRoundMessage          hand the next round's route to the game state
//   PostGameProcessingFinished         back to the lobby, or out of the game
//   PrepareForInvite                   settle the session before an invite is taken
//   JoinGameSession / CreateGame /
//   ModifyGame                         join an invited session, create or change a game
//   UpdateEvents                       drain the GUI, game-action and network queues
//   StartGameMode                      build and send the start-game event
//   ProcessGuiEvents                   act on the frame's network GUI events
// ===================================================================================

// Xbox Guide entry points, keyed by the signed-in user index, for a 64-bit XUID (0 == success).
// Declared as extern "C" free functions, as the other network TUs that open the Guide do.


// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkStateManager.cpp".

namespace BrnNetwork
{
    typedef CgsNetwork::ServerInterfaceDirtySock NativeServerInterface;
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    namespace
    {
        // PreparedForInviteEvent names the module that is ready for the invite; the network
        // module is 1. FLAG: EModulePreparedForInvite has no home yet.
        const s32 KI_MODULE_PREPARED_FOR_INVITE_NETWORK = 1;

        // CreateGame: an eight-player game that needs two to launch.
        const s32 KI_CREATE_GAME_MAX_PLAYERS = 8;
        const s32 KI_CREATE_GAME_MIN_PLAYERS = 2;

        // ModifyGame: the session flags follow the game's security (EBrnGameSecurity 0 public,
        // 1 private, 2 closed; no home yet). FLAG: flag names not recovered.
        const s32 KI_GAME_SECURITY_CLOSED      = 2;
        const s32 KI_SESSION_FLAGS_OPEN_GAME   = 0x400;
        const s32 KI_SESSION_FLAGS_CLOSED_GAME = 0x600;

        // StartGameMode: the packed time limit counts in steps of ten; the event carries it
        // times sixty. FLAG: the two names are ours.
        const s32 KI_TIME_LIMIT_STEP    = 10;
        const f32 KF_SECONDS_PER_MINUTE = 60.0f;

        // The time limit the start-game event accepts (reference KF_TIME_MIN / KF_TIME_MAX).
        const f32 KF_TIME_MIN = 10.0f;
        const f32 KF_TIME_MAX = 30.0f;

        // ProcessGuiEvents: a launch needs at least two players (a literal 2: the DWARF lists only
        // KI_TIME_INCREMENTS, KF_TIME_MAX and KF_TIME_MIN here); the values GuiEventNetworkLaunched
        // carries (CgsGui::GuiEventNetworkLaunched::ELaunchedStatus, reference values; the
        // committed event is a bare 4-byte payload).
        const s32 KI_LAUNCHED_STATUS_NOT_ENOUGH_PLAYERS  = 1;
        const s32 KI_LAUNCHED_STATUS_STILL_CONNECTING    = 2;
        const s32 KI_LAUNCHED_STATUS_WAITING_FOR_ROUTES  = 3;
        const s32 KI_LAUNCHED_STATUS_NOT_ENOUGH_TEAMS    = 4;
        const s32 KI_LAUNCHED_STATUS_STILL_PLAYING       = 5;

        // ProcessGuiEvents: the custom-match search asks for one free slot; the last Prepare
        // argument is 2. FLAG: that argument's name is not recovered.
        const u32 KU_SEARCH_REQUIRED_SLOTS = 1;
        const u32 KU_SEARCH_FIELD_29C      = 2;

        // The GUI event ids ProcessGuiEvents acts on. Where the payload type has a home its
        // GetEventType() returns the same id. FLAG: the names of 52, 54, 56, 67, 68, 253, 259,
        // 260, 261 and 265 are ours (their GUI types have no home).
        const s32 KI_GUI_EVENT_LOCALISED_TEXT_POINTER_REMOVED  = 13;
        const s32 KI_GUI_EVENT_LOADING_SCREEN_STATE            = 33;
        const s32 KI_GUI_EVENT_NETWORK_SUSPENSION              = 45;
        const s32 KI_GUI_EVENT_ANSWER_LOGIN_QUESTION           = 48;
        const s32 KI_GUI_EVENT_NETWORK_LEAVE_GAME              = 52;
        const s32 KI_GUI_EVENT_NETWORK_LEAVING_GAME            = 53;
        const s32 KI_GUI_EVENT_NETWORK_LAUNCH                  = 54;
        const s32 KI_GUI_EVENT_NETWORK_FORCE_LAUNCH            = 56;
        const s32 KI_GUI_EVENT_AUTOSAVE_STARTED                = 67;
        const s32 KI_GUI_EVENT_AUTOSAVE_FINISHED               = 68;
        const s32 KI_GUI_EVENT_NETWORK_SELECTED_PLAYER_OPTION  = 246;
        const s32 KI_GUI_EVENT_NETWORK_HIGHLIGHTED_PLAYER      = 247;
        const s32 KI_GUI_EVENT_NETWORK_QUIT_PLAYING            = 250;
        const s32 KI_GUI_EVENT_NETWORK_QUICK_MATCH             = 251;
        const s32 KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_SEARCH     = 252;
        const s32 KI_GUI_EVENT_NETWORK_CANCEL_SEARCH           = 253;
        const s32 KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_JOIN       = 255;
        const s32 KI_GUI_EVENT_NETWORK_CREATE_GAME             = 256;
        const s32 KI_GUI_EVENT_NETWORK_GAME_PARAMS             = 257;
        const s32 KI_GUI_EVENT_NETWORK_REQUEST_LOCAL_STATS     = 259;
        const s32 KI_GUI_EVENT_NETWORK_REQUEST_GAME_PARAMS     = 260;
        const s32 KI_GUI_EVENT_NETWORK_READY_TO_JOIN_SESSION   = 261;
        const s32 KI_GUI_EVENT_NETWORK_OUTPUT_PLAYER_TEXTURE   = 264;
        const s32 KI_GUI_EVENT_NETWORK_SWITCH_TEAM             = 265;
        const s32 KI_GUI_EVENT_NETWORK_NEWS_AND_TOS            = 266;
        const s32 KI_GUI_EVENT_NETWORK_SPLASH                  = 269;
        const s32 KI_GUI_EVENT_NETWORK_CONNECT                 = 272;

        // The channel the PC GUI module leaves wrapped records on (see ProcessGuiEvents).
        const s32 KI_GUI_CHANNEL_OUT = 40;

        // The bare 4-byte GUI payloads carry one word.







        template <typename TGuiEvent>
        s32 GetPayloadWord(const TGuiEvent* lpEvent)
        {
            CGS_STATIC_ASSERT((sizeof(lpEvent->maData) == sizeof(s32)), "4-byte GUI payload");
            s32 liValue;
            std::memcpy(&liValue, lpEvent->maData, sizeof(s32));
            return liValue;
        }
    }

    // ------------------------------------------------------------------------------------
    // A player joined the session. The host of a live multi-player game that is not in
    // the middle of an invite restarts the freeburn lobby this frame (as a join); every
    // add refreshes the lobby.
    // ------------------------------------------------------------------------------------
    void StateManager::PlayerAdded()
    {
        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame() > 1
            && !mpNetworkManager->GetNetworkInviteManager()->IsInInvite())
        {
            mbStartFreeburnLobbyThisFrame       = true;
            mbStartingAfterJoinThisFrame        = true;
            mbStartingAfterOnlineEventThisFrame = false;
            mbForceStartFreeburnLobbyThisFrame  = false;
        }

        mbRefreshingFreeburnLobbyThisFrame = true;
    }

    // ------------------------------------------------------------------------------------
    // A player left the session: refresh the freeburn lobby this frame.
    // ------------------------------------------------------------------------------------
    void StateManager::PlayerRemoved()
    {
        mbRefreshingFreeburnLobbyThisFrame = true;
    }

    // ------------------------------------------------------------------------------------
    // Remember the marked man chosen in the lobby. Once the game has started the choice
    // is owned by the marked-man manager, so the cache is left alone.
    // ------------------------------------------------------------------------------------
    void StateManager::UpdateMarkedMan(NetworkPlayerID lMarkedManID)
    {
        if (!mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
        {
            mMarkedManData.mPlayerID = lMarkedManID;
            mMarkedManData.mbValid   = true;
        }
    }

    // ------------------------------------------------------------------------------------
    // True when the game mode of the current game is played in teams.
    // ------------------------------------------------------------------------------------
    bool StateManager::GameModeHasTeams()
    {
        GameParams lGameParams;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        return lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_ROAD_RAGE
            || lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN
            || lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_FUGITIVE;
    }

    // ------------------------------------------------------------------------------------
    // Count the distinct teams among the players in the game and test the count against
    // what the team mode needs. Modes without teams always have enough.
    // ------------------------------------------------------------------------------------
    bool StateManager::GameModeHasEnoughTeams()
    {
        s32 liNumTeamsFound = 0;

        if (!GameModeHasTeams())
        {
            return true;
        }

        {
            // One flag per team (the console's GsmIO::E_PLAYER_TEAM_COUNT is 9).
            bool         labFoundTeams[GsmIO::E_PLAYER_TEAM_COUNT];
            PlayerParams lPlayerParams;

            memset(labFoundTeams, 0, sizeof(labFoundTeams));

            for (s32 liPlayerIndex = 0;
                 liPlayerIndex < mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame();
                 ++liPlayerIndex)
            {
                lPlayerParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(liPlayerIndex,
                                                                                                        &lPlayerParams);

                if (!labFoundTeams[lPlayerParams.GetPlayerTeam()])
                {
                    ++liNumTeamsFound;
                }
                labFoundTeams[lPlayerParams.GetPlayerTeam()] = true;
            }
        }

        {
            GameParams lGameParams;

            lGameParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            switch (lGameParams.GameMode())
            {
            case GsmIO::E_MODE_ONLINE_ROAD_RAGE:
                return liNumTeamsFound == 2;

            case GsmIO::E_MODE_ONLINE_FUGITIVE:
                return liNumTeamsFound >= 2;

            case GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN:
                return true;

            default:
                CGS_FAIL("Selected game mode shouldn't have teams!\n");
                return false;
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Leave the game by suspending the server interface. Ignored while already leaving or
    // in post-round processing. A suspension manager that is still busy defers the
    // suspend until it goes idle; an interface that is already suspended needs nothing.
    // ------------------------------------------------------------------------------------
    void StateManager::SuspendToLeaveGame(ELeftGameReason leLeftReason, CgsNetwork::EKickReason leKickReason)
    {
        if (meState == E_STATE_WAIT_SUSPENSION_LEAVING_GAME || meState == E_STATE_WAIT_POST_ROUND)
        {
            return;
        }

        meLeftReason = leLeftReason;
        meKickReason = leKickReason;

        if (mpNetworkManager->GetSuspensionManager()->IsSuspending())
        {
            meState = E_STATE_WAIT_SUSPENSION_IDLE_LEAVING_GAME;
        }
        else if (!mpNetworkManager->GetServerInterface()->IsSuspended())
        {
            meState = E_STATE_WAIT_SUSPENSION_LEAVING_GAME;
            // FLAG: Suspend is expanded inline here and both suspension types yield the same
            // interface flags, so the type argument cannot be read back; OFFLINE is assumed.
            mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                              SuspensionFinishedCallback, this);
        }
    }

    // ------------------------------------------------------------------------------------
    // Post-round manager completion: the round's results are through.
    // ------------------------------------------------------------------------------------
    void StateManager::PostRoundFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT_AT(lbSuccess,
            "lbSuccess", __FILE__, 3636 /* ARTIST 0x82553DB0 */);

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->PostRoundProcessingFinished();
    }

    // ------------------------------------------------------------------------------------
    // Post-round manager completion at the end of the game.
    // ------------------------------------------------------------------------------------
    void StateManager::PostGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT_AT(lbSuccess,
            "lbSuccess", __FILE__, 3694 /* ARTIST 0x825730C8 */);

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->PostGameProcessingFinished();
    }
    // ------------------------------------------------------------------------------------
    // The intro of a game mode finished. Logged in, a freeburn-lobby (or showtime) intro
    // that is not a move between lobby modes starts the network time sync. Offline, the
    // online modes other than those two force the start time from the game timer.
    // ------------------------------------------------------------------------------------
    void StateManager::IntroStopped(const BrnGameState::GameStateModuleIO::StopModeIntroAction* lpStopIntroAction)
    {
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 3873 /* ARTIST 0x825492EC */);
        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface(),
            "mpNetworkManager->GetServerInterface()",
            __FILE__, 3874 /* ARTIST 0x82549314 */);
        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface()->GetConnectionComponent(),
            "mpNetworkManager->GetServerInterface()->GetConnectionComponent()",
            __FILE__, 3875 /* ARTIST 0x82549340 */);

        if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            if (!lpStopIntroAction->mbMovingBetweenLobbyModes
                && (lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                    || lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                && mpNetworkManager->IsDoingFreeBurnLobby())
            {
                CGS_ASSERT_AT(!GsmIO::IsShowtimeGameMode(lpStopIntroAction->meGameMode),
                    "!GsmIO::IsShowtimeGameMode(lpStopIntroAction->meGameMode)",
                    __FILE__, 3888 /* ARTIST 0x825493E4 */);

                mpNetworkManager->GetStartTimeManager()->StartSyncingTime(
                    lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY);
                meState = E_STATE_SYNC_TIME;
            }
        }
        else if (!lpStopIntroAction->mbMovingBetweenLobbyModes
                 && !(lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                      || lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                 && lpStopIntroAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START
                 && lpStopIntroAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
        {
            mpNetworkManager->GetStartTimeManager()->ForceStartTime(mpNetworkManager->GetTimerStatus());
            meState = E_STATE_SYNC_TIME;
        }
    }

    // ------------------------------------------------------------------------------------
    // The marked man's assets are loaded. Logged in, a load that is not a move between
    // lobby modes starts the network time sync; offline, the start time is forced from
    // the game timer.
    // ------------------------------------------------------------------------------------
    void StateManager::MarkedManLoaded(const BrnGameState::GameStateModuleIO::MarkedManLoadedAction* lpMarkedManLoadedAction)
    {
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager", __FILE__, 3925 /* ARTIST 0x825494C0 */);
        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface(),
            "mpNetworkManager->GetServerInterface()",
                __FILE__, 3926 /* ARTIST 0x825494E8 */);

        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface()->GetConnectionComponent(),
            "mpNetworkManager->GetServerInterface()->GetConnectionComponent()",
                __FILE__, 3927 /* ARTIST 0x82549514 */);


        if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            if (!lpMarkedManLoadedAction->mbMovingBetweenLobbyModes)
            {
                CGS_ASSERT_AT(!GsmIO::IsShowtimeGameMode(lpMarkedManLoadedAction->meGameMode),
                    "!GsmIO::IsShowtimeGameMode(lpMarkedManLoadedAction->meGameMode)",
                        __FILE__, 3935 /* ARTIST 0x8254957C */);


                mpNetworkManager->GetStartTimeManager()->StartSyncingTime(
                    lpMarkedManLoadedAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY);
                meState = E_STATE_SYNC_TIME;
            }
        }
        else
        {
            mpNetworkManager->GetStartTimeManager()->ForceStartTime(mpNetworkManager->GetTimerStatus());
            meState = E_STATE_SYNC_TIME;
        }
    }

    // ------------------------------------------------------------------------------------
    // The round's post-processing is done: tell the manager the round is over, wait for
    // the next game-mode start, forget every player's marked man and stop host migration
    // for the rest of the game.
    // ------------------------------------------------------------------------------------
    void StateManager::PostRoundProcessingFinished()
    {
        mpNetworkManager->OnRoundFinish();
        meState = E_STATE_WAIT_GAME_MODE_START;

        NetworkPlayerID lPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT_AT(lpMenuData,
                "lpMenuData", __FILE__, 3663 /* ARTIST 0x82549208 */);

            lpMenuData->mMarkedManID = CgsNetwork::KI_INVALID_PLAYER_ID;
        }

        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            mpNetworkManager->GetHostMigrationManager()->Disable(
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu));
        }
    }

    // ------------------------------------------------------------------------------------
    // Tell the game state to start round liRoundIndex: copy that round's route (landmarks
    // and traffic-light trigger) into a start-round event. The host also records the
    // round's event id in telemetry.
    // ------------------------------------------------------------------------------------
    void StateManager::SendStartNextRoundMessage(s32 liRoundIndex)
    {
        GsmIO::StartNetworkRoundEvent lEvent;
        GsmIO::SpecificGameModeEventInterface::Event laEvents[GsmIO::KU_MAX_ONLINE_ROUNDS_IN_MODE];
        NetworkPlayerID lPlayerID;

        mpNetworkManager->GetSelectedRoutesManager()->GetRouteData(laEvents);

        lEvent.miNumLandmarksInRound = laEvents[liRoundIndex].GetNumLandmarks();
        lEvent.mLightTriggerID       = laEvents[liRoundIndex].GetTrafficLightTriggerId();
        for (s32 liLandmarkIndex = 0; liLandmarkIndex < lEvent.miNumLandmarksInRound; ++liLandmarkIndex)
        {
            lEvent.maLandmarks[liLandmarkIndex] = laEvents[liRoundIndex].GetLandmark(liLandmarkIndex);
        }

        mpNetworkModule->GetGameEventQueue()->AddGameEvent(lEvent);

        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lPlayerID);
        CGS_ASSERT_AT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
            "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 4481 /* ARTIST 0x82569F88 */);

        if (mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID)
        {
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_TRACKID,
                                                    laEvents[liRoundIndex].GetEventID());
        }
    }

    // ------------------------------------------------------------------------------------
    // The game's post-processing is done. Still in the game: stop host migration, clear
    // every player's car choice and camera state and fall back into the freeburn lobby.
    // Out of the game: leave (suspending if logged in) and, outside the lobby modes, put
    // the local player back in the freeburn car. Either way tell the game side and drop
    // the cached network player ids.
    // ------------------------------------------------------------------------------------
    void StateManager::PostGameProcessingFinished()
    {
        BrnNetworkModuleIO::NetworkOutPostGameProcessingFinished lPostGameProcessingFinished;

        mpNetworkManager->OnGameFinish();
        meState = E_STATE_COUNT;

        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            mpNetworkManager->GetHostMigrationManager()->Disable(
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu));

            NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                       &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
            {
                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT_AT(lpMenuData,
                    "lpMenuData",
                    CGS_ARTIST_FILE, 3730 /* ARTIST 0x8256ECC8 */);

                lpMenuData->mCarId               = 0;
                lpMenuData->mWheelId             = 0;
                lpMenuData->mu16CarColourIndex   = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
                lpMenuData->mu16PaintFinishIndex = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
                lpMenuData->meCameraStatus       = E_CAMERA_STATUS_NONE;
            }

            StartFreeBurnLobbyGameMode(false, true, false, false);
            lPostGameProcessingFinished.mbIsStillInGame = true;
        }
        else
        {
            if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                SuspendToLeaveGame(E_LEFT_GAME_REASON_LEFT, CgsNetwork::E_KICKREASON_COUNT);
            }

            CGS_ASSERT_AT(mpNetworkModule,
                "mpNetworkModule",
                CGS_ARTIST_FILE, 3755 /* ARTIST 0x8256ED74 */);
            CGS_ASSERT_AT(mpNetworkModule->GetGameStateToNetworkInterface(),
                "mpNetworkModule->GetGameStateToNetworkInterface()",
                CGS_ARTIST_FILE, 3756 /* ARTIST 0x8256EDA0 */);

            const GsmIO::EGameModeType leGameMode =
                mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();
            // ARTIST 0x8256EDB8-0x8256EDD8 preserves the car in the lobby and online showtime.
            const bool lbInOnlineLobby = leGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                || leGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME;
            if (!lbInOnlineLobby)
            {
                GsmIO::ChangePlayerCarEvent lChangeCarEvent;
                lChangeCarEvent.mCarModelId         = mpNetworkManager->GetFreeBurnCarID();
                lChangeCarEvent.mWheelModelId       = mpNetworkManager->GetFreeBurnWheelID();
                lChangeCarEvent.mbResetPlayerCamera = false;
                lChangeCarEvent.mbKeepResetSection  = true;
                GsmIO::GameEventQueue* lpGameEventQueue = mpNetworkModule->GetGameEventQueue();
                lpGameEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeCarEvent),
                                           GsmIO::E_EVENT_CHANGE_PLAYER_CAR,
                                           static_cast<s32>(sizeof(lChangeCarEvent)));
            }

            lPostGameProcessingFinished.mbIsStillInGame = false;
        }

        BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue = mpNetworkModule->GetNetworkEventQueue();
        lpNetworkEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPostGameProcessingFinished),
            lPostGameProcessingFinished.GetEventType(), static_cast<s32>(sizeof(lPostGameProcessingFinished)));
        mpNetworkModule->ClearCachedNetworkPlayerIDsAtGameEnd();
    }

    // ------------------------------------------------------------------------------------
    // An invite arrived. In car or team selection the game is ended (and left) first. Idle,
    // any running game search is cancelled once the games component is free, then the
    // manager waits for the component to settle; once it has, the game state is told the
    // network side is ready for the invite.
    // ------------------------------------------------------------------------------------
    void StateManager::PrepareForInvite()
    {
        if (meState == E_STATE_WAIT_CAR_SELECT || meState == E_STATE_WAIT_TEAM_SELECTION)
        {
            meState = E_STATE_WAIT_POST_ROUND;
            mpNetworkManager->GetPostRoundManager()->StartEndOfGame(true, true, PostGameFinishedCallback, this);
            mpNetworkModule->GetNetworkManager()->SetRound(-1);
        }
        else if (meState == E_STATE_COUNT)
        {
            if (!mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended()
                && mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                        CgsNetwork::E_COMPONENTS_GAMES) != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    return;
                }
                mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();
            }
            meState = E_STATE_PREPARING_FOR_INVITE;
        }
        else if (meState == E_STATE_PREPARING_FOR_INVITE)
        {
            if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                    CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                GsmIO::PreparedForInviteEvent lPreparedForInviteEvent;
                lPreparedForInviteEvent.meModulePreparedForInvite = KI_MODULE_PREPARED_FOR_INVITE_NETWORK;
                mpNetworkModule->GetGameEventQueue()->AddGameEvent(lPreparedForInviteEvent);
                meState = E_STATE_PREPARED_FOR_INVITE;
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Join the game session lpcSessionID (an accepted invite): start the join through the
    // matchmaking manager and tell the GUI a join is under way.
    // ------------------------------------------------------------------------------------
    void StateManager::JoinGameSession(char* lpcSessionID)
    {
        BrnNetworkModuleIO::NetworkOutJoiningGameEvent lJoiningGameEvent;
        GameParams                                     lGameParams;

        mbForceStartFreeburnLobby = false;

        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn(),
            "mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()",
            __FILE__, 3109 /* ARTIST 0x82573004 */);

        lGameParams.Prepare();
        lGameParams.SetSession(lpcSessionID);
        mpNetworkManager->GetMatchMakingManager()->JoinGame(&lGameParams, true, JoinGameFinishedCallback, this);

        mpNetworkModule->GetNetworkEventQueue()->AddEvent(lJoiningGameEvent);
        meState = E_STATE_WAIT_MATCHMAKING;
    }

    // ------------------------------------------------------------------------------------
    // Create a game from the GUI's options: an 8-player fixed game named after the local
    // player, at the local player's rank and locality, then hand it to matchmaking (which
    // leaves the current game first when there is one) and publish the chosen routes.
    // ------------------------------------------------------------------------------------
    void StateManager::CreateGame(const BrnGui::GuiEventNetworkCreateGame* lpGameParamsEvent, bool lbCreatedFromMenus)
    {
        GameParams     lGameParams;
        PlayerInfoData lPlayerInfo;

        mbForceStartFreeburnLobby = false;

        lGameParams.Prepare();
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

        lGameParams.SetName(lPlayerInfo.GetName());
        lGameParams.SetMaxPlayers(KI_CREATE_GAME_MAX_PLAYERS);
        lGameParams.SetMinPlayers(KI_CREATE_GAME_MIN_PLAYERS);
        lGameParams.SetTotalSlots(KI_CREATE_GAME_MAX_PLAYERS, 0);
        lGameParams.SetTimeLimit(lpGameParamsEvent->miTimeLimit);
        lGameParams.SetRagerVehicleLevelLimit(0);
        lGameParams.SetFixedGame(true);
        lGameParams.SetSkillLevel(static_cast<u32>(lPlayerInfo.GetRank()));
        lGameParams.SetInfiniteBoost(lpGameParamsEvent->mbInfiniteBoost);
        lGameParams.SetNumberRounds(lpGameParamsEvent->miNumRounds);
        lGameParams.SetVehicleLevelLimit(lpGameParamsEvent->miVehicleClass);
        lGameParams.SetRankedGame(lpGameParamsEvent->mbRanked);
        lGameParams.SetSecurity(lpGameParamsEvent->meSecurity);
        lGameParams.SetGameMode(lpGameParamsEvent->meGameMode);
        lGameParams.SetTrafficOn(lpGameParamsEvent->mbTrafficOn);
        lGameParams.SetTrafficCheckingOn(lpGameParamsEvent->mbTrafficCheckingOn);
        lGameParams.SetBoostType(lpGameParamsEvent->meBoostType);
        lGameParams.SetNumRunnerCrashes(lpGameParamsEvent->miNumRunnerCrashes);
        lGameParams.SetVehicleChoice(lpGameParamsEvent->meVehicleChoice);
        lGameParams.SetLocality(lPlayerInfo.GetLocality());

        mpNetworkManager->GetMatchMakingManager()->CreateGame(&lGameParams, CreateGameFinishedCallback, this);

        mpNetworkManager->GetSelectedRoutesManager()->SetRouteData(
            lpGameParamsEvent->miNumRounds, lpGameParamsEvent->maEvents,
            static_cast<GsmIO::EGameModeType>(lpGameParamsEvent->meGameMode));

        mbCreatedFromMenus = lbCreatedFromMenus;
        meState            = E_STATE_WAIT_MATCHMAKING;
    }

    // ------------------------------------------------------------------------------------
    // The host changed the game's options: re-apply them over the current game parameters
    // (with the local player's rank and locality), set the session flags a closed game
    // needs, push the parameters to the server and publish the chosen routes.
    // ------------------------------------------------------------------------------------
    void StateManager::ModifyGame(const BrnGui::GuiEventNetworkGameParams* lpGameParamsEvent)
    {
        GameParams     lGameParams;
        PlayerInfoData lPlayerInfo;

        lGameParams.Prepare();
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lGameParams.SetSkillLevel(static_cast<u32>(lPlayerInfo.GetRank()));
        lGameParams.SetInfiniteBoost(lpGameParamsEvent->mbInfiniteBoost);
        lGameParams.SetNumberRounds(lpGameParamsEvent->miNumRounds);
        lGameParams.SetVehicleLevelLimit(lpGameParamsEvent->miVehicleClass);
        lGameParams.SetSecurity(lpGameParamsEvent->meSecurity);
        lGameParams.SetGameMode(lpGameParamsEvent->meGameMode);
        lGameParams.SetTrafficOn(lpGameParamsEvent->mbTrafficOn);
        lGameParams.SetTrafficCheckingOn(lpGameParamsEvent->mbTrafficCheckingOn);
        lGameParams.SetBoostType(lpGameParamsEvent->meBoostType);
        lGameParams.SetNumRunnerCrashes(lpGameParamsEvent->miNumRunnerCrashes);
        lGameParams.SetVehicleChoice(lpGameParamsEvent->meVehicleChoice);
        lGameParams.SetTimeLimit(lpGameParamsEvent->miTimeLimit);
        lGameParams.SetLocality(lPlayerInfo.GetLocality());

        // One call per arm (X360-only code, absent from the PS3 DWARF): ARTIST 0x825693F4 loads the
        // games component above the branch, the two arms' identical prefix hoisted.
        if (lGameParams.Security() == KI_GAME_SECURITY_CLOSED)
        {
            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
                ->SetSessionFlags(KI_SESSION_FLAGS_CLOSED_GAME);
        }
        else
        {
            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
                ->SetSessionFlags(KI_SESSION_FLAGS_OPEN_GAME);
        }
        mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdateGameParameters(&lGameParams);

        mpNetworkManager->GetSelectedRoutesManager()->SetRouteData(
            lpGameParamsEvent->miNumRounds, lpGameParamsEvent->maEvents,
            static_cast<GsmIO::EGameModeType>(lpGameParamsEvent->meGameMode));

        meState = E_STATE_WAIT_MODIFY_GAME;
    }

    // ------------------------------------------------------------------------------------
    // The launch went through: build the start-game event from the game parameters and
    // every finalised player's menu choices (a player with no car, wheel, colour or paint
    // takes the first player's), remember each player's XUID, send it with the first
    // round's route, set the round counter and let the host record the game in telemetry.
    // ------------------------------------------------------------------------------------
    void StateManager::StartGameMode()
    {
        PlayerParams                 lPlayerParams;
        GameParams                   lGameParams;
        PlayerMenuData*              lpMenuData;
        PlayerMenuData*              lpHostMenuData;
        CgsID                        lFirstCarID                  = 0;
        CgsID                        lFirstWheelID                = 0;
        GsmIO::StartNetworkGameEvent lEvent;
        s32                          liGridPosition;
        f32                          lfTimeLimit;
        u16                          lu16FirstCarColourIndex      = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
        u16                          lu16FirstCarPaintFinishIndex = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
        NetworkPlayerID              lLocalPlayerID;
        NetworkPlayerID              lPlayerID                    = CgsNetwork::K_INVALID_PLAYER_ID;

        for (s32 liSlot = 0; liSlot < CurrentPlayerXUIDs::KI_NUM_SLOTS; ++liSlot)
        {
            mCurrentPlayerXUIDs.maSlots[liSlot].miPlayerId = CurrentPlayerXUIDs::KI_FREE_SLOT;
            mCurrentPlayerXUIDs.maSlots[liSlot].mu64XUID   = 0;
        }

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.Clear();
        lEvent.meGameMode          = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
        lEvent.muRandomSeedForGame = lGameParams.GetRandomSeed();
        lEvent.miNumRaceCars       = mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
            CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
        lEvent.miNumRounds         = lGameParams.NumberRounds();

        lfTimeLimit = static_cast<f32>((lGameParams.TimeLimit() + 1) * KI_TIME_LIMIT_STEP);
        CGS_ASSERT(lfTimeLimit >= KF_TIME_MIN, "lfTimeLimit >= KF_TIME_MIN");
        CGS_ASSERT(lfTimeLimit <= KF_TIME_MAX, "lfTimeLimit <= KF_TIME_MAX");

        // FLAG: the reference fills these through the event's inline SetGameData / SetTimeLimit /
        // SetLocalNetworkPlayerID, which have no home yet; their stores are written by name.
        lEvent.miNumRunnerCrashes         = lGameParams.NumRunnerCrashes();
        lEvent.meBoostType                = lGameParams.BoostType();
        lEvent.mbIsTrafficCheckingOn      = lGameParams.IsTrafficCheckingOn();
        lEvent.mbRedTeamHaveInfiniteBoost = lGameParams.InfiniteBoost();
        lEvent.mbIsTrafficOn              = lGameParams.IsTrafficOn();
        lEvent.mbIsRanked                 = lGameParams.IsRankedGame();
        lEvent.mfTimeLimit                = lfTimeLimit * KF_SECONDS_PER_MINUTE;

        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        lEvent.mLocalNetworkPlayerID = lLocalPlayerID;

        lPlayerID      = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
        lpHostMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
        CGS_ASSERT(lpHostMenuData, "lpHostMenuData");

        // Pick the first choice made for each of car, wheel, colour and paint, and note every
        // player's XUID on the way.
        lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            u64 lu64XUID;

            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                    &lPlayerParams);
            lpMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));

            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
                ->GetPlayerXUIDByID(lPlayerID, &lu64XUID);
            mCurrentPlayerXUIDs.SetXUID(lPlayerID, lu64XUID);

            if (lFirstCarID == 0 && lpMenuData->mCarId != 0)
            {
                lFirstCarID = lpMenuData->mCarId;
            }
            if (lFirstWheelID == 0 && lpMenuData->mWheelId != 0)
            {
                lFirstWheelID = lpMenuData->mWheelId;
            }
            if (lu16FirstCarColourIndex == PlayerMenuData::KU_INVALID_COLOUR_INDEX
                && lpMenuData->mu16CarColourIndex != PlayerMenuData::KU_INVALID_COLOUR_INDEX)
            {
                lu16FirstCarColourIndex = lpMenuData->mu16CarColourIndex;
            }
            if (lu16FirstCarPaintFinishIndex == PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX
                && lpMenuData->mu16PaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                lu16FirstCarPaintFinishIndex = lpMenuData->mu16PaintFinishIndex;
            }

            // The console stops once car, wheel and paint are known (the colour is not tested).
            if (lFirstCarID != 0 && lFirstWheelID != 0
                && lu16FirstCarPaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                break;
            }
        }

        CGS_ASSERT((lFirstCarID != 0), "(lFirstCarID != 0)");

        // Fill the gaps from the first choices and put every player on the grid in turn.
        lPlayerID      = CgsNetwork::K_INVALID_PLAYER_ID;
        liGridPosition = 0;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                    &lPlayerParams);
            lpMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));

            if (lpMenuData->mCarId == 0)
            {
                lpMenuData->mCarId = lFirstCarID;
            }
            if (lpMenuData->mWheelId == 0)
            {
                lpMenuData->mWheelId = lFirstWheelID;
            }
            if (lpMenuData->mu16CarColourIndex == PlayerMenuData::KU_INVALID_COLOUR_INDEX)
            {
                lpMenuData->mu16CarColourIndex = lu16FirstCarColourIndex;
            }
            if (lpMenuData->mu16PaintFinishIndex == PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                lpMenuData->mu16PaintFinishIndex = lu16FirstCarPaintFinishIndex;
            }

            CGS_ASSERT(lpMenuData->mu16CarColourIndex != PlayerMenuData::KU_INVALID_COLOUR_INDEX,
                       "Invalid car colour index\n");
            CGS_ASSERT(lpMenuData->mu16PaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX,
                       "Invalid paint palette index\n");

            lEvent.SetPlayerData(liGridPosition, lPlayerID, lpMenuData->mCarId, lpMenuData->mWheelId,
                                 lpMenuData->mu16CarColourIndex, lpMenuData->mu16PaintFinishIndex,
                                 lpMenuData->mfUnknown48, lPlayerParams.GetPlayerTeam(),
                                 mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID,
                                 lPlayerParams.HasFever());
            ++liGridPosition;
        }

        mpNetworkModule->GetGameEventQueue()->AddGameEvent(lEvent);

        SendStartNextRoundMessage(0);

        if (GsmIO::IsOnlineFreeBurnLobby(lEvent.meGameMode))
        {
            mpNetworkModule->GetNetworkManager()->SetRound(-1);
        }
        else
        {
            mpNetworkModule->GetNetworkManager()->SetRound(0);
        }

        lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lPlayerID);
        CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        if (mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID)
        {
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_PLAYERS_IN_GAME, lEvent.miNumRaceCars);
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_NUMBER_OF_ROUNDS, lEvent.miNumRounds);
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_IS_RANKED,
                                                    lEvent.mbIsRanked ? "true" : "false");
        }
    }

    // ------------------------------------------------------------------------------------
    // Drain the frame's event queues: GUI events, then game-state actions, then network
    // events, checking between each that no memory was allocated.
    // ------------------------------------------------------------------------------------
    void StateManager::UpdateEvents(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        u32 luMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessGuiEvents(lpInput->GetGuiEventQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessGameStateActions(lpInput->GetGameActionQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessNetworkEvents(lpInput->GetNetworkEventQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);
    }

    // ------------------------------------------------------------------------------------
    // ProcessGuiEvents
    //
    // Walk the frame's GUI out-events and act on the network ones: suspension, the login
    // questions, leaving / launching / joining / creating / modifying games, the game-room
    // player options, stats and picture requests, news and terms-of-service, the splash
    // screen and the connect request.
    //
    void StateManager::ProcessGuiEvents(const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        const CgsModule::Event* lpEvent     = 0;
        s32                     liEventSize = 0;
        s32                     liEventId   = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != 0)
        {
            const CgsModule::Event* lpRecord  = lpEvent;
            const void*             lpPayload = lpEvent;
            s32                     liCommand = liEventId;
            const s32               liHeaderSize = static_cast<s32>(sizeof(CgsGui::GuiEvent<0>));
            if (liEventId == KI_GUI_CHANNEL_OUT && liEventSize >= liHeaderSize)
            {
                const u32* lpuRecord = reinterpret_cast<const u32*>(lpEvent);
                liCommand = static_cast<s32>(lpuRecord[1]);
                const u32 luOffset = lpuRecord[2];
                if (luOffset >= static_cast<u32>(liHeaderSize) && static_cast<s32>(luOffset) < liEventSize)
                {
                    lpPayload = reinterpret_cast<const u8*>(lpEvent) + luOffset;
                }
            }

            switch (liCommand)
            {
            case KI_GUI_EVENT_LOCALISED_TEXT_POINTER_REMOVED:
            {
                // The GUI dropped a downloaded text: free our copy of it.
                const CgsGui::GuiEventLocalisedTextPointerRemoved* lpTextRemovedEvent =
                    static_cast<const CgsGui::GuiEventLocalisedTextPointerRemoved*>(lpPayload);
                CGS_ASSERT(lpTextRemovedEvent, "lpTextRemovedEvent");

                if (lpTextRemovedEvent->IsThisTheRemovedString("TOS_TEXT"))
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(mpTOS, 0, 0);
                    mpTOS = 0;
                }
                else if (lpTextRemovedEvent->IsThisTheRemovedString("NEWS_TEXT"))
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(mpNews, 0, 0);
                    mpNews = 0;
                }
                break;
            }

            case KI_GUI_EVENT_LOADING_SCREEN_STATE:
            {
                const CgsGui::GuiEventLoadingScreenState* lpLoadingScreenStateEvent =
                    static_cast<const CgsGui::GuiEventLoadingScreenState*>(lpPayload);
                CGS_ASSERT(lpLoadingScreenStateEvent, "lpLoadingScreenStateEvent");

                mbLoadingScreenVisible = lpLoadingScreenStateEvent->mbIsVisible;
                break;
            }

            case KI_GUI_EVENT_NETWORK_SUSPENSION:
            {
                // ARTIST 0x82578510: the GUI router has unwrapped this event; r25
                // points at its four-byte payload, and lwz reads the type at +0.
                const CgsGui::GuiEventNetworkSuspension::ESuspensionType leSuspensionType =
                    *static_cast<const CgsGui::GuiEventNetworkSuspension::ESuspensionType*>(lpPayload);

                switch (leSuspensionType)
                {
                case CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND:
                    if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                        && !mpNetworkManager->GetLoginManager()->IsSigningIn()
                        && !mpNetworkManager->GetSuspensionManager()->IsSuspending()
                        && !mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended()
                        && !mpNetworkManager->IsDoingFreeBurnLobby())
                    {
                        meState = E_STATE_WAIT_SUSPENSION;
                        // FLAG: Suspend is expanded inline here and both suspension types yield the
                        // same interface flags, so the type argument cannot be read back; OFFLINE is
                        // assumed (as in SuspendToLeaveGame).
                        mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                          SuspensionFinishedCallback, this);
                    }
                    else if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                             && !mpNetworkManager->GetLoginManager()->IsSigningIn()
                             && !mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended()
                             && !mpNetworkManager->IsDoingFreeBurnLobby())
                    {
                        // The suspension manager is busy: suspend once it goes idle.
                        meState = E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND;
                    }
                    // FLAG: otherwise the console logs "SUSPENDING: faied as either logging in,
                    // signing in or already suspended\n" to a network debug stream that has no
                    // home in this tree; the log line is not reproduced.
                    break;

                case CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_RESUME:
                    if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                        && !mpNetworkManager->GetSuspensionManager()->IsSuspending()
                        && mpNetworkManager->GetServerInterface()->NativeServerInterface::IsSuspended())
                    {
                        meState = E_STATE_WAIT_SUSPENSION;
                        mpNetworkManager->GetSuspensionManager()->Resume(ResumeFinishedCallback, this);
                    }
                    break;

                default:
                    CGS_FAIL("Invalid suspension type");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_ANSWER_LOGIN_QUESTION:
            {
                const CgsGui::GuiEventAnswerLoginQuestion* lpAnswerQuestionEvent =
                    static_cast<const CgsGui::GuiEventAnswerLoginQuestion*>(lpPayload);
                LoginManagerBase* lpLoginManager = mpNetworkManager->GetLoginManager();

                switch (lpAnswerQuestionEvent->meLoginQuestion)
                {
                case CgsGui::E_LOGIN_QUESTION_TOS:
                    lpLoginManager->AnswerAgreeTOS(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT:
                    lpLoginManager->AnswerCreateAccount(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_SHARE:
                    lpLoginManager->AnswerShareInfo(lpAnswerQuestionEvent->mbAccept, lpAnswerQuestionEvent->mbAccept2);
                    break;
                case CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT:
                    lpLoginManager->AnswerOpenUsAccount(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT:
                    lpLoginManager->AnswerNoAgreement(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN:
                    lpLoginManager->AnswerSignIn(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION:
                    lpLoginManager->AnswerChatRestricted(lpAnswerQuestionEvent->mbAccept);
                    break;
                default:
                    CGS_FAIL("Invalid login question");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_LEAVE_GAME:
            {
                // Leave the online game for an offline one, unless the game is locked or the
                // games component is busy.
                if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                        CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE
                    && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                    && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameLocked())
                {
                    CgsGui::GuiEvent<KI_GUI_EVENT_NETWORK_LEAVING_GAME> lLeavingGameEvent;

                    mpNetworkManager->GetMatchMakingManager()->StartProcess(
                        MatchMakingManager::E_PROCESS_LEAVE_GAME, LeaveGameForOfflineGameFinishedCallback, this);
                    meState = E_STATE_WAIT_MATCHMAKING;
                    mpNetworkModule->AddOutputGuiEvent(lLeavingGameEvent);
                }
                else
                {
                    BrnGui::GuiEventNetworkLeavingGameFailed lLeavingGameFailedEvent;
                    mpNetworkModule->AddOutputGuiEvent(lLeavingGameFailedEvent);
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_LAUNCH:
            {
                if (meState != E_STATE_COUNT)
                {
                    break;
                }

                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost())
                {
                    // The host launches once there are enough players, all connected, all with
                    // the routes, in enough teams and none still playing.
                    GameParams                               lGameParams;
                    BrnNetworkModuleIO::NetworkOutLaunchEvent lNetworkLaunchEvent;
                    CgsGui::GuiEventNetworkLaunched          lLaunchedEvent;
                    bool                                     lbPlayerStillPlaying = false;
                    PlayerParams                             lPlayerParams;

                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame()
                        < 2)
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_NOT_ENOUGH_PLAYERS);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_STILL_CONNECTING);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!mpNetworkManager->GetSelectedRoutesManager()->HaveAllPlayersReceivedRoutes())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_WAITING_FOR_ROUTES);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!GameModeHasEnoughTeams())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_NOT_ENOUGH_TEAMS);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else
                    {
                        for (s32 liPlayerIndex = 0;
                             liPlayerIndex < mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame();
                             ++liPlayerIndex)
                        {
                            lPlayerParams.Prepare();
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(
                                liPlayerIndex, &lPlayerParams);
                            if (lPlayerParams.IsPlaying())
                            {
                                lbPlayerStillPlaying = true;
                                break;
                            }
                        }

                        if (lbPlayerStillPlaying)
                        {
                            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_STILL_PLAYING);
                            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                        }
                        else
                        {
                            lGameParams.Prepare();
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                            lNetworkLaunchEvent.meGameModeType = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
                            lNetworkLaunchEvent.mHostPlayerID  = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
                            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkLaunchEvent);

                            mpNetworkManager->GetLaunchManager()->Start();
                            mpNetworkManager->OnGameLaunching();
                            meState = E_STATE_LAUNCHING;
                            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_STARTED, 0);
                        }
                    }
                }
                else if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                             CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    // A client toggles its ready flag.
                    PlayerParams    lPlayerParams;
                    NetworkPlayerID lLocalNetworkPlayerID;

                    const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                    CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                    lPlayerParams.Prepare();
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                        lLocalNetworkPlayerID, &lPlayerParams);
                    lPlayerParams.SetReady(!lPlayerParams.IsReady());
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdatePlayerParameters(
                        lLocalNetworkPlayerID, &lPlayerParams);
                    meState = E_STATE_WAIT_SERVER_INTERFACE_ACTION;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_FORCE_LAUNCH:
            {
                GameParams                                lGameParams;
                BrnNetworkModuleIO::NetworkOutLaunchEvent lNetworkLaunchEvent;

                lGameParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                lNetworkLaunchEvent.meGameModeType = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
                lNetworkLaunchEvent.mHostPlayerID  = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(lNetworkLaunchEvent);

                mpNetworkManager->GetLaunchManager()->Start();
                mpNetworkManager->OnGameLaunching();
                meState = E_STATE_LAUNCHING;
                break;
            }

            case KI_GUI_EVENT_AUTOSAVE_STARTED:
                mbAreWeAutosaving = true;
                break;

            case KI_GUI_EVENT_AUTOSAVE_FINISHED:
                mbAreWeAutosaving = false;
                break;

            case KI_GUI_EVENT_NETWORK_SELECTED_PLAYER_OPTION:
            {
                const BrnGui::GuiEventNetworkSelectedPlayerOption* lpSelectedPlayerOptionEvent =
                    static_cast<const BrnGui::GuiEventNetworkSelectedPlayerOption*>(lpPayload);

                if (!(meState == E_STATE_COUNT || meState == E_STATE_SYNC_TIME || meState == E_STATE_RACING
                      || meState == E_STATE_WAIT_GAME_MODE_START))
                {
                    break;
                }
                // Once the round is syncing or racing nobody can be kicked.
                if ((meState == E_STATE_SYNC_TIME || meState == E_STATE_RACING)
                    && lpSelectedPlayerOptionEvent->meOptionSelected
                           == BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_KICK_PLAYER)
                {
                    break;
                }

                switch (lpSelectedPlayerOptionEvent->meOptionSelected)
                {
                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_MARK_PLAYER:
                    if (!mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        // In the lobby the mark lives in the player parameters: marking the
                        // player already marked clears it.
                        PlayerParams    lLocalPlayerParams;
                        PlayerParams    lPlayerParams;
                        NetworkPlayerID lLocalNetworkPlayerID;

                        const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                        CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                        lLocalPlayerParams.Prepare();
                        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                            lLocalNetworkPlayerID, &lLocalPlayerParams);
                        lPlayerParams.Prepare();
                        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                            lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lPlayerParams);

                        if (lLocalPlayerParams.GetMarkedPlayerID() == lPlayerParams.GetID())
                        {
                            UpdateMarkedMan(CgsNetwork::K_INVALID_PLAYER_ID);
                        }
                        else
                        {
                            UpdateMarkedMan(lPlayerParams.GetID());
                        }
                    }
                    else if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        // In a game the mark lives in the local menu data and goes to everyone.
                        NetworkPlayerID lLocalNetworkPlayerID;
                        PlayerMenuData* lpMenuData;

                        const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                        CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                        lpMenuData = static_cast<PlayerMenuData*>(
                            mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalNetworkPlayerID));
                        CGS_ASSERT(lpMenuData, "lpMenuData");

                        if (lpMenuData->mMarkedManID == lpSelectedPlayerOptionEvent->mSelectedPlayerID)
                        {
                            lpMenuData->mMarkedManID = CgsNetwork::K_INVALID_PLAYER_ID;
                        }
                        else
                        {
                            lpMenuData->mMarkedManID = lpSelectedPlayerOptionEvent->mSelectedPlayerID;
                        }
                        mpNetworkManager->GetMarkedManManager()->SendMarkedManDataToAll(false);
                    }
                    break;

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_VIEW_GAMERCARD:
                {
                    u64 lu64XUID;
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsPlayerInGame(
                               lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(
                            mpNetworkManager->GetServerInterface()->GetGameComponent())
                            ->GetPlayerXUIDByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    else
                    {
                        mCurrentPlayerXUIDs.GetXUID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    XShowGamerCardUI(static_cast<u32>(mpNetworkManager->GetActiveControllerPort()), lu64XUID);
                    break;
                }

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_SUBMIT_REVIEW:
                {
                    u64 lu64XUID;
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsPlayerInGame(
                               lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(
                            mpNetworkManager->GetServerInterface()->GetGameComponent())
                            ->GetPlayerXUIDByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    else
                    {
                        mCurrentPlayerXUIDs.GetXUID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    XShowPlayerReviewUI(static_cast<u32>(mpNetworkManager->GetActiveControllerPort()), lu64XUID);
                    break;
                }

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_KICK_PLAYER:
                {
                    // A player in our userset is kicked from the userset, anyone else from the game.
                    CgsNetwork::ServerInterfaceGames* lpGames = static_cast<CgsNetwork::ServerInterfaceGames*>(
                        mpNetworkManager->GetServerInterface()->NativeServerInterface::GetGameComponent());
                    CgsNetwork::ServerInterfaceUsersets* lpUsersets = static_cast<CgsNetwork::ServerInterfaceUsersets*>(
                        mpNetworkManager->GetServerInterface()->GetUsersetsComponent());

                    CGS_ASSERT(lpGames, "lpGames");
                    CGS_ASSERT(lpUsersets, "lpUsersets");

                    if (lpUsersets->IsPlayerInOurUserset(lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                                CgsNetwork::E_COMPONENTS_USERSETS) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                        {
                            lpUsersets->KickPlayerByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID,
                                                       CgsNetwork::E_KICKREASON_HOSTKICK);
                        }
                    }
                    else if (mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                                 CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                    {
                        lpGames->KickPlayerByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID,
                                                CgsNetwork::E_KICKREASON_HOSTKICK, false);
                    }
                    break;
                }

                default:
                    CGS_FAIL("Invalid player option");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_HIGHLIGHTED_PLAYER:
            {
                // Watch the highlighted player's camera and show their stats.
                const BrnGui::GuiEventNetworkHighlightedPlayer* lpHighlightedPlayerEvent =
                    static_cast<const BrnGui::GuiEventNetworkHighlightedPlayer*>(lpPayload);

                mpNetworkManager->GetCamera()->RequestFeed(lpHighlightedPlayerEvent->mPlayerID);
                mpNetworkManager->OutputPlayerStatsToGui(lpHighlightedPlayerEvent->mPlayerID);
                break;
            }

            case KI_GUI_EVENT_NETWORK_QUIT_PLAYING:
            {
                // FLAG: the console first logs "***** NetworkManager got a
                // E_GUI_NETWORK_QUIT_PLAYING_EVENT - SO SEND PlayerExitedModeEvent\n" to the
                // unhomed network debug stream; the log line is not reproduced.
                GsmIO::PlayerExitedModeEvent lPlayerExitedModeEvent;
                mpNetworkModule->GetGameEventQueue()->AddGameEvent(lPlayerExitedModeEvent);
                break;
            }

            case KI_GUI_EVENT_NETWORK_QUICK_MATCH:
            {
                const BrnGui::GuiEventNetworkQuickMatch* lpQuickMatchEvent =
                    static_cast<const BrnGui::GuiEventNetworkQuickMatch*>(lpPayload);

                mbForceStartFreeburnLobby = false;
                mpNetworkManager->GetMatchMakingManager()->QuickJoinGame(
                    lpQuickMatchEvent->mbRanked, lpQuickMatchEvent->mbFreeburn, QuickJoinFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_SEARCH:
            {
                // Search for games matching the GUI's filters at the local player's rank.
                const BrnGui::GuiEventNetworkCustomMatchSearch* lpCustomMatchSearchEvent =
                    static_cast<const BrnGui::GuiEventNetworkCustomMatchSearch*>(lpRecord);
                GameSearchParams lSearchParams;
                PlayerParams     lPlayerParams;
                PlayerInfoData   lPlayerInfo;

                lPlayerParams.Prepare();
                lPlayerInfo.Prepare();
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

                lSearchParams.Prepare(lpCustomMatchSearchEvent->meGameMode, E_GAMESTATE_WAITING_FOR_PLAYERS,
                                      static_cast<u32>(lPlayerInfo.GetRank()),
                                      lpCustomMatchSearchEvent->meSearchOpponentTypes,
                                      lpCustomMatchSearchEvent->mbRanked, lpCustomMatchSearchEvent->mbFreeburn,
                                      KU_SEARCH_REQUIRED_SLOTS, lPlayerParams.GetFirewallSettings(),
                                      mpNetworkManager, KU_SEARCH_FIELD_29C);
                lSearchParams.SetReturnPlayers(true);

                mpNetworkManager->GetMatchMakingManager()->SearchForGames(&lSearchParams, SearchFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CANCEL_SEARCH:
                if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();
                }
                break;

            case KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_JOIN:
            {
                // Join one of the games the last search found.
                const BrnGui::GuiEventNetworkCustomMatchJoin* lpCustomMatchJoinEvent =
                    static_cast<const BrnGui::GuiEventNetworkCustomMatchJoin*>(lpPayload);
                GameParams lGameParams;

                lGameParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetFoundGame(
                    GetPayloadWord(lpCustomMatchJoinEvent), &lGameParams);
                mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();

                mbForceStartFreeburnLobby = false;
                mpNetworkManager->GetMatchMakingManager()->JoinGame(&lGameParams, false, JoinGameFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CREATE_GAME:
                CreateGame(static_cast<const BrnGui::GuiEventNetworkCreateGame*>(lpPayload), true);
                break;

            case KI_GUI_EVENT_NETWORK_GAME_PARAMS:
                CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame(),
                           "mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()");
                ModifyGame(static_cast<const BrnGui::GuiEventNetworkGameParams*>(lpPayload));
                break;

            case KI_GUI_EVENT_NETWORK_REQUEST_LOCAL_STATS:
            {
                if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    PlayerInfoData lPlayerInfo;

                    lPlayerInfo.Prepare();
                    mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                    mpNetworkManager->OutputPlayerStatsToGui(lPlayerInfo.GetID());
                }
                else
                {
                    // Offline: show the stats cached for the local player, if they are current.
                    const NetworkPlayerStats* lpPlayerStats = mpNetworkManager->GetStatsManager()->GetLocalPlayerStats();
                    if (lpPlayerStats != 0 && lpPlayerStats->GetStatus() == NetworkPlayerStats::E_STATS_AGE_CURRENT)
                    {
                        BrnGui::GuiEventNetworkPlayerStats lPlayerStatsEvent;
                        const char* lpcPlayerName = lpPlayerStats->GetName();

                        CGS_ASSERT(std::strlen(lpcPlayerName) < sizeof(lPlayerStatsEvent.macPlayerName),
                                   "String too long: ");
                        std::strncpy(lPlayerStatsEvent.macPlayerName, lpcPlayerName,
                                     sizeof(lPlayerStatsEvent.macPlayerName));
                        lPlayerStatsEvent.mPlayerID = 0;
                        static_cast<NetworkPlayerStats&>(lPlayerStatsEvent) = *lpPlayerStats;
                        lPlayerStatsEvent.miWorldRank = 0;
                        mpNetworkModule->AddOutputGuiEvent(lPlayerStatsEvent);
                    }
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_REQUEST_GAME_PARAMS:
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    mpNetworkManager->OutputGameParameters();
                }
                break;

            case KI_GUI_EVENT_NETWORK_READY_TO_JOIN_SESSION:
                mbReadyToJoinGameSession = true;
                break;

            case KI_GUI_EVENT_NETWORK_OUTPUT_PLAYER_TEXTURE:
            {
                // Which picture to output (and for whom). The game camera and the mugshots
                // take turns: outputting a picture turns the mugshots off.
                const BrnGui::GuiEventNetworkOutputPlayerTexture* lpOutputPlayerTextureEvent =
                    static_cast<const BrnGui::GuiEventNetworkOutputPlayerTexture*>(lpPayload);

                meOutputPlayerTexture    = static_cast<s32>(lpOutputPlayerTextureEvent->meOutput);
                mNetworkPlayerIDToOutput = lpOutputPlayerTextureEvent->mPlayerIDToOutput;

                mpNetworkManager->GetCamera()->SetGameEnabled(
                    meOutputPlayerTexture != BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);
                mpNetworkManager->GetNetworkImageManager()->EnableMugshotOutput(
                    meOutputPlayerTexture == BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);

                if (meOutputPlayerTexture == BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(0, -1);
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_SWITCH_TEAM:
            {
                // In the lobby of a team mode, move the local player to the other team.
                if (meState == E_STATE_COUNT
                    && GameModeHasTeams()
                    && mpNetworkManager->GetServerInterface()->NativeServerInterface::GetStatus(
                           CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE
                    && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                {
                    NetworkPlayerID lLocalPlayerID;
                    PlayerParams    lPlayerParams;

                    lPlayerParams.Prepare();
                    const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
                    CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                        lLocalPlayerID, &lPlayerParams);

                    if (lPlayerParams.GetPlayerTeam() == GsmIO::E_PLAYER_TEAM_RED_TEAM)
                    {
                        lPlayerParams.SetPlayerTeam(GsmIO::E_PLAYER_TEAM_BLUE_TEAM);
                    }
                    else if (lPlayerParams.GetPlayerTeam() == GsmIO::E_PLAYER_TEAM_BLUE_TEAM)
                    {
                        lPlayerParams.SetPlayerTeam(GsmIO::E_PLAYER_TEAM_RED_TEAM);
                    }

                    mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdatePlayerParameters(lLocalPlayerID,
                                                                                                      &lPlayerParams);
                    meState = E_STATE_WAIT_SERVER_INTERFACE_ACTION;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_NEWS_AND_TOS:
            {
                const BrnGui::GuiEventNetworkNewsAndTOS* lpNewsAndTOSEvent =
                    static_cast<const BrnGui::GuiEventNetworkNewsAndTOS*>(lpPayload);
                CGS_ASSERT(lpNewsAndTOSEvent, "lpNewsAndTOSEvent");

                if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_REQUEST_NEWS)
                {
                    meState = E_STATE_WAIT_DOWNLOAD_NEWS_IDLE;
                }
                else if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_REQUEST_TOS)
                {
                    meState = E_STATE_WAIT_DOWNLOAD_TOS_IDLE;
                }
                else if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_RELEASE)
                {
                    ReleaseNewsAndTOSDownload();
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_SPLASH:
            {
                // The splash screen has finished: in a game the local player's marking is final.
                const BrnGui::GuiEventNetworkSplashEvent* lpSplashEvent =
                    static_cast<const BrnGui::GuiEventNetworkSplashEvent*>(lpPayload);
                CGS_ASSERT(lpSplashEvent, "lpSplashEvent");

                if (lpSplashEvent->meSplashState == BrnGui::GuiEventNetworkSplashEvent::E_SPLASH_STATE_FINISHED)
                {
                    NetworkPlayerID lLocalPlayerID;

                    CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
                    CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent(),
                               "mpNetworkManager->GetServerInterface()->GetGameComponent()");
                    CGS_ASSERT(mpNetworkManager->GetPlayerManager(), "mpNetworkManager->GetPlayerManager()");

                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
                    {
                        mpNetworkManager->GetMarkedManManager()->SendMarkedManDataToAll(true);
                        mpNetworkManager->GetMarkedManManager()->MarkingFinished();
                    }
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_CONNECT:
                HandleConnectEvent(static_cast<const BrnGui::GuiEventNetworkConnect*>(lpRecord));
                break;

            default:
                break;
            }

            liEventId = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }

} // namespace BrnNetwork

#undef CGS_ARTIST_FILE
