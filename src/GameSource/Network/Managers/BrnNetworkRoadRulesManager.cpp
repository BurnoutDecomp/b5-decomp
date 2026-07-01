// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- recovered function bodies
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.cpp
//
// This TU homes the X360-recovered functions of the online road-rules sync manager that are
// tractable WITHOUT re-deriving the internal sub-object layout of RoadRulesData's embedded
// RoadRulesMessage/RoadRulesPersonalBestMessage members or the two FIFO buffers -- i.e. every
// function below only ever touches the manager's OWN scalar members (mpServerInterface,
// mpNetworkModule, mpTimeManager, mpPlayerManager, meState, the mIndexOfNext* cursors, the
// two mTimeUntilNextResult* timers, muTimeStampOfLastDownload, miNumRoadsConsideredForUpload,
// mbBufferRoadRulesReceived, maRoadRulesScoreKey) or plain by-value array elements it doesn't
// need to look inside (OnEnterGame's ChallengeData::Construct() loop). Wave34 previously tried
// a full rewrite that also touched the un-named RoadRulesData/FIFO sub-object offsets and had
// to be reverted for changing the committed layout incompatibly -- this pass is additive only:
// it keeps the original ctor's field-cleared exactly as before (now under their DWARF names)
// and does not touch maRoadRulesData[]'s internal message-object bytes.
//
//   NetworkRoadRulesManager()              @ 0x827E2BE8  (the C++ constructor)
//   GetNextFreeRoadRulesDataEntry()        @ 0x82549F60  (first free per-player slot)
//   GetRoadRulesDataForPlayer(NetworkPlayerID) @ 0x82549ED0 (slot lookup by player)
//   OnAutoLogin()                          @ 0x82564338  (auto-login config pull + kickoff)
//   Prepare()                              @ 0x8255C808  (latch collaborator pointers)
//   OnEnterGame()                          @ 0x8254A4B0  (re-construct local-lobby scores)
//   OnGameLaunching()                      @ 0x8254A500  (clear download cursors + auto-login signal)
//   OnGameFinish()                         @ 0x82554D30  (clear download cursors on game leave)
//   OnRoundFinish()                        @ 0x82554DE8  (clear state+cursors on game leave)
//   OnLeaveGame()                          @ 0x82554EA0  (drop to booting unless mid-cycle)
//   AttemptToDownloadLocalRoadRulesHighScores() @ 0x8254A6B8
//   AttemptToDownloadRoadRulesHighScores() @ 0x8254A5D8
//   AttemptToUploadNewRoadRulesScores()    @ 0x82554AD8
//   StartDownloadingLocalRoadRulesScoresFromServer() @ 0x8254A360
//   StartDownloadingRoadRulesScoresFromServer()      @ 0x8254A210
//   StartUploadingRoadRulesScoresToServer()          @ 0x8254A0B8
//   HandleDownloadingRoadRulesScores()     @ 0x825611D0
//   HandleDownloadingLocalRoadRulesScores()@ 0x82561370
//   ProcessGameActions()                   @ 0x8255CA48  (PrepareForMode/StopMode -> mbBufferRoadRulesReceived)
//   ProcessAfterSimulation()               @ 0x8256F648  (per-frame dispatcher)
//
// NOT bodied in this pass (thread un-named RoadRulesData / FIFO sub-object offsets, or an
// unrecovered helper's output struct -- see the class banner in the header for why): Construct,
// Destruct, Release, AddPlayer, RemovePlayer, Disconnected, ProcessBeforeSimulation,
// ProcessNetworkEvents, SendPersonalBestScore, HandleSendingRoadRulesScores,
// HandleUploadingRoadRulesScores (its finish-vs-continue branch depends on
// GetRoadRulesDataToUpload's undeclared output record/count), HandleNewPersonalBest (real DWARF
// param is NetworkInRoadRulesPBEvent*, a type not reconstructed anywhere in this codebase yet --
// bodying it would need either that type's real layout or a raw-offset poke into a
// non-serialised C++ object, both against policy), StartSendingRoadRulesScoresToPlayer, the
// three _...Callback statics, GetRoadRulesDataEntry/ToSend/ToUpload (declared-only where
// DWARF-named).
//
// Every store / branch / constant below is grounded in the X360 assembly (dossier postmortem,
// scratchpad/wave33 + the live `work show --full --asm` dossier). Sub-object layout/offsets are
// by-name only; the absolute X360 byte offsets are quoted as provenance comments.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameSource/Network/BrnNetworkModule.h"                                    // BrnNetworkModule (GetGameStateToNetworkInterface / GetNetworkManager / GetNetworkEventQueue)
#include "GameSource/Network/BrnNetworkManager.h"                                   // BrnNetworkManager::OnAutoLoginProcessComplete
#include "GameSource/Network/BrnNetworkModuleIO.h"                                  // BrnNetworkModuleIO::NetworkEventQueue
#include "GameSource/Network/BrnServerInterface.h"                                  // BrnServerInterface::GetServerInfoComponent / GetConnectionComponent / GetGameComponent / GetCustomCommandsComponent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"      // GameStateToNetworkInterface::GetCurrentGameMode
#include "GameSource/GameState/BrnGameStateSharedIO.h"                              // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h" // ServerInterfaceServerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection::IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames::IsLocalPlayerInGame / IsGameStarted
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"         // ServerInterfaceCustomCommands::Get*/SetRoadRulesForLocalPlayer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                    // VariableEventQueue<14000,16> / <13312,16>

