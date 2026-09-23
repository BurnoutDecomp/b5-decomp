// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- recovered function bodies (part 1)
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.cpp
//
// This TU holds the constructor, the slot lookups, the lifecycle edges (Prepare, OnAutoLogin,
// OnEnterGame, OnGameLaunching, OnGameFinish, OnRoundFinish, OnLeaveGame), the Attempt*/Start*
// kick-offs, the two download steps, ProcessGameActions and ProcessAfterSimulation. The
// Construct/Destruct/Release pair, the per-frame send path, the per-player registration and the
// message / custom-command callbacks live in BrnNetworkRoadRulesManager_wN2_01.cpp.
// Member layout and console offsets: see the class banner in the header.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameSource/Network/BrnNetworkModule.h"                                    // BrnNetworkModule (GetGameStateToNetworkInterface / GetNetworkManager / GetNetworkEventQueue)
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager::OnAutoLoginProcessComplete
#include "GameSource/Network/BrnNetworkModuleIO.h"                                  // PostSimulationInputBuffer::GetNetworkEventQueue / GetGameActionQueue
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                          // NetworkOutRoadRulesConnectedOnlineEvent
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface component accessors
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"      // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameSource/GameState/BrnGameStateSharedIO.h"                              // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h" // ServerInterfaceServerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection::IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames::IsLocalPlayerInGame / IsGameStarted
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"         // ServerInterfaceCustomCommands::GetRoadRulesHighScores / GetLocalRoadRulesHighScores
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue<14000,16> / <13312,16>

namespace BrnNetwork
{
    namespace
    {
        // Max scores requested per download batch.
        const s32 KI_NUM_SCORES_TO_DOWNLOAD_IN_A_BATCH = 10;

        // Game-action ids ProcessGameActions reacts to.
        const s32 KI_GAME_ACTION_PREPARE_FOR_MODE = 23;
        const s32 KI_GAME_ACTION_STOP_MODE        = 39;
    }

    // -----------------------------------------------------------------------------
    // The C++ constructor. The seven slots' message vtables and the debug component's vtable
    // are installed by the embedded objects' own constructors; the body proper only zeroes the
    // two result timers (seconds word 0, fraction 0.0f).
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::NetworkRoadRulesManager()
        : mTimeUntilNextResultUpload( 0.0f )
        , mTimeUntilNextResultDownload( 0.0f )
    {
    }

