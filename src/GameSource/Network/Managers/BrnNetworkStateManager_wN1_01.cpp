#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include <cstring>                                                                         // strstr
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // AmIHost
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"                // (Un)RegisterHostMigrationCallback
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/Network/BrnNetworkModule.h"                                           // GetNetworkManager / GetNetworkEventQueue / GetGameStateToNetworkInterface
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOutInstantFreeburnEvent

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
} // namespace BrnNetwork