namespace BrnNetwork
{
    namespace
    {
        // The BrnNetworkModuleIO::NetworkEventQueue accessor return is typed against an incomplete
        // forward; the concrete queue is the 14000/16 variable-event queue (same pattern as
        // BrnChallengeSuccessManager.cpp / BrnEventScoresManager.cpp).
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsConcreteQueue(
            BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        // X360 immediates from OnAutoLogin's AddEvent(&v10, 0x26, 4): the reset-date OUT-event
        // type id and its 4-byte timestamp payload size.
        const s32 KI_OUTEVENT_ROAD_RULES_RESET_DATE      = 0x26;   // 38
        const s32 KI_OUTEVENT_ROAD_RULES_RESET_DATE_SIZE = 4;

        // X360 immediate: meState value once the auto-login config has been primed (stw 5,0x3634).
        // NOTE: this build's literal meState values (OnAutoLogin stores 5; ProcessAfterSimulation's
        // switch dispatches on 1/2/3, both confirmed against the raw asm cmpwi's) do NOT line up
        // with the DecFIGS DWARF ERoadRulesState enum (BOOTING=0,CONNECTING=1,IN_GAME=2,
        // WAIT_TO_DOWNLOAD=3,UPLOADING=4,DOWNLOADING=5,DOWNLOADING_LOCAL=6) -- a merge-window/
        // version delta between the DWARF (PS3 Dec-2007) and this ARTIST (X360 Jan-2008) build.
        // Per the source-of-truth ladder the asm wins for behaviour, so meState's numeric values
        // below are quoted from the asm and are NOT claimed to be the DWARF-named enumerators.
        const s32 KI_STATE_AUTO_LOGIN_PRIMED = 5; // asm-grounded literal; NOT DWARF's E_..._DOWNLOADING

        // X360 GetStringFromClientConfig out-length immediate (li r6,0x10).
        const s32 KI_SCORE_KEY_LENGTH = 16;

        // DWARF BrnNetworkRoadRulesManager.cpp:45 -- max scores requested per download batch.
        const s32 KI_NUM_SCORES_TO_DOWNLOAD_IN_A_BATCH = 10;
    }