    // -----------------------------------------------------------------------------
    // The first free slot (mPlayerID == invalid); asserts and returns nullptr when all seven are
    // taken.
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::RoadRulesData* NetworkRoadRulesManager::GetNextFreeRoadRulesDataEntry()
    {
        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID )
            {
                return &maRoadRulesData[liIndex];
            }
        }

        CGS_ASSERT( false, "Unable to find a free road rules data entry\n" );
        return nullptr;
    }

    // -----------------------------------------------------------------------------
    // The slot keyed by lPlayerID, or nullptr.
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::RoadRulesData* NetworkRoadRulesManager::GetRoadRulesDataEntry(
        NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID == lPlayerID )
            {
                return &maRoadRulesData[liIndex];
            }
        }

        return nullptr;
    }

    // -----------------------------------------------------------------------------
    // Once the road-rules cycle has left BOOTING, just report the auto-login step complete.
    // Otherwise, in the modes that use road rules (offline modes, the online free-burn lobby,
    // online showtime): read the server score key and reset date from the client config, post
    // the reset date to the game (connected-online event), start the local/global downloads and
    // the upload, and enter the idle-online state.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnAutoLogin()
    {
        if ( meState != E_ROAD_RULES_STATE_BOOTING )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );

            mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( 0 );
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leGameMode =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();

        const bool lbUsesRoadRules =
            ( leGameMode <  BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_START )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME );

        if ( !lbUsesRoadRules )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetServerInfoComponent() != nullptr,
                    "mpServerInterface->GetServerInfoComponent()" );

        mpServerInterface->GetServerInfoComponent()->GetStringFromClientConfig(
            "ROAD_RULES_SKEY", macRoadRulesServerKey, KI_SERVER_KEY_LENGTH );

        BrnNetworkModuleIO::NetworkOutRoadRulesConnectedOnlineEvent lConnectedEvent;
        lConnectedEvent.muLastRoadRulesResetTime =
            mpServerInterface->GetServerInfoComponent()->GetTimeStampFromClientConfig( "ROAD_RULES_RESET_DATE" );

        CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
        CGS_ASSERT( mpNetworkModule->GetNetworkEventQueue() != nullptr,
                    "mpNetworkModule->GetNetworkEventQueue()" );

        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &lConnectedEvent ),
            lConnectedEvent.GetEventType(), sizeof( lConnectedEvent ) );

        if ( !mbDownloadedLocalScores )
        {
            AttemptToDownloadLocalRoadRulesHighScores();
        }

        AttemptToDownloadRoadRulesHighScores();
        AttemptToUploadNewRoadRulesScores();

        meState = KI_STATE_AUTO_LOGIN_PRIMED;
    }

    // -----------------------------------------------------------------------------
    // Latch the four collaborators (asserting each), empty the buffered received-scores queue,
    // clear the local-scores flag and go back to BOOTING.
    // -----------------------------------------------------------------------------
    bool NetworkRoadRulesManager::Prepare( BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface,
                                            CgsNetwork::PlayerManager* lpPlayerManager, CgsNetwork::TimeManager* lpTimeManager )
    {
        mpTimeManager      = lpTimeManager;
        mpPlayerManager    = lpPlayerManager;
        mpServerInterface  = lpServerInterface;
        mpNetworkModule    = lpNetworkModule;

        CGS_ASSERT( mpTimeManager != nullptr, "mpTimeManager" );
        CGS_ASSERT( mpPlayerManager != nullptr, "mpPlayerManager" );
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );

        mBufferedRoadRulesRecvQueue.Clear();
        mbDownloadedLocalScores = false;
        meState = E_ROAD_RULES_STATE_BOOTING;

        return true;
    }

    // -----------------------------------------------------------------------------
    // Re-construct every lobby challenge-score slot.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnEnterGame()
    {
        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalLobbyScores[liIndex].Construct();
        }
    }

    // -----------------------------------------------------------------------------
    // Stop all three server cursors. If an auto-login cycle was running (idle-online or any
    // upload/download step), report it complete. Then enter IN_GAME.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnGameLaunching()
    {
        const s32 liPreviousState = meState;

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;

        if ( liPreviousState == KI_STATE_AUTO_LOGIN_PRIMED
             || liPreviousState == E_ROAD_RULES_STATE_UPLOADING
             || liPreviousState == E_ROAD_RULES_STATE_DOWNLOADING
             || liPreviousState == E_ROAD_RULES_STATE_DOWNLOADING_LOCAL )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );

            mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( 0 );
        }

        meState = E_ROAD_RULES_STATE_IN_GAME;
    }

    // -----------------------------------------------------------------------------
    // Once the local player is out of the server game, drop any queued personal bests; in every
    // case go back to BOOTING.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnGameFinish()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );

        if ( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() )
        {
            mPersonalBestToSendBuffer.Clear();
        }

        meState = E_ROAD_RULES_STATE_BOOTING;
    }

    // -----------------------------------------------------------------------------
    // Once the local player is out of the server game, go back to BOOTING and drop any queued
    // personal bests.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnRoundFinish()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );

        if ( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() )
        {
            meState = E_ROAD_RULES_STATE_BOOTING;
            mPersonalBestToSendBuffer.Clear();
        }
    }

    // -----------------------------------------------------------------------------
    // The local player must already be out of the server game. Unless an auto-login cycle is
    // still running (idle-online or an upload/download step), go back to BOOTING; always drop
    // the queued personal bests.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnLeaveGame()
    {
        CGS_ASSERT( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame(),
                    "!mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()" );

        if ( meState != KI_STATE_AUTO_LOGIN_PRIMED
             && meState != E_ROAD_RULES_STATE_UPLOADING
             && meState != E_ROAD_RULES_STATE_DOWNLOADING
             && meState != E_ROAD_RULES_STATE_DOWNLOADING_LOCAL )
        {
            meState = E_ROAD_RULES_STATE_BOOTING;
        }

        mPersonalBestToSendBuffer.Clear();
    }

    // -----------------------------------------------------------------------------
    // If logged in and not inside a started server game, start the local-scores download.
    // -----------------------------------------------------------------------------
    u32 NetworkRoadRulesManager::AttemptToDownloadLocalRoadRulesHighScores()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );

        const bool lbLoggedIn = mpServerInterface->GetConnectionComponent()->IsLoggedIn();
        if ( lbLoggedIn )
        {
            if ( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() )
            {
                StartDownloadingLocalRoadRulesScoresFromServer();
                return true;
            }

            if ( !mpServerInterface->GetGameComponent()->IsGameStarted() )
            {
                StartDownloadingLocalRoadRulesScoresFromServer();
                return true;
            }
        }

        return lbLoggedIn;
    }

    // -----------------------------------------------------------------------------
    // Same shape as AttemptToDownloadLocalRoadRulesHighScores, for the global high scores.
    // -----------------------------------------------------------------------------
    u32 NetworkRoadRulesManager::AttemptToDownloadRoadRulesHighScores()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );

        const bool lbLoggedIn = mpServerInterface->GetConnectionComponent()->IsLoggedIn();
        if ( lbLoggedIn )
        {
            if ( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() )
            {
                StartDownloadingRoadRulesScoresFromServer();
                return true;
            }

            if ( !mpServerInterface->GetGameComponent()->IsGameStarted() )
            {
                StartDownloadingRoadRulesScoresFromServer();
                return true;
            }
        }

        return lbLoggedIn;
    }

    // -----------------------------------------------------------------------------
    // If not inside a started server game and logged in, start the upload cycle.
    // -----------------------------------------------------------------------------
    s32 NetworkRoadRulesManager::AttemptToUploadNewRoadRulesScores()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );

        const bool lbInStartedGame =
            mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
            && mpServerInterface->GetGameComponent()->IsGameStarted();

        if ( !lbInStartedGame )
        {
            CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                        "mpServerInterface->GetConnectionComponent()" );

            if ( mpServerInterface->GetConnectionComponent()->IsLoggedIn() )
            {
                StartUploadingRoadRulesScoresToServer();
                return true;
            }
        }

        return false;
    }

    // -----------------------------------------------------------------------------
    // Reset the local-download cursor and the inter-download timer.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartDownloadingLocalRoadRulesScoresFromServer()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        mIndexOfNextLocalChallengeToDownload = 0;
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // Reset the global-download cursor and the inter-download timer.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartDownloadingRoadRulesScoresFromServer()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        mIndexOfNextChallengeToDownload = 0;
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // Reset the upload cursor, the roads-considered counter and the inter-upload timer.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartUploadingRoadRulesScoresToServer()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        mIndexOfNextChallengeToUpload     = 0;
        miNumRoadsConsideredForUpload     = 0;
        mTimeUntilNextResultUpload.SetFloatVal( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // DOWNLOADING step: once the custom-commands component is idle and the inter-download timer
    // has run out, request the next batch of global high scores.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleDownloadingRoadRulesScores()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        if ( ( mTimeUntilNextResultDownload.GetFloatVal() ) > 0.0f )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        mpServerInterface->GetCustomCommandsComponent()->GetRoadRulesHighScores(
            muTimeStampOfLastDownload, KI_NUM_SCORES_TO_DOWNLOAD_IN_A_BATCH,
            mIndexOfNextChallengeToDownload, &_DownloadRoadRulesCallback, this );
    }

    // -----------------------------------------------------------------------------
    // DOWNLOADING_LOCAL step: same gating, then request the next batch of the local player's own
    // scores (batch clamped to the slots left).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleDownloadingLocalRoadRulesScores()
    {
        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        if ( ( mTimeUntilNextResultDownload.GetFloatVal() ) > 0.0f )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != nullptr,
                    "mpServerInterface->GetConnectionComponent()" );
        CGS_ASSERT( mpServerInterface->GetGameComponent() != nullptr,
                    "mpServerInterface->GetGameComponent()" );
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
                    "mpServerInterface->GetConnectionComponent()->IsLoggedIn()" );
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
                    "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || "
                    "( !mpServerInterface->GetGameComponent()->IsGameStarted() )" );

        s32 liNumToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS - mIndexOfNextLocalChallengeToDownload;
        if ( liNumToDownload > KI_NUM_SCORES_TO_DOWNLOAD_IN_A_BATCH )
        {
            liNumToDownload = KI_NUM_SCORES_TO_DOWNLOAD_IN_A_BATCH;
        }

        mpServerInterface->GetCustomCommandsComponent()->GetLocalRoadRulesHighScores(
            liNumToDownload, mIndexOfNextLocalChallengeToDownload, macRoadRulesServerKey,
            &_DownloadLocalRoadRulesCallback, this );
    }

    // -----------------------------------------------------------------------------
    // Walk this frame's game actions: PrepareForMode stops buffering received scores, StopMode
    // starts buffering them.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessGameActions(
        const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue )
    {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liActionID = lpGameActionQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != nullptr )
        {
            if ( liActionID == KI_GAME_ACTION_PREPARE_FOR_MODE )
            {
                CGS_ASSERT( lpEvent != nullptr, "lpPrepareForModeAction" );
                mbBufferRoadRulesReceived = false;
            }
            else if ( liActionID == KI_GAME_ACTION_STOP_MODE )
            {
                CGS_ASSERT( lpEvent != nullptr, "lpStopModeAction" );
                mbBufferRoadRulesReceived = true;
            }

            liActionID = lpGameActionQueue->GetNextEvent( lpEvent, &lpEvent, &liSize );
        }
    }

    // -----------------------------------------------------------------------------
    // Per-frame tick.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessAfterSimulation(
        const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput, bool lbAreWePlaying )
    {
        ProcessNetworkEvents( lpInput->GetNetworkEventQueue() );
        ProcessGameActions( lpInput->GetGameActionQueue() );
        SendPersonalBestScore();
        HandleSendingRoadRulesScores( lbAreWePlaying );

        switch ( meState )
        {
            case E_ROAD_RULES_STATE_UPLOADING:
                HandleUploadingRoadRulesScores();
                break;
            case E_ROAD_RULES_STATE_DOWNLOADING:
                HandleDownloadingRoadRulesScores();
                break;
            case E_ROAD_RULES_STATE_DOWNLOADING_LOCAL:
                HandleDownloadingLocalRoadRulesScores();
                break;
            default:
                break;
        }
    }
}
