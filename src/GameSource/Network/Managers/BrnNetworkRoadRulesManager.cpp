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
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkRoadRulesManager.cpp"
#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
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

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkRoadRulesManager.cpp".

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
        return 0;
    }

    // -----------------------------------------------------------------------------
    // The slot keyed by lPlayerID, or nullptr.
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::RoadRulesData* NetworkRoadRulesManager::GetRoadRulesDataEntry(
        NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( lPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT("lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", __FILE__, 1277 /* ARTIST 0x82549EFC */) );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID == lPlayerID )
            {
                return &maRoadRulesData[liIndex];
            }
        }

        return 0;
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
            CGS_ASSERT( mpNetworkModule != 0, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != 0,
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

        CGS_ASSERT( mpServerInterface != 0, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetServerInfoComponent() != 0,
                    "mpServerInterface->GetServerInfoComponent()" );

        mpServerInterface->GetServerInfoComponent()->GetStringFromClientConfig(
            "ROAD_RULES_SKEY", macRoadRulesServerKey, KI_SERVER_KEY_LENGTH );

        bool lbHasResetDate = true;

        CGS_ASSERT( mpNetworkModule != 0, "mpNetworkModule" );
        CGS_ASSERT( mpNetworkModule->GetNetworkEventQueue() != 0,
                    "mpNetworkModule->GetNetworkEventQueue()" );

        if (lbHasResetDate)
        {
            BrnNetworkModuleIO::NetworkOutRoadRulesConnectedOnlineEvent lConnectedEvent;
            lConnectedEvent.muLastRoadRulesResetTime =
                mpServerInterface->GetServerInfoComponent()->GetTimeStampFromClientConfig( "ROAD_RULES_RESET_DATE" );
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lConnectedEvent ),
                lConnectedEvent.GetEventType(), sizeof( lConnectedEvent ) );
        }

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

        CGS_ASSERT( mpTimeManager != 0,
            CGS_ASSERT_MESSAGE_AT("mpTimeManager", __FILE__, 159 /* ARTIST 0x8255C848 */));
        CGS_ASSERT( mpPlayerManager != 0,
            CGS_ASSERT_MESSAGE_AT("mpPlayerManager", __FILE__, 160 /* ARTIST 0x8255C870 */));
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 161 /* ARTIST 0x8255C898 */));
        CGS_ASSERT( mpNetworkModule != 0,
            CGS_ASSERT_MESSAGE_AT("mpNetworkModule", __FILE__, 162 /* ARTIST 0x8255C8C0 */));

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
            CGS_ASSERT(mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
                "mpNetworkModule",
                __FILE__, 1568 /* ARTIST 0x8254A574 */));
            CGS_ASSERT(mpNetworkModule->GetNetworkManager() != 0, CGS_ASSERT_MESSAGE_AT(
                "mpNetworkModule->GetNetworkManager()",
                __FILE__, 1569 /* ARTIST 0x8254A5A0 */));

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
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1590 /* ARTIST 0x82554D64 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1591 /* ARTIST 0x82554D90 */));

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
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1614 /* ARTIST 0x82554E1C */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1615 /* ARTIST 0x82554E48 */));

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
            CGS_ASSERT_MESSAGE_AT(
                "!mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()",
                __FILE__, 1635 /* ARTIST 0x82554ED4 */));

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
        CGS_ASSERT(mpServerInterface != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface",
            __FILE__, 1686 /* ARTIST 0x8254A6F8 */));
        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetConnectionComponent()",
            __FILE__, 1687 /* ARTIST 0x8254A724 */));

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
        CGS_ASSERT(mpServerInterface != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface",
            __FILE__, 1663 /* ARTIST 0x8254A618 */));
        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetConnectionComponent()",
            __FILE__, 1664 /* ARTIST 0x8254A644 */));

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
        CGS_ASSERT(mpServerInterface != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface",
            CGS_ARTIST_FILE, 763 /* ARTIST 0x82554B18 */));
        CGS_ASSERT(mpServerInterface->GetGameComponent() != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetGameComponent()",
            CGS_ARTIST_FILE, 764 /* ARTIST 0x82554B44 */));

        const bool lbInStartedGame =
            mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
            && mpServerInterface->GetGameComponent()->IsGameStarted();

        if ( !lbInStartedGame )
        {
            CGS_ASSERT(mpServerInterface->GetConnectionComponent() != 0, CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                CGS_ARTIST_FILE, 770 /* ARTIST 0x82554BA0 */));

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
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1403 /* ARTIST 0x8254A394 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                __FILE__, 1404 /* ARTIST 0x8254A3C0 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1405 /* ARTIST 0x8254A3EC */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
                __FILE__, 1408 /* ARTIST 0x8254A420 */));
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
            CGS_ASSERT_MESSAGE_AT(
                "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
                __FILE__, 1410 /* ARTIST 0x8254A46C */));

        mIndexOfNextLocalChallengeToDownload = 0;
        mTimeUntilNextResultDownload = 0.0f;
    }

    // -----------------------------------------------------------------------------
    // Reset the global-download cursor and the inter-download timer.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartDownloadingRoadRulesScoresFromServer()
    {
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1378 /* ARTIST 0x8254A244 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                __FILE__, 1379 /* ARTIST 0x8254A270 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1380 /* ARTIST 0x8254A29C */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
                __FILE__, 1383 /* ARTIST 0x8254A2D0 */));
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
            CGS_ASSERT_MESSAGE_AT(
                "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
                __FILE__, 1385 /* ARTIST 0x8254A31C */));

        mIndexOfNextChallengeToDownload = 0;
        mTimeUntilNextResultDownload = 0.0f;
    }

    // -----------------------------------------------------------------------------
    // Reset the upload cursor, the roads-considered counter and the inter-upload timer.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartUploadingRoadRulesScoresToServer()
    {
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1352 /* ARTIST 0x8254A0EC */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                __FILE__, 1353 /* ARTIST 0x8254A118 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1354 /* ARTIST 0x8254A144 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
                __FILE__, 1357 /* ARTIST 0x8254A178 */));
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
            CGS_ASSERT_MESSAGE_AT(
                "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
                __FILE__, 1359 /* ARTIST 0x8254A1C4 */));

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
        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 1107 /* ARTIST 0x82561204 */));

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        if ( ( mTimeUntilNextResultDownload.GetFloatVal() ) > 0.0f )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                __FILE__, 1117 /* ARTIST 0x82561274 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 1118 /* ARTIST 0x825612A0 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
                __FILE__, 1119 /* ARTIST 0x825612D4 */));
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
            CGS_ASSERT_MESSAGE_AT(
                "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
                __FILE__, 1121 /* ARTIST 0x82561320 */));

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
        CGS_ASSERT(mpServerInterface != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface",
            CGS_ARTIST_FILE, 1144 /* ARTIST 0x825613B0 */));

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        if ( ( mTimeUntilNextResultDownload.GetFloatVal() ) > 0.0f )
        {
            return;
        }

        CGS_ASSERT(mpServerInterface->GetConnectionComponent() != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetConnectionComponent()",
            CGS_ARTIST_FILE, 1154 /* ARTIST 0x82561420 */));
        CGS_ASSERT(mpServerInterface->GetGameComponent() != 0, CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetGameComponent()",
            CGS_ARTIST_FILE, 1155 /* ARTIST 0x8256144C */));
        CGS_ASSERT(mpServerInterface->GetConnectionComponent()->IsLoggedIn(), CGS_ASSERT_MESSAGE_AT(
            "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
            CGS_ARTIST_FILE, 1156 /* ARTIST 0x82561480 */));
        CGS_ASSERT(!( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() && mpServerInterface->GetGameComponent()->IsGameStarted() ), CGS_ASSERT_MESSAGE_AT(
            "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || " "( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
            CGS_ARTIST_FILE, 1158 /* ARTIST 0x825614CC */));

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
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liActionID = lpGameActionQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != 0 )
        {
            if ( liActionID == KI_GAME_ACTION_PREPARE_FOR_MODE )
            {
                CGS_ASSERT( lpEvent != 0, "lpPrepareForModeAction" );
                mbBufferRoadRulesReceived = false;
            }
            else if ( liActionID == KI_GAME_ACTION_STOP_MODE )
            {
                CGS_ASSERT( lpEvent != 0, "lpStopModeAction" );
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

// FOLDED FROM BrnNetworkRoadRulesManager_wN2_01.cpp
// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- recovered function bodies (part 2)
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager_wN2_01.cpp
//
// Construct / Release / Destruct, the per-frame ProcessBeforeSimulation, per-player
// registration (AddPlayer / RemovePlayer / Disconnected), the personal-best and lobby-score send
// paths, the upload step, and the reliable-message / upload completion callbacks.
// Member layout and console offsets: see the class banner in the header.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include <xtl.h>
#include <cstring>                                                                  // memset / memcpy

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                          // CgsDev::Log::gpDebugPrint (fake-nack warning)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue<14000,16>::AddEvent
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                // PlayerManager::GetPlayerByID / GetMenuDataByID / IsPlayerTurnToSendRoundRobinMessage
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                // NetworkPlayer::RegisterMessageType, PlayerMenuData, KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                     // TimeManager::GetFrameCount
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection::IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames::IsLocalPlayerInGame / IsGameStarted
#include "GameSource/Network/BrnNetworkModule.h"                                    // BrnNetworkModule accessors
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager::OnAutoLoginProcessComplete / GetBuddyManager
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                          // NetworkOutRecvRoadRulesUploadedEvent
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface component accessors
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"         // ServerInterfaceCustomCommands::SetRoadRulesForLocalPlayer
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"     // BrnServerInterfaceDownloadableConfig::TimeBetweenRoadRulesUploads
#include "GameSource/Network/Parameters/BrnNetworkRoadRulesData.h"                  // RoadRulesUploadData
#include "GameSource/GameState/BrnCgsPlayerName.h"                                  // CgsNetwork::PlayerName::Construct
#include "GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.h"            // BuddyManagerX360::IsFullBuddy (download filter)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"             // StreetManager::CopyUserChallengeData

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkRoadRulesManager.cpp".

namespace BrnNetwork
{
    namespace
    {
        // Reliable message type ids registered per player.
        const s32 KI_ROAD_RULES_MESSAGE_TYPE               = 26;
        const s32 KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE = 27;

        // Capacity of one road-rules message (its score-record array).
        const s32 KI_MAX_ROAD_RULES_MESSAGE_ENTRIES = 10;

        // The 16-bit wire frame window the messages are stamped with.
        const u32 KU_FRAME_WRAP = 0xFFFF;

        // Challenges per download batch, and the pause before the next batch.
        const s32 KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH = 10;
        const f32 KF_TIME_BETWEEN_DOWNLOAD_BATCHES     = 1.0f;
    }

    // -----------------------------------------------------------------------------
    // Reset everything: BOOTING, every slot freed with its four messages re-constructed, the
    // local score tables re-constructed, the cursors stopped, the timers zeroed, the three
    // queues constructed and emptied, buffering of received scores on, the server key / id /
    // force-overwrite flag / local-scores flag cleared, and the debug component bound.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Construct()
    {
        meState = E_ROAD_RULES_STATE_BOOTING;

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData& lData = maRoadRulesData[liIndex];
            lData.mPlayerID                  = CgsNetwork::KI_INVALID_PLAYER_ID;
            lData.mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            lData.mRoadRulesMessageSend.Construct();
            lData.mRoadRulesMessageRecv.Construct();
            lData.mRoadRulesPBMessageSend.Construct();
            lData.mRoadRulesPBMessageRecv.Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalRoadScoresToUpload[liIndex].Construct();
            maLocalLobbyScores[liIndex].Construct();
        }

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        muTimeStampOfLastDownload            = 0;
        miNumRoadsConsideredForUpload        = 0;
        mTimeUntilNextResultUpload.SetFloatVal( 0.0f );
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );

        mBufferedRoadRulesRecvQueue.Construct();
        mBufferedRoadRulesRecvQueue.Clear();
        mRoadRulesPersonalBestBuffer.Construct();
        mRoadRulesPersonalBestBuffer.Clear();
        mbBufferRoadRulesReceived = true;
        memset( macRoadRulesServerKey, 0, sizeof( macRoadRulesServerKey ) );
        mu64RoadRulesID = 0;
        mbForceOverwriteServerRecords = false;
        mPersonalBestToSendBuffer.Construct();
        mPersonalBestToSendBuffer.Clear();
        mbDownloadedLocalScores = false;

        mRoadRulesDebugComponent.Construct( this );
    }

    // -----------------------------------------------------------------------------
    // Empty the buffered received-scores queue, drop the four collaborators and clear the
    // local-scores flag.
    // -----------------------------------------------------------------------------
    bool NetworkRoadRulesManager::Release()
    {
        mBufferedRoadRulesRecvQueue.Clear();
        mpTimeManager     = 0;
        mpPlayerManager   = 0;
        mpServerInterface = 0;
        mpNetworkModule   = 0;
        mbDownloadedLocalScores = false;
        return true;
    }

    // -----------------------------------------------------------------------------
    // The Construct reset in teardown order, then unbind the debug component.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Destruct()
    {
        mbDownloadedLocalScores = false;
        mPersonalBestToSendBuffer.Clear();
        mu64RoadRulesID = 0;
        mbForceOverwriteServerRecords = false;
        memset( macRoadRulesServerKey, 0, sizeof( macRoadRulesServerKey ) );
        mbBufferRoadRulesReceived = true;
        mRoadRulesPersonalBestBuffer.Clear();
        mBufferedRoadRulesRecvQueue.Clear();
        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        muTimeStampOfLastDownload            = 0;
        miNumRoadsConsideredForUpload        = 0;
        mTimeUntilNextResultUpload.SetFloatVal( 0.0f );
        mTimeUntilNextResultDownload.SetFloatVal( 0.0f );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalRoadScoresToUpload[liIndex].Construct();
            maLocalLobbyScores[liIndex].Construct();
        }

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData& lData = maRoadRulesData[liIndex];
            lData.mPlayerID                  = CgsNetwork::KI_INVALID_PLAYER_ID;
            lData.mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            lData.mRoadRulesMessageSend.Construct();
            lData.mRoadRulesMessageRecv.Construct();
            lData.mRoadRulesPBMessageSend.Construct();
            lData.mRoadRulesPBMessageRecv.Construct();
        }

        meState = E_ROAD_RULES_STATE_BOOTING;
        mRoadRulesDebugComponent.Destruct();
    }

    // -----------------------------------------------------------------------------
    // While received scores are not being buffered, hand the buffered received-scores queue to
    // the game (only when the game's queue is empty) and forward one buffered personal best.
    // Then run the state machine: tick the upload / download timers in their states; in the
    // idle-online state move on to whichever server cursor still has work (local download,
    // then download, then upload), or finish the auto-login cycle and go back to BOOTING.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessBeforeSimulation( BrnNetworkModuleIO::OutputBuffer* lpOutput,
                                                           f32 lfGameTimeStep, BrnUpdateSet lUpdateSet )
    {
        (void)lUpdateSet;

        CGS_ASSERT(lpOutput != 0, CGS_ASSERT_MESSAGE_AT(
            "lpOutput",
            CGS_ARTIST_FILE, 286 /* ARTIST 0x82563DF4 */));

        if ( !mbBufferRoadRulesReceived )
        {
            CGS_ASSERT(mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
                "mpNetworkModule",
                CGS_ARTIST_FILE, 291 /* ARTIST 0x82563E30 */));
            CGS_ASSERT(mpNetworkModule->GetNetworkToGameStateInterface() != 0, CGS_ASSERT_MESSAGE_AT(
                "mpNetworkModule->GetNetworkToGameStateInterface()",
                CGS_ARTIST_FILE, 292 /* ARTIST 0x82563E5C */));

            BrnNetworkModuleIO::NetworkToGameStateInterface::RoadRulesReceivedQueue* lpOutputRoadRulesRecvQueue =
                mpNetworkModule->GetNetworkToGameStateInterface()->GetRoadRulesReceivedQueue();
            CGS_ASSERT(lpOutputRoadRulesRecvQueue != 0, CGS_ASSERT_MESSAGE_AT(
                "lpOutputRoadRulesRecvQueue",
                CGS_ARTIST_FILE, 294 /* ARTIST 0x82563E8C */));

            if ( lpOutputRoadRulesRecvQueue->GetLength() == 0 )
            {
                lpOutputRoadRulesRecvQueue->Append( mBufferedRoadRulesRecvQueue );
                mBufferedRoadRulesRecvQueue.Clear();
            }

            CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != 0, CGS_ASSERT_MESSAGE_AT(
                "mpNetworkModule->GetNetworkEventQueue()",
                CGS_ARTIST_FILE, 305 /* ARTIST 0x82563ED4 */));

            if ( mRoadRulesPersonalBestBuffer.GetLength() > 0 )
            {
                BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent lRoadRulesPbEvent;
                mRoadRulesPersonalBestBuffer.Pop( &lRoadRulesPbEvent );
                BrnNetworkModuleIO::NetworkEventQueue* lpEventQueue = mpNetworkModule->GetNetworkEventQueue();
                lpEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lRoadRulesPbEvent ),
                    lRoadRulesPbEvent.GetEventType(), sizeof( lRoadRulesPbEvent ) );
            }
        }

        switch ( meState )
        {
            case E_ROAD_RULES_STATE_UPLOADING:
            {
                const CgsSystem::Time lTimeStep( lfGameTimeStep );
                mTimeUntilNextResultUpload -= lTimeStep;
                break;
            }

            case E_ROAD_RULES_STATE_DOWNLOADING:
            case E_ROAD_RULES_STATE_DOWNLOADING_LOCAL:
            {
                const CgsSystem::Time lTimeStep( lfGameTimeStep );
                mTimeUntilNextResultDownload -= lTimeStep;
                break;
            }

            case KI_STATE_AUTO_LOGIN_PRIMED:
                if ( mIndexOfNextLocalChallengeToDownload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT(meState != E_ROAD_RULES_STATE_IN_GAME, CGS_ASSERT_MESSAGE_AT(
                        "meState != E_ROAD_RULES_STATE_IN_GAME",
                        CGS_ARTIST_FILE, 346 /* ARTIST 0x82563FB8 */));
                    meState = E_ROAD_RULES_STATE_DOWNLOADING_LOCAL;
                }
                else if ( mIndexOfNextChallengeToDownload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT(meState != E_ROAD_RULES_STATE_IN_GAME, CGS_ASSERT_MESSAGE_AT(
                        "meState != E_ROAD_RULES_STATE_IN_GAME",
                        CGS_ARTIST_FILE, 353 /* ARTIST 0x82563FFC */));
                    meState = E_ROAD_RULES_STATE_DOWNLOADING;
                }
                else if ( mIndexOfNextChallengeToUpload < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
                {
                    CGS_ASSERT(meState != E_ROAD_RULES_STATE_IN_GAME, CGS_ASSERT_MESSAGE_AT(
                        "meState != E_ROAD_RULES_STATE_IN_GAME",
                        CGS_ARTIST_FILE, 361 /* ARTIST 0x82564040 */));
                    meState = E_ROAD_RULES_STATE_UPLOADING;
                }
                else
                {
                    CGS_ASSERT(mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
                        "mpNetworkModule",
                        CGS_ARTIST_FILE, 372 /* ARTIST 0x82564078 */));
                    CGS_ASSERT(mpNetworkModule->GetNetworkManager() != 0, CGS_ASSERT_MESSAGE_AT(
                        "mpNetworkModule->GetNetworkManager()",
                        CGS_ARTIST_FILE, 373 /* ARTIST 0x825640A4 */));

                    mpNetworkModule->GetNetworkManager()->OnAutoLoginProcessComplete( 0 );
                    meState = E_ROAD_RULES_STATE_BOOTING;
                }
                break;

            default:
                break;
        }
    }

    // -----------------------------------------------------------------------------
    // For a player the player manager knows: claim a free slot, register the road-rules and
    // personal-best reliable message pairs on the player (both registered with the road-rules
    // message length), key the slot and start sending our lobby scores to them.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::AddPlayer( NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( mpPlayerManager != 0,
            CGS_ASSERT_MESSAGE_AT("mpPlayerManager", __FILE__, 1185 /* ARTIST 0x8256BA68 */));

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID( lPlayerID );
        if ( lpNetworkPlayer == 0 )
        {
            return;
        }

        RoadRulesData* lpDataEntry = GetNextFreeRoadRulesDataEntry();
        CGS_ASSERT( lpDataEntry != 0,
            CGS_ASSERT_MESSAGE_AT("lpDataEntry", __FILE__, 1195 /* ARTIST 0x8256BAB0 */));

        lpNetworkPlayer->RegisterMessageType( KI_ROAD_RULES_MESSAGE_TYPE, sizeof( RoadRulesMessage ),
                                              &lpDataEntry->mRoadRulesMessageSend,
                                              &lpDataEntry->mRoadRulesMessageRecv,
                                              &_RoadRulesMessageArrivedCallback,
                                              &_RoadRulesMessageDeliveredCallback,
                                              this );
        lpNetworkPlayer->RegisterMessageType( KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE, sizeof( RoadRulesMessage ),
                                              &lpDataEntry->mRoadRulesPBMessageSend,
                                              &lpDataEntry->mRoadRulesPBMessageRecv,
                                              &_RoadRulesPersonalBestArrivedCallback,
                                              &_RoadRulesPersonalBestDeliveredCallback,
                                              this );

        lpDataEntry->mPlayerID = lPlayerID;
        StartSendingRoadRulesScoresToPlayer( lpDataEntry );
    }

    // -----------------------------------------------------------------------------
    // Unregister both message types on the player and free its slot.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::RemovePlayer( NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( mpPlayerManager != 0,
            CGS_ASSERT_MESSAGE_AT("mpPlayerManager", __FILE__, 1242 /* ARTIST 0x82554C10 */));

        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpPlayerManager->GetPlayerByID( lPlayerID );
        if ( lpNetworkPlayer == 0 )
        {
            return;
        }

        lpNetworkPlayer->UnRegisterMessageType( KI_ROAD_RULES_MESSAGE_TYPE );
        lpNetworkPlayer->UnRegisterMessageType( KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE );

        RoadRulesData* lpDataEntry = GetRoadRulesDataEntry( lPlayerID );
        CGS_ASSERT( lpDataEntry != 0,
            CGS_ASSERT_MESSAGE_AT("lpDataEntry", __FILE__, 1254 /* ARTIST 0x82554C70 */));

        lpDataEntry->mPlayerID                   = CgsNetwork::KI_INVALID_PLAYER_ID;
        lpDataEntry->mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
    }

    // -----------------------------------------------------------------------------
    // Remove every keyed player (each slot must come back free), stop the three server cursors
    // and go back to BOOTING.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::Disconnected()
    {
        for ( s32 liPlayerIndex = 0; liPlayerIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liPlayerIndex )
        {
            if ( maRoadRulesData[liPlayerIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID )
            {
                RemovePlayer( maRoadRulesData[liPlayerIndex].mPlayerID );
                CGS_ASSERT( maRoadRulesData[liPlayerIndex].mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID,
                    CGS_ASSERT_MESSAGE_AT(
                        "maRoadRulesData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID",
                        __FILE__, 1455 /* ARTIST 0x82554CEC */));
            }
        }

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        meState                              = E_ROAD_RULES_STATE_BOOTING;
    }

    // -----------------------------------------------------------------------------
    // A lobby personal best goes to the lobby table (and is queued for sending); any other goes
    // to the local upload table.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleNewPersonalBest(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT(lpNetworkRoadRulesPBEvent != 0, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkRoadRulesPBEvent",
            CGS_ARTIST_FILE, 586 /* ARTIST 0x82561044 */));

        if ( !lpNetworkRoadRulesPBEvent->mbLobbyPersonalBest )
        {
            UpdateLocalRoadRulesScoresWithNewPB( lpNetworkRoadRulesPBEvent );
        }
        else
        {
            UpdateLocalLobbyScoresWithNewPB( lpNetworkRoadRulesPBEvent );
        }
    }

    // -----------------------------------------------------------------------------
    // Rewind a keyed player's lobby-score send cursor to the first challenge.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::StartSendingRoadRulesScoresToPlayer( RoadRulesData* lpPlayerDataEntry )
    {
        CGS_ASSERT( lpPlayerDataEntry != 0,
            CGS_ASSERT_MESSAGE_AT("lpPlayerDataEntry", __FILE__, 1334 /* ARTIST 0x8254A058 */));
        CGS_ASSERT( lpPlayerDataEntry->mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID,
            CGS_ASSERT_MESSAGE_AT(
                "lpPlayerDataEntry->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 1335 /* ARTIST 0x8254A080 */));

        lpPlayerDataEntry->mIndexOfNextChallengeToSend = 0;
    }

    // -----------------------------------------------------------------------------
    // With a personal best queued: collect every keyed player's personal-best send message
    // (giving up for this frame if any of them is still in flight), pop the personal best and
    // send it to all of them.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::SendPersonalBestScore()
    {
        CGS_ASSERT(mpPlayerManager != 0, CGS_ASSERT_MESSAGE_AT(
            "mpPlayerManager",
            CGS_ARTIST_FILE, 704 /* ARTIST 0x82554940 */));
        CGS_ASSERT(mpTimeManager != 0, CGS_ASSERT_MESSAGE_AT(
            "mpTimeManager",
            CGS_ARTIST_FILE, 705 /* ARTIST 0x82554968 */));

        // ARTIST 0x82554974 zeroes the message count before the buffer-length test.
        RoadRulesPersonalBestMessage* lapRoadRulesMessages[KI_NUMBER_OF_PLAYER_DATA_SLOTS];
        s32 liNumMessages = 0;

        if ( mPersonalBestToSendBuffer.GetLength() <= 0 )
        {
            return;
        }

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID != CgsNetwork::KI_INVALID_PLAYER_ID )
            {
                lapRoadRulesMessages[liNumMessages] = &maRoadRulesData[liIndex].mRoadRulesPBMessageSend;
                if ( lapRoadRulesMessages[liNumMessages]->IsMessageValid() )
                {
                    return;
                }
                ++liNumMessages;
            }
        }

        const u16 lu16Frame = static_cast<u16>( mpTimeManager->GetFrameCount() % KU_FRAME_WRAP );

        BrnNetworkModuleIO::NetworkInRoadRulesPBEvent lPersonalBest;
        mPersonalBestToSendBuffer.Pop( &lPersonalBest );

        for ( s32 liIndex = 0; liIndex < liNumMessages; ++liIndex )
        {
            RoadRulesPersonalBestMessage* lpRoadRulesMessage = lapRoadRulesMessages[liIndex];
            CGS_ASSERT(!lpRoadRulesMessage->IsMessageValid(), CGS_ASSERT_MESSAGE_AT(
                "!lpRoadRulesMessage->IsMessageValid()",
                CGS_ARTIST_FILE, 742 /* ARTIST 0x82554A9C */));
            lpRoadRulesMessage->PrepareForSend( lu16Frame, lPersonalBest.mChallengeIndex,
                                                lPersonalBest.mPersonalBestScore );
        }
    }

    // -----------------------------------------------------------------------------
    // Round-robin the keyed players that still have lobby scores to receive: for the first one
    // whose turn it is, gather the next message's worth of scores; with none left, stop that
    // player's cursor; otherwise send them (unless its message is still in flight) and stop.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleSendingRoadRulesScores( bool lbAreWePlaying )
    {
        CGS_ASSERT(mpPlayerManager != 0, CGS_ASSERT_MESSAGE_AT(
            "mpPlayerManager",
            __FILE__, 803 /* ARTIST 0x82564224 */));
        CGS_ASSERT(mpTimeManager != 0, CGS_ASSERT_MESSAGE_AT(
            "mpTimeManager",
            __FILE__, 804 /* ARTIST 0x8256424C */));

        const u16 lu16Frame = static_cast<u16>( mpTimeManager->GetFrameCount() % KU_FRAME_WRAP );

        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            RoadRulesData* lpData = &maRoadRulesData[liIndex];

            if ( lpData->mPlayerID == CgsNetwork::KI_INVALID_PLAYER_ID
                 || lpData->mIndexOfNextChallengeToSend >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                continue;
            }

            if ( !mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage( lpData->mPlayerID, lbAreWePlaying, 0 ) )
            {
                continue;
            }

            RoadRulesMessageData laRoadRulesMessageData[KI_MAX_ROAD_RULES_MESSAGE_ENTRIES];
            s32 liNumScores;
            const Road::ChallengeIndex liNumChallengesWalked =
                GetRoadRulesDataToSend( laRoadRulesMessageData, &liNumScores, lpData->mIndexOfNextChallengeToSend );

            if ( liNumScores <= 0 )
            {
                lpData->mIndexOfNextChallengeToSend = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                continue;
            }

            if ( lpData->mRoadRulesMessageSend.IsMessageValid() )
            {
                continue;
            }

            lpData->mRoadRulesMessageSend.PrepareForSend( lu16Frame, liNumScores, laRoadRulesMessageData );
            lpData->mIndexOfNextChallengeToSend += liNumChallengesWalked;
            return;
        }
    }

    // -----------------------------------------------------------------------------
    // From lStartIndex on, copy every lobby challenge that holds a score into the next record
    // (scores by type, challenge index) until the message is full or the table ends. Returns the
    // number of challenge slots walked.
    // -----------------------------------------------------------------------------
    Road::ChallengeIndex NetworkRoadRulesManager::GetRoadRulesDataToSend( RoadRulesMessageData* lpRoadRulesMessageData,
                                                                         s32* lpiNumScores,
                                                                         Road::ChallengeIndex lStartIndex )
    {
        s32 liNumScores = 0;
        Road::ChallengeIndex lChallengeIndex = lStartIndex;

        for ( ; lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++lChallengeIndex )
        {
            if ( liNumScores >= KI_MAX_ROAD_RULES_MESSAGE_ENTRIES )
            {
                break;
            }

            CGS_ASSERT(lChallengeIndex >= 0, CGS_ASSERT_MESSAGE_AT(
                "lChallengeIndex >= 0",
                __FILE__, 883 /* ARTIST 0x82561108 */));
            CGS_ASSERT(lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS, CGS_ASSERT_MESSAGE_AT(
                "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES",
                __FILE__, 884 /* ARTIST 0x82561128 */));

            const BrnStreetData::ChallengeData& lLobbyScore = maLocalLobbyScores[lChallengeIndex];
            if ( !lLobbyScore.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
            {
                continue;
            }

            RoadRulesMessageData& lRecord = lpRoadRulesMessageData[liNumScores];
            lRecord.mChallengeIndex = 0;
            memset( lRecord.maScores, 0, sizeof( lRecord.maScores ) );

            for ( s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType )
            {
                const BrnStreetData::ScoreType leScoreType = static_cast<BrnStreetData::ScoreType>( liScoreType );
                if ( lLobbyScore.ContainsData( leScoreType ) )
                {
                    lRecord.maScores[liScoreType] = lLobbyScore.GetScore( leScoreType );
                }
            }

            lRecord.mChallengeIndex = lChallengeIndex;
            ++liNumScores;
        }

        *lpiNumScores = liNumScores;
        return lChallengeIndex - lStartIndex;
    }

    // -----------------------------------------------------------------------------
    // Copy the personal best's scores into the lobby table, then queue the event for sending to
    // the other players.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::UpdateLocalLobbyScoresWithNewPB(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT( lpNetworkRoadRulesPBEvent != 0, "lpNetworkRoadRulesPBEvent" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        BrnStreetData::ChallengeData& lLobbyScore = maLocalLobbyScores[lpNetworkRoadRulesPBEvent->mChallengeIndex];
        const BrnStreetData::ChallengePlayerScoreEntry& lPersonalBest = lpNetworkRoadRulesPBEvent->mPersonalBestScore;

        for ( s32 liScoreType = BrnStreetData::E_SCORE_TYPE_START; liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; )
        {
            const BrnStreetData::ScoreType leScoreType = static_cast<BrnStreetData::ScoreType>( liScoreType );
            if ( lPersonalBest.ContainsData( leScoreType ) )
            {
                lLobbyScore.SetScore( leScoreType, lPersonalBest.GetScore( leScoreType ) );
            }

            ++liScoreType;
            CGS_ASSERT( liScoreType <= BrnStreetData::E_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT" );
        }

        mPersonalBestToSendBuffer.Push( lpNetworkRoadRulesPBEvent );
    }

    // -----------------------------------------------------------------------------
    // UPLOADING step: when no batch is outstanding, the inter-upload timer has run out, there is
    // upload work left and the custom-commands component is idle, gather the dirty local scores
    // and send them; with nothing to send, finish the upload cycle (idle-online).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::HandleUploadingRoadRulesScores()
    {
        const bool lbReadyToUpload =
            ( miNumRoadsConsideredForUpload == 0 ) && !( mTimeUntilNextResultUpload.GetFloatVal() > 0.0f );

        if ( mIndexOfNextChallengeToUpload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS || !lbReadyToUpload )
        {
            return;
        }

        if ( mpServerInterface->GetStatus( CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS )
             != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface != 0,
            CGS_ASSERT_MESSAGE_AT("mpServerInterface", __FILE__, 946 /* ARTIST 0x8256B898 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()",
                __FILE__, 947 /* ARTIST 0x8256B8C4 */));
        CGS_ASSERT( mpServerInterface->GetGameComponent() != 0,
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetGameComponent()",
                __FILE__, 948 /* ARTIST 0x8256B8F0 */));
        CGS_ASSERT( mpServerInterface->GetConnectionComponent()->IsLoggedIn(),
            CGS_ASSERT_MESSAGE_AT(
                "mpServerInterface->GetConnectionComponent()->IsLoggedIn()",
                __FILE__, 949 /* ARTIST 0x8256B924 */));
        CGS_ASSERT( !( mpServerInterface->GetGameComponent()->IsLocalPlayerInGame()
                       && mpServerInterface->GetGameComponent()->IsGameStarted() ),
            CGS_ASSERT_MESSAGE_AT(
                "( !mpServerInterface->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpServerInterface->GetGameComponent()->IsGameStarted() )",
                __FILE__, 951 /* ARTIST 0x8256B970 */));

        RoadRulesUploadData lUploadData;
        GetRoadRulesDataToUpload( &lUploadData );

        if ( lUploadData.GetNumScores() > 0 )
        {
            CGS_ASSERT( mpServerInterface->GetCustomCommandsComponent() != 0,
                CGS_ASSERT_MESSAGE_AT(
                    "mpServerInterface->GetCustomCommandsComponent()",
                    __FILE__, 958 /* ARTIST 0x8256B9B4 */));
            CGS_ASSERT( NULL != macRoadRulesServerKey,
                CGS_ASSERT_MESSAGE_AT(
                    "NULL != macRoadRulesServerKey",
                    __FILE__, 959 /* ARTIST 0x8256B9D8 */));

            mpServerInterface->GetCustomCommandsComponent()->SetRoadRulesForLocalPlayer(
                &lUploadData, macRoadRulesServerKey, &_UploadRoadRulesCallback, this );
        }
        else
        {
            miNumRoadsConsideredForUpload = 0;
            mbForceOverwriteServerRecords = false;
            meState                       = KI_STATE_AUTO_LOGIN_PRIMED;
            mIndexOfNextChallengeToUpload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        }
    }

    // The street-data snapshot event's copy of the local scores (inline; its only user is this
    // manager, and StreetManager is complete here).
    inline void BrnNetworkModuleIO::NetworkInRoadRulesDataEvent::GetRoadRulesScoreData(
        BrnStreetData::ChallengePlayerScoreEntry* lpDest, s32 liSizeInBytes ) const
    {
        mpStreetManager->CopyUserChallengeData( lpDest, liSizeInBytes );
    }

    // -----------------------------------------------------------------------------
    // The game's network events: a street-data snapshot refreshes the local upload table and the
    // download stamp / road-rules id, a personal best goes to the lobby or the upload table, and
    // the overwrite request forces the next upload to send every recorded score.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessNetworkEvents( const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue )
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != 0 )
        {
            switch ( liEventType )
            {
            case BrnNetworkModuleIO::NetworkInRoadRulesDataEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRoadRulesDataEvent* lpRoadRulesScoreDataEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRoadRulesDataEvent*>( lpEvent );
                CGS_ASSERT( lpRoadRulesScoreDataEvent != 0, "lpRoadRulesScoreDataEvent" );

                lpRoadRulesScoreDataEvent->GetRoadRulesScoreData( maLocalRoadScoresToUpload,
                                                                  sizeof( maLocalRoadScoresToUpload ) );
                muTimeStampOfLastDownload = lpRoadRulesScoreDataEvent->GetTimeStampOfLastRoadRulesDownload();
                mu64RoadRulesID           = lpRoadRulesScoreDataEvent->GetRoadRulesID();
                break;
            }

            case BrnNetworkModuleIO::NetworkInRoadRulesPBEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent*>( lpEvent );
                CGS_ASSERT( lpNetworkRoadRulesPBEvent != 0, "lpNetworkRoadRulesPBEvent" );

                if ( lpNetworkRoadRulesPBEvent->mbLobbyPersonalBest )
                {
                    UpdateLocalLobbyScoresWithNewPB( lpNetworkRoadRulesPBEvent );
                }
                else
                {
                    UpdateLocalRoadRulesScoresWithNewPB( lpNetworkRoadRulesPBEvent );
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInRoadRulesOverwriteServerRecord::KI_EVENT_TYPE:
                mbForceOverwriteServerRecords = true;
                break;

            default:
                break;
            }

            const CgsModule::Event* lpNextEvent = 0;
            liEventType = lpNetworkEventQueue->GetNextEvent( lpEvent, &lpNextEvent, &liSize );
            lpEvent = lpNextEvent;
        }
    }

    // -----------------------------------------------------------------------------
    // Copy a new road personal best into the local upload table: every recorded score type takes
    // the new score and the car that set it.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::UpdateLocalRoadRulesScoresWithNewPB(
        const BrnNetworkModuleIO::NetworkInRoadRulesPBEvent* lpNetworkRoadRulesPBEvent )
    {
        CGS_ASSERT( lpNetworkRoadRulesPBEvent != 0, "lpNetworkRoadRulesPBEvent" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex >= 0" );
        CGS_ASSERT( lpNetworkRoadRulesPBEvent->mChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                    "lpNetworkRoadRulesPBEvent->mChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        BrnStreetData::ChallengePlayerScoreEntry& lRoadScore =
            maLocalRoadScoresToUpload[lpNetworkRoadRulesPBEvent->mChallengeIndex];
        const BrnStreetData::ChallengePlayerScoreEntry& lPersonalBest = lpNetworkRoadRulesPBEvent->mPersonalBestScore;

        for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
        {
            if ( lPersonalBest.ContainsData( leScoreType ) )
            {
                lRoadScore.SetScore( leScoreType, lPersonalBest.GetScore( leScoreType ) );
                lRoadScore.SetCarID( leScoreType, lPersonalBest.GetCarID( leScoreType ) );
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Fill one upload batch from the local upload table, starting at the next challenge to
    // upload: the dirty scores, or every score (zero when unrecorded) while the server records are
    // being overwritten. A challenge whose scores do not all fit is left to start the next batch.
    // Records how many challenges the batch covers.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::GetRoadRulesDataToUpload( RoadRulesUploadData* lpUploadData )
    {
        bool lbSpaceLeft = true;

        lpUploadData->Construct( mu64RoadRulesID );

        Road::ChallengeIndex lChallengeIndex = mIndexOfNextChallengeToUpload;
        for ( ; lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS && lbSpaceLeft; ++lChallengeIndex )
        {
            CGS_ASSERT( lChallengeIndex >= 0, "lChallengeIndex >= 0" );
            CGS_ASSERT( lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS,
                        "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

            const BrnStreetData::ChallengePlayerScoreEntry* lpRoadScoreDataEntry =
                &maLocalRoadScoresToUpload[lChallengeIndex];
            CGS_ASSERT( lpRoadScoreDataEntry != 0, "lpRoadScoreDataEntry" );

            if ( !mbForceOverwriteServerRecords && !lpRoadScoreDataEntry->IsDirty( BrnStreetData::E_SCORE_TYPE_COUNT ) )
            {
                continue;
            }

            for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
            {
                s32   liScore;
                CgsID lCarID;
                if ( lpRoadScoreDataEntry->IsDirty( leScoreType ) )
                {
                    lCarID  = lpRoadScoreDataEntry->GetCarID( leScoreType );
                    liScore = lpRoadScoreDataEntry->GetScore( leScoreType );
                }
                else if ( !mbForceOverwriteServerRecords )
                {
                    continue;
                }
                else
                {
                    liScore = 0;
                    lCarID  = 0;
                    if ( lpRoadScoreDataEntry->ContainsData( leScoreType ) )
                    {
                        liScore = lpRoadScoreDataEntry->GetScore( leScoreType );
                        lCarID  = lpRoadScoreDataEntry->GetCarID( leScoreType );
                    }
                }

                const s32 liRecordIndex = lChallengeIndex * BrnStreetData::E_SCORE_TYPE_COUNT + leScoreType;
                if ( lpUploadData->SetScoreData( liRecordIndex, liScore, lCarID ) )
                {
                    // The batch is full. A challenge cut short before its last score type is
                    // uploaded again, whole, by the next batch.
                    if ( leScoreType != BrnStreetData::E_SCORE_TYPE_COUNT - 1 )
                    {
                        --lChallengeIndex;
                    }
                    lbSpaceLeft = false;
                    break;
                }
            }
        }

        miNumRoadsConsideredForUpload = lChallengeIndex - mIndexOfNextChallengeToUpload;
    }

    // -----------------------------------------------------------------------------
    // A road-rules high-score download batch finished. On success, every downloaded score of the
    // batch's challenges (up to ten, two score types each) is handed to the game -- scores set by
    // players who are no longer friends are dropped -- and the next batch is scheduled; with no
    // data, or once the last challenge is covered, the download cycle ends (idle-online). On
    // failure the cycle ends and a custom-commands error is cleared.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_DownloadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        CGS_ASSERT( lpData != 0, "lpData" );
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT( lpRoadRulesManager != 0, "lpRoadRulesManager" );

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_DOWNLOADING )
        {
            RoadRulesDownloadData* lpDownloadedData = static_cast<RoadRulesDownloadData*>( lpResult );
            CGS_ASSERT( lpDownloadedData != 0, "lpDownloadedData" );

            if ( lpDownloadedData->GetNumScores() == 0 )
            {
                lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                lpRoadRulesManager->meState                         = KI_STATE_AUTO_LOGIN_PRIMED;
                return;
            }

            Road::ChallengeIndex lEndDownloadIndex =
                lpRoadRulesManager->mIndexOfNextChallengeToDownload + KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            if ( lEndDownloadIndex >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lEndDownloadIndex = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            }

            s32 liDownloadIndex = 0;
            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextChallengeToDownload;
                  lChallengeIndex < lEndDownloadIndex; ++lChallengeIndex )
            {
                RoadRulesDownloadEvent lDownloadEvent;
                lDownloadEvent.Construct();

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != 0, "lpRoadRulesManager->mpNetworkModule" );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
                {
                    char lacName[KI_ROAD_RULES_NAME_LENGTH];
                    s32  liScoreType;
                    s32  liQuantisedScore;
                    lpDownloadedData->GetRoadRulesDataForDownloadIndex( liDownloadIndex, lacName, &liScoreType,
                                                                        &liQuantisedScore );
                    CGS_ASSERT( leScoreType == static_cast<BrnStreetData::ScoreType>( liScoreType ),
                                "leScoreType == static_cast<BrnStreetData::ScoreType>(liScoreType)" );

                    BrnNetworkManager* lpNetworkManager = lpRoadRulesManager->mpNetworkModule->GetNetworkManager();
                    CgsNetwork::PlayerName lPlayerName;
                    lPlayerName.Construct( lacName );

                    if ( lacName[0] != 0 && !lpNetworkManager->GetBuddyManager()->IsFullBuddy( &lPlayerName ) )
                    {
                        *CgsDev::Log::gpDebugPrint << "Discarding downloaded road rule for " << lacName
                                                   << " because he is no longer our friend\n";
                    }
                    else
                    {
                        lDownloadEvent.mRoadRulesData.maScores[leScoreType] = liQuantisedScore;
                        lDownloadEvent.maPlayerNames[leScoreType].Construct( lacName );
                        if ( lacName[0] != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint << "Downloaded road rule for " << lacName
                                                       << " because he is still our friend\n";
                        }
                    }

                    ++liDownloadIndex;
                }

                lDownloadEvent.mRoadRulesData.mChallengeIndex = lChallengeIndex;

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != 0,
                            "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()" );
                BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
                    lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
                CGS_ASSERT( lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue() != 0,
                            "lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()" );
                lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()->AddEvent( lDownloadEvent );
            }

            lpRoadRulesManager->mIndexOfNextChallengeToDownload += KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            lpRoadRulesManager->mTimeUntilNextResultDownload.SetFloatVal( KF_TIME_BETWEEN_DOWNLOAD_BATCHES );

            if ( lpRoadRulesManager->mIndexOfNextChallengeToDownload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
                }
            }
        }
        else
        {
            BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
            CGS_ASSERT( lpServerInterface != 0, "lpServerInterface" );

            lpRoadRulesManager->mIndexOfNextChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != 0,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }

    // -----------------------------------------------------------------------------
    // The local player's own road-rules download batch finished. On success, each challenge of
    // the batch gets its two downloaded scores (by score type) as a score record handed to the
    // game, and the next batch is scheduled; once the last challenge is covered the local scores
    // count as downloaded and the cycle ends (idle-online). On failure the cycle ends and a
    // custom-commands error is cleared.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_DownloadLocalRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        CGS_ASSERT( lpData != 0, "lpData" );
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT( lpRoadRulesManager != 0, "lpRoadRulesManager" );

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_DOWNLOADING_LOCAL )
        {
            RoadRulesLocalPlayerDownloadedScores* lpDownloadedData =
                static_cast<RoadRulesLocalPlayerDownloadedScores*>( lpResult );
            CGS_ASSERT( lpDownloadedData != 0, "lpDownloadedData" );

            Road::ChallengeIndex lEndDownloadIndex =
                lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload + KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            if ( lEndDownloadIndex >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lEndDownloadIndex = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            }

            s32 liDownloadIndex = 0;
            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload;
                  lChallengeIndex < lEndDownloadIndex; ++lChallengeIndex )
            {
                RoadRulesMessageData lScoreData;
                lScoreData.mChallengeIndex = 0;
                memset( lScoreData.maScores, 0, sizeof( lScoreData.maScores ) );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++ )
                {
                    s32 liScoreType;
                    s32 liScore;
                    lpDownloadedData->GetRoadRulesDataForDownloadIndex( liDownloadIndex, &liScoreType, &liScore );
                    lScoreData.maScores[liScoreType] = liScore;
                    ++liDownloadIndex;
                }

                lScoreData.mu64RoadRulesID = lpDownloadedData->GetRoadRulesID();
                lScoreData.mChallengeIndex = lChallengeIndex;

                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule != 0, "lpRoadRulesManager->mpNetworkModule" );
                CGS_ASSERT( lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != 0,
                            "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()" );
                BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
                    lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
                CGS_ASSERT( lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue() != 0,
                            "lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue()" );
                lpNetworkToGameStateInterface->GetLocalRoadRulesDownloadedQueue()->AddEvent( lScoreData );
            }

            lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload += KI_NUM_CHALLENGES_PER_DOWNLOAD_BATCH;
            lpRoadRulesManager->mTimeUntilNextResultDownload.SetFloatVal( KF_TIME_BETWEEN_DOWNLOAD_BATCHES );

            if ( lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload >= KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState                 = KI_STATE_AUTO_LOGIN_PRIMED;
                    lpRoadRulesManager->mbDownloadedLocalScores = true;
                }
            }
        }
        else
        {
            BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
            CGS_ASSERT( lpServerInterface != 0, "lpServerInterface" );

            lpRoadRulesManager->mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != 0,
                        "lpServerInterface->GetCustomCommandsComponent()" );
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }

    // -----------------------------------------------------------------------------
    // Reliable road-rules message from another player: unpack its lobby scores into a received
    // record stamped with the sender's name and id, and queue it for the game (into the manager's
    // own buffer while received scores are being buffered).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesMessageArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                                    NetworkPlayerID lSendingPlayerID, void* lpUserData )
    {
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpUserData );
        CGS_ASSERT(lpRoadRulesManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager",
            CGS_ARTIST_FILE, 1722 /* ARTIST 0x82554F74 */));

        RoadRulesMessage* lpRoadRulesMessage = static_cast<RoadRulesMessage*>( lpMessage );
        CGS_ASSERT(lpRoadRulesMessage != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesMessage",
            CGS_ARTIST_FILE, 1726 /* ARTIST 0x82554F98 */));

        s32                  liNumRoadRulesScoresRecv;
        RoadRulesMessageData laRoadRulesMessageData[KI_MAX_ROAD_RULES_MESSAGE_ENTRIES];
        const bool lbRetrieved = lpRoadRulesMessage->Retrieve( &liNumRoadRulesScoresRecv, laRoadRulesMessageData );
        CGS_ASSERT(lbRetrieved, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesMessage->Retrieve(&liNumRoadRulesScoresRecv, laRoadRulesMessageData)",
            CGS_ARTIST_FILE, 1727 /* ARTIST 0x82554FD0 */));

        CGS_ASSERT(lpRoadRulesManager->mpPlayerManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpPlayerManager",
            CGS_ARTIST_FILE, 1730 /* ARTIST 0x82554FF8 */));
        CgsNetwork::PlayerMenuData* lpPlayerMenuData =
            lpRoadRulesManager->mpPlayerManager->GetMenuDataByID( lSendingPlayerID );
        CGS_ASSERT(lpPlayerMenuData != 0, CGS_ASSERT_MESSAGE_AT(
            "lpPlayerMenuData",
            CGS_ARTIST_FILE, 1732 /* ARTIST 0x8255502C */));

        CgsNetwork::PlayerName lPlayerName;
        lPlayerName.Construct( lpPlayerMenuData->macName );

        RoadRulesRecvData lRoadRulesRecvData;
        XMemCpy( lRoadRulesRecvData.maRoadRulesData, laRoadRulesMessageData,
                sizeof( RoadRulesMessageData ) * liNumRoadRulesScoresRecv );
        lRoadRulesRecvData.miNumRoadRulesScoresRecv = liNumRoadRulesScoresRecv;
        lRoadRulesRecvData.mPlayerName              = lPlayerName;
        lRoadRulesRecvData.mPlayerID                = lSendingPlayerID;

        CGS_ASSERT(lpRoadRulesManager->mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpNetworkModule",
            CGS_ARTIST_FILE, 1740 /* ARTIST 0x82555098 */));
        CGS_ASSERT(lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface() != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface()",
            CGS_ARTIST_FILE, 1741 /* ARTIST 0x825550C4 */));
        BrnNetworkModuleIO::NetworkToGameStateInterface* lpNetworkToGameStateInterface =
            lpRoadRulesManager->mpNetworkModule->GetNetworkToGameStateInterface();
        CGS_ASSERT(lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue() != 0, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkToGameStateInterface->GetRoadRulesDownloadedQueue()",
            CGS_ARTIST_FILE, 1743 /* ARTIST 0x825550F4 */));

        if ( lpRoadRulesManager->mbBufferRoadRulesReceived )
        {
            CGS_ASSERT(lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetLength() < lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetMaxLength(), CGS_ASSERT_MESSAGE_AT(
                "lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetLength() < " "lpRoadRulesManager->mBufferedRoadRulesRecvQueue.GetMaxLength()",
                CGS_ARTIST_FILE, 1750 /* ARTIST 0x8255512C */));
            lpRoadRulesManager->mBufferedRoadRulesRecvQueue.AddEvent( lRoadRulesRecvData );
        }
        else
        {
            CGS_ASSERT(lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetLength() < lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetMaxLength(), CGS_ASSERT_MESSAGE_AT(
                "lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetLength() < " "lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->GetMaxLength()",
                CGS_ARTIST_FILE, 1756 /* ARTIST 0x8255516C */));
            lpNetworkToGameStateInterface->GetRoadRulesReceivedQueue()->AddEvent( lRoadRulesRecvData );
        }
    }

    // -----------------------------------------------------------------------------
    // Delivery report for a road-rules message: only a fake nack is logged.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesMessageDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                                      CgsNetwork::SignalMessage* lpAck,
                                                                      NetworkPlayerID lRecvingPlayerID, void* lpUserData )
    {
        (void)lbDelivered;
        (void)lpAck;
        (void)lRecvingPlayerID;
        (void)lpUserData;

        if ( lbFakeNack )
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint << "BrnNetwork::NetworkRoadRulesManager::_RoadRulesMessageDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // -----------------------------------------------------------------------------
    // Reliable personal-best message from another player: unpack it into a personal-best event
    // stamped with the sender, flag whether the sender is a full buddy, and either queue it
    // straight to the game (while received scores are being buffered) or hold it in the
    // personal-best buffer for ProcessBeforeSimulation.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesPersonalBestArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                                         NetworkPlayerID lSendingPlayerID, void* lpUserData )
    {
        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpUserData );
        CGS_ASSERT(lpRoadRulesManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager",
            __FILE__, 1819 /* ARTIST 0x8256457C */));

        CGS_ASSERT(lpRoadRulesManager->mpPlayerManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpPlayerManager",
            __FILE__, 1822 /* ARTIST 0x825645A4 */));
        CgsNetwork::PlayerMenuData* lpPlayerMenuData =
            lpRoadRulesManager->mpPlayerManager->GetMenuDataByID( lSendingPlayerID );
        CGS_ASSERT(lpPlayerMenuData != 0, CGS_ASSERT_MESSAGE_AT(
            "lpPlayerMenuData",
            __FILE__, 1824 /* ARTIST 0x825645D8 */));

        CgsNetwork::PlayerName lPlayerName;
        lPlayerName.Construct( lpPlayerMenuData->macName );

        BrnNetworkModuleIO::NetworkOutRecvRoadRulesPBEvent lRoadRulesPbEvent;
        lRoadRulesPbEvent.mPersonalBestPlayerID = lSendingPlayerID;

        RoadRulesPersonalBestMessage* lpRoadRulesPBMessage = static_cast<RoadRulesPersonalBestMessage*>( lpMessage );
        CGS_ASSERT(lpRoadRulesPBMessage != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesPBMessage",
            __FILE__, 1833 /* ARTIST 0x8256460C */));
        lpRoadRulesPBMessage->Retrieve( &lPlayerName, &lRoadRulesPbEvent.mPersonalBestChallengeIndex,
                                        &lRoadRulesPbEvent.mPersonalBestScore );

        CGS_ASSERT(lpRoadRulesManager->mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpNetworkModule",
            __FILE__, 1839 /* ARTIST 0x82564648 */));
        CGS_ASSERT(lpRoadRulesManager->mpNetworkModule->GetNetworkManager() != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager->mpNetworkModule->GetNetworkManager()",
            __FILE__, 1840 /* ARTIST 0x82564674 */));
        BuddyManagerX360* lpBuddyManager = lpRoadRulesManager->mpNetworkModule->GetNetworkManager()->GetBuddyManager();
        CGS_ASSERT(lpBuddyManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpBuddyManager",
            __FILE__, 1842 /* ARTIST 0x825646A8 */));
        lRoadRulesPbEvent.mbWasPBByFriend = lpBuddyManager->IsFullBuddy( &lPlayerName );

        if ( !lpRoadRulesManager->mbBufferRoadRulesReceived )
        {
            lpRoadRulesManager->mRoadRulesPersonalBestBuffer.Push( &lRoadRulesPbEvent );
        }
        else
        {
            CGS_ASSERT(lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue() != 0, CGS_ASSERT_MESSAGE_AT(
                "lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()",
                __FILE__, 1855 /* ARTIST 0x82564704 */));
            lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lRoadRulesPbEvent ),
                lRoadRulesPbEvent.GetEventType(), sizeof( lRoadRulesPbEvent ) );
        }
    }

    // -----------------------------------------------------------------------------
    // Delivery report for a personal-best message: only a fake nack is logged.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_RoadRulesPersonalBestDeliveredCallback( bool lbDelivered, bool lbFakeNack,
                                                                           CgsNetwork::SignalMessage* lpAck,
                                                                           NetworkPlayerID lRecvingPlayerID, void* lpUserData )
    {
        (void)lbDelivered;
        (void)lpAck;
        (void)lRecvingPlayerID;
        (void)lpUserData;

        if ( lbFakeNack )
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Fack Nack found in ";
            *CgsDev::Log::gpDebugPrint << "BrnNetwork::NetworkRoadRulesManager::_RoadRulesPersonalBestDeliveredCallback";
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // -----------------------------------------------------------------------------
    // Upload completion. On success while UPLOADING: mark the batch's local scores clean, tell the
    // game which challenge range was uploaded, advance the upload cursor past the batch, restart
    // the inter-upload timer, and once the table is done leave the upload cycle (idle-online
    // unless IN_GAME) and clear the force-overwrite flag. Otherwise leave the upload cycle the
    // same way and clear a pending custom-commands error.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::_UploadRoadRulesCallback( void* lpData, void* lpResult, bool lbSuccess )
    {
        (void)lpResult;

        NetworkRoadRulesManager* lpRoadRulesManager = static_cast<NetworkRoadRulesManager*>( lpData );
        CGS_ASSERT(lpRoadRulesManager != 0, CGS_ASSERT_MESSAGE_AT(
            "lpRoadRulesManager",
            CGS_ARTIST_FILE, 1905 /* ARTIST 0x82564768 */));

        BrnServerInterface* lpServerInterface = lpRoadRulesManager->mpServerInterface;
        CGS_ASSERT(lpServerInterface != 0, CGS_ASSERT_MESSAGE_AT(
            "lpServerInterface",
            CGS_ARTIST_FILE, 1908 /* ARTIST 0x82564790 */));

        if ( lbSuccess && lpRoadRulesManager->meState == E_ROAD_RULES_STATE_UPLOADING )
        {
            const Road::ChallengeIndex lEndUploadIndex =
                lpRoadRulesManager->mIndexOfNextChallengeToUpload + lpRoadRulesManager->miNumRoadsConsideredForUpload;

            for ( Road::ChallengeIndex lChallengeIndex = lpRoadRulesManager->mIndexOfNextChallengeToUpload;
                  lChallengeIndex < lEndUploadIndex; ++lChallengeIndex )
            {
                CGS_ASSERT(lChallengeIndex >= 0, CGS_ASSERT_MESSAGE_AT(
                    "lChallengeIndex >= 0",
                    CGS_ARTIST_FILE, 1925 /* ARTIST 0x8256480C */));
                CGS_ASSERT(lChallengeIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS, CGS_ASSERT_MESSAGE_AT(
                    "lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES",
                    CGS_ARTIST_FILE, 1926 /* ARTIST 0x8256482C */));

                BrnStreetData::ChallengePlayerScoreEntry* lpChallengeData =
                    &lpRoadRulesManager->maLocalRoadScoresToUpload[lChallengeIndex];
                CGS_ASSERT(lpChallengeData != 0, CGS_ASSERT_MESSAGE_AT(
                    "lpChallengeData",
                    CGS_ARTIST_FILE, 1929 /* ARTIST 0x8256484C */));
                lpChallengeData->mDirty.UnSetAll();
            }

            BrnNetworkModuleIO::NetworkOutRecvRoadRulesUploadedEvent lUploadedEvent;
            lUploadedEvent.mStartUploadIndex = lpRoadRulesManager->mIndexOfNextChallengeToUpload;
            lUploadedEvent.mEndUploadIndex   = lEndUploadIndex;

            CGS_ASSERT(lpRoadRulesManager->mpNetworkModule != 0, CGS_ASSERT_MESSAGE_AT(
                "lpRoadRulesManager->mpNetworkModule",
                CGS_ARTIST_FILE, 1937 /* ARTIST 0x82564898 */));
            CGS_ASSERT(lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue() != 0, CGS_ASSERT_MESSAGE_AT(
                "lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()",
                CGS_ARTIST_FILE, 1938 /* ARTIST 0x825648C4 */));
            lpRoadRulesManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lUploadedEvent ),
                lUploadedEvent.GetEventType(), sizeof( lUploadedEvent ) );

            CGS_ASSERT(lpServerInterface->GetDownloadableConfigComponent() != 0, CGS_ASSERT_MESSAGE_AT(
                "lpServerInterface->GetDownloadableConfigComponent()",
                CGS_ARTIST_FILE, 1942 /* ARTIST 0x82564904 */));

            lpRoadRulesManager->mIndexOfNextChallengeToUpload = lEndUploadIndex;
            lpRoadRulesManager->miNumRoadsConsideredForUpload = 0;
            lpRoadRulesManager->mTimeUntilNextResultUpload.SetFloatVal(
                lpServerInterface->GetDownloadableConfigComponent()->TimeBetweenRoadRulesUploads() );

            if ( lpRoadRulesManager->mIndexOfNextChallengeToUpload == KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS )
            {
                if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
                {
                    lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
                }
                lpRoadRulesManager->mbForceOverwriteServerRecords = false;
            }
        }
        else
        {
            if ( lpRoadRulesManager->meState != E_ROAD_RULES_STATE_IN_GAME )
            {
                lpRoadRulesManager->meState = KI_STATE_AUTO_LOGIN_PRIMED;
            }

            CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent() != 0, CGS_ASSERT_MESSAGE_AT(
                "lpServerInterface->GetCustomCommandsComponent()",
                CGS_ARTIST_FILE, 1971 /* ARTIST 0x82564984 */));
            if ( lpServerInterface->GetCustomCommandsComponent()->GetStatus()
                 == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR )
            {
                lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            }
        }
    }
}

#undef CGS_ARTIST_FILE