    // -----------------------------------------------------------------------------
    // NetworkRoadRulesManager() @ 0x827E2BE8
    //
    // The compiler-synthesised C++ constructor. The X360 body installs the four embedded
    // sub-object vtables inside each of the seven RoadRulesData slots (the `stw off_820CFD98`
    // / `stw off_820CFDC0` stores at slot-relative +0x000/+0x120/+0x240/+0x278, repeated at the
    // +0x2B8 slot stride) plus the trailing sub-object vtable at +0x3650 (`stw off_820CE18C`),
    // and clears this object's own upload/download timer pair
    // (the two `stw 0` + two `stfs flt_82001CC0` (== 0.0f) stores at +0x3610..+0x361C -- the
    // CgsSystem::Time { miSeconds; mfFraction; } fields of mTimeUntilNextResultUpload/Download).
    //
    // The per-slot vtable installs are the inlined construction of each slot's embedded
    // sub-objects (and the +0x3650 install is a trailing embedded sub-object's vtable); those
    // are owned by the sub-objects' own constructors, exactly as the binary inlines them here,
    // so the human constructor lets them construct themselves and only writes this object's own
    // grounded scalar defaults. (The manager's pointer/state/flag members -- mpServerInterface,
    // mpNetworkModule, meState, mbDownloadedLocalScores -- are NOT touched by this ctor in the
    // asm; they are initialised later by the manager's lifecycle Construct().)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::NetworkRoadRulesManager()
        : mTimeUntilNextResultUpload( 0.0f )    // X360 +0x3610  stw r8(=0) / stfs flt_82001CC0(==0.0f)
        , mTimeUntilNextResultDownload( 0.0f )  // X360 +0x3618  stw r8(=0) / stfs flt_82001CC0(==0.0f)
    {
        // (the per-slot sub-object vtables and the +0x3650 sub-object vtable are installed by the
        //  embedded sub-objects' own inlined constructors, matching the binary.)
    }

    // -----------------------------------------------------------------------------
    // GetNextFreeRoadRulesDataEntry() @ 0x82549F60
    //
    // Walk the seven per-player slots looking for a free one (mPlayerID == -1). Return the first
    // free slot. If all seven are in use, fire the "no free entry" assert and return nullptr.
    // (X360: scan starts at this+0x2B0 == &maRoadRulesData[0].mPlayerID, stride 0x2B8; the sentinel
    //  compared is -1; the returned pointer is the slot base 696*index + this.)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::RoadRulesData* NetworkRoadRulesManager::GetNextFreeRoadRulesDataEntry()
    {
        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_PLAYER_DATA_SLOTS; ++liIndex )
        {
            if ( maRoadRulesData[liIndex].mPlayerID == -1 )
            {
                return &maRoadRulesData[liIndex];
            }
        }

        CGS_ASSERT( false, "Unable to find a free road rules data entry\n" );
        return nullptr;
    }

    // -----------------------------------------------------------------------------
    // GetRoadRulesDataForPlayer(NetworkPlayerID lPlayerID) @ 0x82549ED0
    //
    // Assert the player id is valid, then walk the seven per-player slots for the one keyed by
    // lPlayerID and return it; return nullptr if no slot matches.
    // (X360: scan starts at this+0x2B0 == &maRoadRulesData[0].mPlayerID, stride 0x2B8; the returned
    //  pointer is the slot base 696*index + this.)
    // -----------------------------------------------------------------------------
    NetworkRoadRulesManager::RoadRulesData* NetworkRoadRulesManager::GetRoadRulesDataForPlayer(
        NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT( lPlayerID != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" );

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
    // OnAutoLogin() @ 0x82564338
    //
    // If the auto-login config has already been primed (meState != 0): just tell the network
    // manager the auto-login process completed (stage 0).
    //
    // Otherwise (meState == 0): only proceed when the current game mode warrants pulling road-rules
    // config -- offline modes (gameMode < E_MODE_ONLINE_MODE_START), the online free-burn lobby
    // (E_MODE_ONLINE_FREE_BURN_LOBBY), or online showtime (E_MODE_ONLINE_SHOWTIME). When it does:
    //   * read the road-rules score key + reset date out of the server-info client config,
    //   * post the reset date onto the network event queue (out-event 0x26, 4-byte payload),
    //   * download local road-rules high scores (unless we already have them),
    //   * download the global road-rules high scores, upload any new local scores,
    //   * and mark the manager primed (meState = 5).
    //
    // X360 offsets (provenance): meState @+0x3634, mpServerInterface @+0x3644,
    // mpNetworkModule @+0x3648, mbDownloadedLocalScores @+0x364C, maRoadRulesScoreKey @+0x3588.
    // The server-info component pointer is *(mpServerInterface + 0x2C) == GetServerInfoComponent().
    // Game-mode immediates: 0xA == E_MODE_ONLINE_MODE_START, 0xF == E_MODE_ONLINE_FREE_BURN_LOBBY,
    // 0x10 == E_MODE_ONLINE_SHOWTIME (read from the asm cmpwi's).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnAutoLogin()
    {
        if ( meState != 0 )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );

            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            lpNetworkManager->OnAutoLoginProcessComplete( 0 );
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leGameMode =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();

        const bool lbShouldPrime =
            ( leGameMode <  BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_START )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY )
            || ( leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME );

        if ( !lbShouldPrime )
        {
            return;
        }

        CGS_ASSERT( mpServerInterface != nullptr, "mpServerInterface" );
        CGS_ASSERT( mpServerInterface->GetServerInfoComponent() != nullptr,
                    "mpServerInterface->GetServerInfoComponent()" );

        mpServerInterface->GetServerInfoComponent()->GetStringFromClientConfig(
            "ROAD_RULES_SKEY", maRoadRulesScoreKey, KI_SCORE_KEY_LENGTH );

        const s32 liResetDate =
            mpServerInterface->GetServerInfoComponent()->GetTimeStampFromClientConfig(
                "ROAD_RULES_RESET_DATE" );

        CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
        CGS_ASSERT( mpNetworkModule->GetNetworkEventQueue() != nullptr,
                    "mpNetworkModule->GetNetworkEventQueue()" );

        AsConcreteQueue( mpNetworkModule->GetNetworkEventQueue() )->AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &liResetDate ),
            KI_OUTEVENT_ROAD_RULES_RESET_DATE, KI_OUTEVENT_ROAD_RULES_RESET_DATE_SIZE );

        if ( !mbDownloadedLocalScores )
        {
            AttemptToDownloadLocalRoadRulesHighScores();
        }

        AttemptToDownloadRoadRulesHighScores();
        AttemptToUploadNewRoadRulesScores();

        meState = KI_STATE_AUTO_LOGIN_PRIMED;
    }

    // -----------------------------------------------------------------------------
    // Prepare(BrnNetworkModule*, BrnServerInterface*, PlayerManager*, TimeManager*) @ 0x8255C808
    //
    // BrnNetworkManager lifecycle: latch the four collaborator pointers this manager needs, then
    // assert each is non-null, and reset the per-cycle scalars (the buffered-events queue index,
    // the "have downloaded local scores" flag, and the state machine, all zeroed/booting).
    //
    // X360 offsets (provenance): mpTimeManager @+0x363C, mpPlayerManager @+0x3640,
    // mpServerInterface @+0x3644, mpNetworkModule @+0x3648, (a1+10000)=+0x2710 (an internal
    // mRoadRulesPersonalBestBuffer index, not modelled by name -- left as opaque storage; the
    // X360 store is a "reset to empty" that the RoadRulesData/FIFO fields not bodied in this TU
    // already need for the queue's own layout), mbDownloadedLocalScores @+0x364C, meState @+0x3634.
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

        mbDownloadedLocalScores = false;
        meState = 0; // E_ROAD_RULES_STATE_BOOTING

        return true;
    }

    // -----------------------------------------------------------------------------
    // OnEnterGame() @ 0x8254A4B0
    //
    // BrnNetworkManager::OnEnterGame forward: re-construct every local-lobby challenge-data slot
    // in place (matching the binary's per-slot ChallengeData::Construct() re-run over all 64).
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnEnterGame()
    {
        for ( s32 liIndex = 0; liIndex < KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS; ++liIndex )
        {
            maLocalLobbyScores[liIndex].Construct();
        }
    }

    // -----------------------------------------------------------------------------
    // OnGameLaunching() @ 0x8254A500
    //
    // BrnNetworkManager::OnGameLaunching forward: clear the three download cursors, and if the
    // manager was mid-cycle (downloading/downloading-local/uploading/primed), signal the network
    // manager that the auto-login process has completed before dropping back to booting.
    //
    // X360 offsets: mIndexOfNextChallengeToUpload/Download/mIndexOfNextLocalChallengeToDownload
    // @+0x3620/+0x3624/+0x3628, meState @+0x3634, mpNetworkModule @+0x3648.
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::OnGameLaunching()
    {
        const s32 liPreviousState = meState;

        mIndexOfNextChallengeToUpload        = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextChallengeToDownload      = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;
        mIndexOfNextLocalChallengeToDownload = KI_NUMBER_OF_LOCAL_CHALLENGE_SLOTS;

        if ( liPreviousState == KI_STATE_AUTO_LOGIN_PRIMED || liPreviousState == 1 || liPreviousState == 2 || liPreviousState == 3 )
        {
            CGS_ASSERT( mpNetworkModule != nullptr, "mpNetworkModule" );
            CGS_ASSERT( mpNetworkModule->GetNetworkManager() != nullptr,
                        "mpNetworkModule->GetNetworkManager()" );

            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            lpNetworkManager->OnAutoLoginProcessComplete( 0 );
        }

        meState = 4; // asm-grounded unconditional v1[3469] = 4 store (DWARF names 4 E_ROAD_RULES_STATE_UPLOADING,
             // but see the meState version-drift note in the anonymous namespace above)
    }

    // -----------------------------------------------------------------------------
    // OnGameFinish() @ 0x82554D30 / OnRoundFinish() @ 0x82554DE8 / OnLeaveGame() @ 0x82554EA0
    //
    // NOT BODIED (fresh-eyes review caught a wrong-offset bug in a prior attempt): the asm's
    // three cursor-clear stores target byte offsets +0x35F8/+0x35FC/+0x3600, which fall INSIDE
    // the still-opaque mPersonalBestToSendBuffer FIFO span (+0x3598..+0x3608,
    // FifoQueue<NetworkInRoadRulesPBEvent,2>), NOT the named mIndexOfNextChallengeToUpload/
    // ToDownload/ToLocalDownload members at +0x3620/+0x3624/+0x3628 (those are a different,
    // ~40-byte-later triplet -- confirmed by Construct/Destruct's own pseudocode zeroing both
    // triplets as separate statement blocks). Cannot body these three by name until
    // mPersonalBestToSendBuffer's FIFO internals are reconstructed with their own named fields.
    // -----------------------------------------------------------------------------

    // -----------------------------------------------------------------------------
    // AttemptToDownloadLocalRoadRulesHighScores() @ 0x8254A6B8
    //
    // OnAutoLogin callee: if the server connection is logged in and the local player is either
    // not in a game or in a game that hasn't started yet, kick off the local-scores download.
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
    // AttemptToDownloadRoadRulesHighScores() @ 0x8254A5D8
    //
    // OnAutoLogin callee: same shape as AttemptToDownloadLocalRoadRulesHighScores, but kicks off
    // the GLOBAL road-rules high-score download.
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
    // AttemptToUploadNewRoadRulesScores() @ 0x82554AD8
    //
    // OnAutoLogin callee: if the local player is NOT in an active/started game and the server
    // connection is logged in, kick off the upload cycle.
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
    // StartDownloadingLocalRoadRulesScoresFromServer() @ 0x8254A360
    //
    // AttemptToDownloadLocalRoadRulesHighScores callee: assert the server connection is logged in
    // and the local player isn't in a started game, reset the local-download cursor, and reset the
    // inter-download timer to zero (so HandleDownloadingLocalRoadRulesScores fires next tick).
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
        mTimeUntilNextResultDownload = CgsSystem::Time( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // StartDownloadingRoadRulesScoresFromServer() @ 0x8254A210
    //
    // AttemptToDownloadRoadRulesHighScores / debug "Get Road Rules High Scores" callee: same shape
    // as StartDownloadingLocalRoadRulesScoresFromServer, but resets the GLOBAL download cursor.
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
        mTimeUntilNextResultDownload = CgsSystem::Time( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // StartUploadingRoadRulesScoresToServer() @ 0x8254A0B8
    //
    // AttemptToUploadNewRoadRulesScores callee: assert the same connection/game preconditions,
    // reset the upload cursor and the "roads considered for upload" counter, and reset the
    // inter-upload timer to zero (so HandleUploadingRoadRulesScores fires next tick).
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
        mTimeUntilNextResultUpload = CgsSystem::Time( 0.0f );
    }

    // -----------------------------------------------------------------------------
    // HandleDownloadingRoadRulesScores() @ 0x825611D0
    //
    // ProcessAfterSimulation callee (meState == 2, the asm-grounded "downloading" dispatch value --
    // see the meState version-drift note in the anonymous namespace above): once the server's
    // custom-commands component is idle and the inter-download timer has elapsed, assert the usual
    // connection/game preconditions and request the next batch (<=10 remaining) of GLOBAL
    // road-rules high scores, continuing from mIndexOfNextChallengeToDownload.
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
    // HandleDownloadingLocalRoadRulesScores() @ 0x82561370
    //
    // ProcessAfterSimulation callee (meState == 3, the asm-grounded "downloading local" dispatch
    // value -- see the meState version-drift note above): same shape as
    // HandleDownloadingRoadRulesScores, but requests the next batch of LOCAL road-rules high
    // scores; the batch size is clamped to what's left of the 64 local challenge slots.
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
            liNumToDownload, mIndexOfNextLocalChallengeToDownload, maRoadRulesScoreKey,
            &_DownloadLocalRoadRulesCallback, this );
    }

    // -----------------------------------------------------------------------------
    // ProcessGameActions(a2 = game-action event queue) @ 0x8255CA48
    //
    // ProcessAfterSimulation callee: walk the frame's game-action event queue. Action id 23
    // (PrepareForMode) clears mbBufferRoadRulesReceived; action id 39 (StopMode) sets it. Every
    // other action id is ignored. (The queue's real element type is the game-action queue's own
    // VariableEventQueue<13312,16> instantiation, resolved from the X360-recovered GetFirstEvent/
    // GetNextEvent callees -- this function never dereferences an event payload, only tests
    // pointer truthiness for the two action ids' asserts.)
    // -----------------------------------------------------------------------------
    void NetworkRoadRulesManager::ProcessGameActions( const void* lpGameActionQueue )
    {
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueConcrete;
        const GameActionQueueConcrete* lpQueue =
            reinterpret_cast<const GameActionQueueConcrete*>( lpGameActionQueue );

        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liActionID = lpQueue->GetFirstEvent( &lpEvent, &liSize );

        while ( lpEvent != nullptr )
        {
            if ( liActionID == 23 ) // PrepareForMode
            {
                CGS_ASSERT( lpEvent != nullptr, "lpPrepareForModeAction" );
                mbBufferRoadRulesReceived = false;
            }
            else if ( liActionID == 39 ) // StopMode
            {
                CGS_ASSERT( lpEvent != nullptr, "lpStopModeAction" );
                mbBufferRoadRulesReceived = true;
            }

            liActionID = lpQueue->GetNextEvent( lpEvent, &lpEvent, &liSize );
        }
    }

    // -----------------------------------------------------------------------------
    // ProcessAfterSimulation(lpPostSimInput, lbHandleSendFlag) @ 0x8256F648
    //
    // NOT BODIED (fresh-eyes review caught a real dropped side effect in a prior attempt): the
    // X360 call sequence is PostSim(a2) -> ProcessNetworkEvents, THEN sub_8254EAD0(a2) resolving
    // the game-action queue -> ProcessGameActions (bodied above, but never wired up because the
    // sub_8254EAD0 accessor is not yet named in BrnNetworkModuleIO). Shipping this dispatcher
    // without that call silently drops a real per-frame side effect (mbBufferRoadRulesReceived
    // toggling, which SendPersonalBestScore/HandleSendingRoadRulesScores gate on) -- leaving it
    // declaration-only until sub_8254EAD0 is named rather than shipping an incomplete dispatcher
    // as done.
    // -----------------------------------------------------------------------------
}
