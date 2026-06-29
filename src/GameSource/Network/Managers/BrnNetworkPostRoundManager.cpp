// ===========================================================================
// BrnNetwork::PostRoundManager  -- the online post-round / post-game state machine.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Per-function X360 addresses are noted
// at each definition. The pervasive CgsDev::StrStream debug logging in the binary
// ("Starting action <name>", "ProcessComplete <ok/fail>") has no observable effect
// on the state machine and references the un-reconstructed global debug stream, so --
// as in the other committed BrnNetwork manager TUs -- it is not reproduced.
//
// The X360 build dispatches the per-substate handlers and the per-action handlers
// through two function-pointer tables filled at Construct time. They are modelled as
// tables of member-function pointers, so each handler is still reached through the
// table (matching the binary) while staying type-safe.
// ===========================================================================
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"            // BrnNetworkManager, SuspensionManager, LiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"  // SuspensionManager::Update
#include "GameSource/Network/BrnNetworkGameParams.h"         // BrnNetwork::GameParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h" // BrnNetwork::PlayerParams
#include "GameSource/GameState/BrnGameActions.h"             // GameStateModuleIO::OnlineGameResults
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"     // ServerInterfaceGames
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection::IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h" // ServerInterfaceEndGameData
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"  // PlayerManager
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"       // NetworkPlayerID / KU_INVALID_NETWORK_PLAYER_ID

namespace BrnNetwork
{
    // -----------------------------------------------------------------------
    // InitialiseUpdateArray  (X360 @ 0x82573DC0)
    //   Fill the per-substate update dispatch table; assert no slot was left null.
    //   E_SUBSTATE_NONE / E_SUBSTATE_WAIT_CALLBACK map to empty handlers (the X360
    //   build folds them onto a shared empty function), so they are present but do
    //   nothing.
    // -----------------------------------------------------------------------
    void PostRoundManager::InitialiseUpdateArray()
    {
        maUpdateFunctions[E_SUBSTATE_NONE]              = &PostRoundManager::UpdateNone;
        maUpdateFunctions[E_SUBSTATE_WAIT_IDLE]         = &PostRoundManager::UpdateWaitIdle;
        maUpdateFunctions[E_SUBSTATE_WAIT_CALLBACK]     = &PostRoundManager::UpdateWaitCallback;
        maUpdateFunctions[E_SUBSTATE_WAIT_GAME_UNSTART] = &PostRoundManager::UpdateWaitGameUnstart;
        maUpdateFunctions[E_SUBSTATE_WAIT_RESUME]       = &PostRoundManager::UpdateWaitResume;

        for (s32 liIndex = 0; liIndex < E_SUBSTATE_COUNT; ++liIndex)
        {
            CGS_ASSERT(maUpdateFunctions[liIndex] != nullptr,
                       "No update function supplied for this sub state");
        }
    }

    // -----------------------------------------------------------------------
    // InitialiseActionArray  (X360 @ 0x82571420)
    //   Fill the per-action handler table and the per-process ordered action lists,
    //   then assert (a) every action handler is set and (b) every process EXCEPT
    //   E_PROCESS_END_ROUND has at least one action. END_ROUND legitimately has none
    //   (it just waits for the server interface to go idle, then completes); the X360
    //   assert skips process index 0 for exactly this reason.
    // -----------------------------------------------------------------------
    void PostRoundManager::InitialiseActionArray()
    {
        for (s32 liActionIndex = 0; liActionIndex < E_ACTION_COUNT; ++liActionIndex)
        {
            maActionFunctions[liActionIndex] = nullptr;
        }

        maActionFunctions[E_ACTION_SEND_RESULTS]        = &PostRoundManager::ActionSendResults;
        maActionFunctions[E_ACTION_RESUME]              = &PostRoundManager::ActionResume;
        maActionFunctions[E_ACTION_LEAVE_GAME]          = &PostRoundManager::ActionLeaveGame;
        maActionFunctions[E_ACTION_WAIT_UNSTART_GAME]   = &PostRoundManager::ActionUnstartGame;
        maActionFunctions[E_ACTION_END_GAME]            = &PostRoundManager::ActionEndGame;
        maActionFunctions[E_ACTION_CHANGE_PLAYER_PARMS] = &PostRoundManager::ActionChangePlayerParams;
        maActionFunctions[E_ACTION_CHANGE_GAME_PARMS]   = &PostRoundManager::ActionChangeGameParams;

        for (s32 liActionIndex = 0; liActionIndex < E_ACTION_COUNT; ++liActionIndex)
        {
            CGS_ASSERT(maActionFunctions[liActionIndex] != nullptr,
                       "No update function supplied for this substate");
        }

        // Every process action slot defaults to "no action" (E_ACTION_COUNT).
        for (s32 liProcessIndex = 0; liProcessIndex < E_PROCESS_COUNT; ++liProcessIndex)
        {
            for (s32 liActionIndex = 0; liActionIndex < ProcessActions::KI_MAX_ACTIONS; ++liActionIndex)
            {
                maProcessActions[liProcessIndex].maeAction[liActionIndex] = E_ACTION_COUNT;
            }
        }

        // E_PROCESS_END_ROUND        : no actions (waits for idle, then completes).

        // E_PROCESS_END_GAME_HOST    : send results, end game, resume, wait unstart,
        //                              change game params, change player params.
        {
            EAction* lpA = maProcessActions[E_PROCESS_END_GAME_HOST].maeAction;
            lpA[0] = E_ACTION_SEND_RESULTS;
            lpA[1] = E_ACTION_END_GAME;
            lpA[2] = E_ACTION_RESUME;
            lpA[3] = E_ACTION_WAIT_UNSTART_GAME;
            lpA[4] = E_ACTION_CHANGE_GAME_PARMS;
            lpA[5] = E_ACTION_CHANGE_PLAYER_PARMS;
        }

        // E_PROCESS_END_GAME_CLIENT  : send results, end game, resume, wait unstart,
        //                              change player params.
        {
            EAction* lpA = maProcessActions[E_PROCESS_END_GAME_CLIENT].maeAction;
            lpA[0] = E_ACTION_SEND_RESULTS;
            lpA[1] = E_ACTION_END_GAME;
            lpA[2] = E_ACTION_RESUME;
            lpA[3] = E_ACTION_WAIT_UNSTART_GAME;
            lpA[4] = E_ACTION_CHANGE_PLAYER_PARMS;
        }

        // E_PROCESS_END_GAME_LEAVE_HOST : send results, end game, resume, wait unstart,
        //                                 change game params, leave game.
        {
            EAction* lpA = maProcessActions[E_PROCESS_END_GAME_LEAVE_HOST].maeAction;
            lpA[0] = E_ACTION_SEND_RESULTS;
            lpA[1] = E_ACTION_END_GAME;
            lpA[2] = E_ACTION_RESUME;
            lpA[3] = E_ACTION_WAIT_UNSTART_GAME;
            lpA[4] = E_ACTION_CHANGE_GAME_PARMS;
            lpA[5] = E_ACTION_LEAVE_GAME;
        }

        // E_PROCESS_END_GAME_LEAVE_CLIENT : send results, end game, resume, wait unstart,
        //                                   leave game.
        {
            EAction* lpA = maProcessActions[E_PROCESS_END_GAME_LEAVE_CLIENT].maeAction;
            lpA[0] = E_ACTION_SEND_RESULTS;
            lpA[1] = E_ACTION_END_GAME;
            lpA[2] = E_ACTION_RESUME;
            lpA[3] = E_ACTION_WAIT_UNSTART_GAME;
            lpA[4] = E_ACTION_LEAVE_GAME;
        }

        for (s32 liProcessIndex = 0; liProcessIndex < E_PROCESS_COUNT; ++liProcessIndex)
        {
            if (liProcessIndex != E_PROCESS_END_ROUND)
            {
                CGS_ASSERT(maProcessActions[liProcessIndex].maeAction[0] != E_ACTION_COUNT,
                           "No actions supplied for this process");
            }
        }
    }

    // -----------------------------------------------------------------------
    // Construct  (X360 @ 0x82576450)
    // -----------------------------------------------------------------------
    void PostRoundManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        InitialiseUpdateArray();
        InitialiseActionArray();

        mpNetworkManager   = lpNetworkManager;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        meSubState         = E_SUBSTATE_NONE;
        mun8CurrentAction  = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
        mbDidTheUserQuit   = false;
    }

    // -----------------------------------------------------------------------
    // Prepare  (X360 @ 0x82544D28)
    //   Reset the machine and re-Prepare the embedded result record (the virtual
    //   call through mGameResults' vtable in the asm is its inherited Prepare()).
    // -----------------------------------------------------------------------
    bool PostRoundManager::Prepare()
    {
        mun8CurrentAction  = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        meSubState         = E_SUBSTATE_NONE;
        mbDidTheUserQuit   = false;

        mGameResults.Prepare();
        return true;
    }

    // -----------------------------------------------------------------------
    // Release  (X360 @ 0x82544D80)
    // -----------------------------------------------------------------------
    bool PostRoundManager::Release()
    {
        mun8CurrentAction  = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        meSubState         = E_SUBSTATE_NONE;
        mbDidTheUserQuit   = false;
        return true;
    }

    // -----------------------------------------------------------------------
    // Destruct  (X360 @ 0x82544DB0)
    // -----------------------------------------------------------------------
    void PostRoundManager::Destruct()
    {
        mun8CurrentAction  = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        meSubState         = E_SUBSTATE_NONE;
        mbDidTheUserQuit   = false;
    }

    // -----------------------------------------------------------------------
    // Disconnected  (X360 @ 0x825451C0)
    //   The order differs from Destruct in the asm but the resulting state is the
    //   same reset; mbDidTheUserQuit is NOT touched here.
    // -----------------------------------------------------------------------
    void PostRoundManager::Disconnected()
    {
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        meSubState         = E_SUBSTATE_NONE;
        mun8CurrentAction  = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
    }

    // -----------------------------------------------------------------------
    // Update  (X360 @ 0x82544DD8)
    //   Dispatch the current substate's update handler (asserting it exists).
    // -----------------------------------------------------------------------
    void PostRoundManager::Update()
    {
        const UpdateFunction lpfUpdate = maUpdateFunctions[meSubState];
        CGS_ASSERT(lpfUpdate != nullptr,
                   "No update function supplied for the current substate");
        (this->*lpfUpdate)();
    }

    // -----------------------------------------------------------------------
    // Empty substate handlers. E_SUBSTATE_NONE and E_SUBSTATE_WAIT_CALLBACK have
    // nothing to do each frame (the X360 build folds both onto a shared empty
    // function -- 4-byte `blr` stubs at 0x...D9FC / 0x...DA00).
    // -----------------------------------------------------------------------
    void PostRoundManager::UpdateNone()
    {
    }

    void PostRoundManager::UpdateWaitCallback()
    {
    }

    // -----------------------------------------------------------------------
    // UpdateWaitIdle  (X360 @ 0x825715D0)
    //   Once the server interface reports idle, advance to the next action.
    // -----------------------------------------------------------------------
    void PostRoundManager::UpdateWaitIdle()
    {
        if (ServerInterfaceIdle())
        {
            SetNextAction();
        }
    }

    // -----------------------------------------------------------------------
    // UpdateWaitGameUnstart  (X360 @ 0x8256D338)
    //   Wait for the local player's game to leave the "started" state (the host has
    //   un-started it); when the games component reports that and the connection has
    //   gone idle, advance to the next action.
    // -----------------------------------------------------------------------
    void PostRoundManager::UpdateWaitGameUnstart()
    {
        CgsNetwork::ServerInterfaceGames* lpGamesComponent =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        if (!lpGamesComponent->IsLocalPlayerInGame() || !lpGamesComponent->IsGameStarted())
        {
            const BrnServerInterface::EStatus leStatus =
                mpNetworkManager->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_GAMES);
            if (leStatus == BrnServerInterface::E_STATUS_IDLE)
            {
                SetNextAction();
            }
        }
    }

    // -----------------------------------------------------------------------
    // UpdateWaitResume  (X360 @ 0x825508B8)
    //   While resuming the server interface after a game, just tick the suspension
    //   manager (it owns the resume sequencing and fires ResumeCompleteCallback).
    // -----------------------------------------------------------------------
    void PostRoundManager::UpdateWaitResume()
    {
        mpNetworkManager->GetSuspensionManager()->Update();
    }

    // -----------------------------------------------------------------------
    // ServerInterfaceIdle  (X360 @ 0x8256D3B8)
    //   Returns true when the server interface games component is idle and the
    //   current action did NOT error. On error it ends the process; while busy it
    //   returns false to keep waiting.
    // -----------------------------------------------------------------------
    bool PostRoundManager::ServerInterfaceIdle()
    {
        const BrnServerInterface::EStatus leStatus =
            mpNetworkManager->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_GAMES);

        switch (leStatus)
        {
        case BrnServerInterface::E_STATUS_BUSY:
            return false;

        case BrnServerInterface::E_STATUS_ERROR:
            // The action that errored is the one we just advanced past.
            if (maProcessActions[meCurrentProcess].maeAction[mun8CurrentAction - 1] != E_ACTION_SEND_RESULTS)
            {
                meSubState = E_SUBSTATE_NONE;
                ProcessComplete(false);
                return false;
            }
            mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            return true;

        case BrnServerInterface::E_STATUS_IDLE:
            return true;

        default:
            CGS_ASSERT(false,
                       "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in PostRoundManager::ServerInterfaceIdle");
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // SetNextAction  (X360 @ 0x82567610)
    //   Run the next action in the current process's list. A true return advances to
    //   the following action; a false return ends the process unsuccessfully; running
    //   off the end of the list (or hitting "no action") ends it successfully.
    // -----------------------------------------------------------------------
    void PostRoundManager::SetNextAction()
    {
        const u8 lu8Action = mun8CurrentAction;
        const EAction leAction = maProcessActions[meCurrentProcess].maeAction[lu8Action];

        bool lbSuccess;
        if (lu8Action == ProcessActions::KI_MAX_ACTIONS || leAction == E_ACTION_COUNT)
        {
            lbSuccess = true;
        }
        else
        {
            const ActionFunction lpfAction = maActionFunctions[leAction];
            if ((this->*lpfAction)())
            {
                ++mun8CurrentAction;
                return;
            }
            lbSuccess = false;
        }

        meSubState = E_SUBSTATE_NONE;
        ProcessComplete(lbSuccess);
    }

    // -----------------------------------------------------------------------
    // StartProcess  (X360 @ 0x825FC6EC)
    //   Arm the state machine for a process: record the completion callback, select
    //   the process, reset the action cursor and begin waiting for the server
    //   interface to go idle so the first action can run.
    //
    //   FLAGGED: this function's decompiled body was NOT present in the available
    //   X360 export (only its symbol/address and the inlined StrStream logging are
    //   recovered). The body below is reconstructed from the firm state-machine
    //   invariants: it is the only entry point that arms the machine, and every
    //   caller (StartEndOfRound / StartEndOfGame) hands it (process, callback,
    //   userData); the resulting (meCurrentProcess, mun8CurrentAction == 0,
    //   meSubState == WAIT_IDLE) state is what UpdateWaitIdle / SetNextAction require
    //   to run the process. No magic constants are introduced.
    // -----------------------------------------------------------------------
    void PostRoundManager::StartProcess(EProcess leProcess, Callback lpfCallback, void* lpCallbackUserData)
    {
        CGS_ASSERT(lpfCallback != nullptr, "lCallback");
        CGS_ASSERT(leProcess >= E_PROCESS_END_ROUND && leProcess < E_PROCESS_COUNT,
                   "(leProcess >= E_PROCESS_END_ROUND) && (leProcess < E_PROCESS_COUNT)");

        mCallback          = lpfCallback;
        mpCallbackUserData = lpCallbackUserData;
        meCurrentProcess   = leProcess;
        mun8CurrentAction  = 0;
        meSubState         = E_SUBSTATE_WAIT_IDLE;
    }

    // -----------------------------------------------------------------------
    // StartEndOfRound  (X360 @ 0x825FC9CC)
    //   Kick off the end-of-round process (trivial wrapper over StartProcess).
    // -----------------------------------------------------------------------
    void PostRoundManager::StartEndOfRound(Callback lpfCallback, void* lpCallbackUserData)
    {
        StartProcess(E_PROCESS_END_ROUND, lpfCallback, lpCallbackUserData);
    }

    // -----------------------------------------------------------------------
    // StartEndOfGame  (X360 @ 0x825676C8)
    //   Choose the end-of-game process by host/client and whether the local player is
    //   leaving the session, then start it. If we are no longer logged in there is
    //   nothing to negotiate -- record the callback and complete immediately.
    // -----------------------------------------------------------------------
    void PostRoundManager::StartEndOfGame(bool lbQuit, bool lbLeaveGame,
                                          Callback lpfCallback, void* lpCallbackUserData)
    {
        mbDidTheUserQuit = lbQuit;

        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr,
                   "mpNetworkManager->GetServerInterface()");
        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetConnectionComponent() != nullptr,
                   "mpNetworkManager->GetServerInterface()->GetConnectionComponent()");

        if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost())
            {
                StartProcess(lbLeaveGame ? E_PROCESS_END_GAME_LEAVE_HOST : E_PROCESS_END_GAME_HOST,
                             lpfCallback, lpCallbackUserData);
            }
            else
            {
                StartProcess(lbLeaveGame ? E_PROCESS_END_GAME_LEAVE_CLIENT : E_PROCESS_END_GAME_CLIENT,
                             lpfCallback, lpCallbackUserData);
            }
        }
        else
        {
            mCallback          = lpfCallback;
            mpCallbackUserData = lpCallbackUserData;
            ProcessComplete(true);
        }
    }

    // -----------------------------------------------------------------------
    // ProcessComplete  (X360 @ 0x82567470)
    //   Finish the current process: (optionally) re-read the local player's params
    //   while the interface is still in game, then clear and fire the stored callback.
    // -----------------------------------------------------------------------
    void PostRoundManager::ProcessComplete(bool lbSuccess)
    {
        CGS_ASSERT(mCallback != nullptr, "mCallback");

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames = lpServerInterface->GetGameComponent();
        CGS_ASSERT(lpServerInterfaceGames != nullptr, "lpServerInterfaceGames");

        CgsNetwork::NetworkPlayerID lLocalNetworkPlayerID = static_cast<CgsNetwork::NetworkPlayerID>(-1);
        const bool lbHaveLocalPlayer =
            mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);

        if (lbHaveLocalPlayer && lpServerInterfaceGames->IsLocalPlayerInGame())
        {
            PlayerParams lLocalPlayerParams;
            lLocalPlayerParams.Prepare();
            lpServerInterfaceGames->GetPlayerParametersByPlayerID(lLocalNetworkPlayerID, &lLocalPlayerParams);
        }

        const Callback lpfCallback = mCallback;
        void* const lpCallbackUserData = mpCallbackUserData;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;

        lpfCallback(lbSuccess, lpCallbackUserData);
    }

    // -----------------------------------------------------------------------
    // ResumeCompleteCallback  (X360 @ 0x82567C78)
    //   Suspension-manager completion hook for the resume action. On success advance
    //   to the next action; on failure log (omitted) and complete the process.
    // -----------------------------------------------------------------------
    void PostRoundManager::ResumeCompleteCallback(bool lbSuccess, void* lpUserData)
    {
        PostRoundManager* lpPostRoundManager = static_cast<PostRoundManager*>(lpUserData);

        if (lbSuccess)
        {
            lpPostRoundManager->SetNextAction();
            return;
        }

        CGS_ASSERT(lpPostRoundManager->mpNetworkManager != nullptr,
                   "lpPostRoundManager->mpNetworkManager");
        CGS_ASSERT(lpPostRoundManager->mpNetworkManager->GetServerInterface() != nullptr,
                   "lpPostRoundManager->mpNetworkManager->GetServerInterface()");

        BrnServerInterface* lpServerInterface =
            lpPostRoundManager->mpNetworkManager->GetServerInterface();

        // Failure diagnostics (the X360 build polls the overall interface status --
        // component index 12, the count/overall slot the suspension flow also queries --
        // and logs the last error / not-logged-in through the debug stream; the asserts
        // below preserve the contract).
        lpServerInterface->GetStatus(12);

        CGS_ASSERT(lpServerInterface->GetConnectionComponent() != nullptr,
                   "lpPostRoundManager->mpNetworkManager->GetServerInterface()->GetConnectionComponent()");

        lpPostRoundManager->meSubState = E_SUBSTATE_NONE;
        lpPostRoundManager->ProcessComplete(false);
    }

    // -----------------------------------------------------------------------
    // ProcessRaceResults  (X360 @ 0x8255EC68)
    //   Stamp the embedded result record with this round's race results plus the
    //   number of online rivals.
    // -----------------------------------------------------------------------
    void PostRoundManager::ProcessRaceResults(const GameStateModuleIO::OnlineGameResults* lpRaceResults)
    {
        mGameResults.Prepare();
        const s32 liNumberOfRivals = mpNetworkManager->GetLiveRevengeManager()->GetNumberOfRivals();
        mGameResults.SetGameStats(lpRaceResults, liNumberOfRivals);
    }

    // ===================================================================
    //  Actions. Each returns true to advance to the next action in the
    //  process, false to fail the process. (The X360 debug logging that
    //  opens every action is omitted -- see file header.)
    // ===================================================================

    // ActionSendResults  (X360 @ 0x82545040)
    bool PostRoundManager::ActionSendResults()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames = lpServerInterface->GetGameComponent();
        CGS_ASSERT(lpServerInterfaceGames != nullptr, "lpServerInterfaceGames");

        lpServerInterfaceGames->SendGameResult(&mGameResults, nullptr);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    // ActionResume  (X360 @ 0x825FD24C)
    //   FLAGGED: this function's decompiled body was NOT in the available X360 export
    //   (func_files span_count == 5, dominant/only-inlined file == CgsStrStream.h),
    //   i.e. the recovered footprint is purely the "Starting action" debug logging
    //   followed by `return true`. The resume sequencing proper is owned by the
    //   suspension manager (driven via the WAIT_RESUME substate / ResumeCompleteCallback).
    //   Returning true (advance) without an un-groundable substate store is reproduced
    //   here; no behaviour is invented.
    bool PostRoundManager::ActionResume()
    {
        return true;
    }

    // ActionLeaveGame  (X360 @ 0x82544F78)
    bool PostRoundManager::ActionLeaveGame()
    {
        meSubState = E_SUBSTATE_WAIT_IDLE;

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();
        if (lpServerInterfaceGames->IsLocalPlayerInGame())
        {
            lpServerInterfaceGames->LeaveGame(0, 1);
        }
        return true;
    }

    // ActionUnstartGame  (X360 @ 0x825FDACC)
    //   FLAGGED: as with ActionResume, the decompiled body was NOT in the available
    //   X360 export (func_files span_count == 5, only-inlined file == CgsStrStream.h):
    //   the recovered footprint is the debug logging + `return true`. The actual
    //   un-start wait is handled by the WAIT_GAME_UNSTART substate. Reproduced as
    //   return true; no behaviour is invented.
    bool PostRoundManager::ActionUnstartGame()
    {
        return true;
    }

    // ActionEndGame  (X360 @ 0x82567B18)
    //   Build the end-game data record from every finalised player's params, then
    //   submit it. The X360 walks the player registry, fetches each player's lobby
    //   params and appends a {score, playerId} pair into the end-game data payload
    //   before SendGameResult. The exact per-player score field offset inside the
    //   fetched params and the packed destination layout are not exposed by the
    //   reconstructed types, so the (opaque-payload) packing is intentionally left to
    //   the not-yet-homed end-game-data TU; the observable server traffic -- the
    //   per-player param fetch and the final SendGameResult -- is reproduced here.
    bool PostRoundManager::ActionEndGame()
    {
        CgsNetwork::ServerInterfaceEndGameData lEndGameData;
        lEndGameData.Prepare();

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        CgsNetwork::NetworkPlayerID lPlayerID = static_cast<CgsNetwork::NetworkPlayerID>(-1);
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID,
                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerParams lPlayerParams;
            lPlayerParams.Prepare();
            lpServerInterfaceGames->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
        }

        meSubState = E_SUBSTATE_WAIT_IDLE;
        mpNetworkManager->GetServerInterface()->GetGameComponent()->SendGameResult(&lEndGameData, nullptr);
        return true;
    }

    // ActionChangePlayerParams  (X360 @ 0x82567810)
    //   Re-push the local player's params with the "playing" flag cleared at game end.
    bool PostRoundManager::ActionChangePlayerParams()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames = lpServerInterface->GetGameComponent();
        CGS_ASSERT(lpServerInterfaceGames != nullptr, "lpServerInterfaceGames");

        if (lpServerInterfaceGames->IsLocalPlayerInGame())
        {
            CgsNetwork::NetworkPlayerID lLocalNetworkPlayerID = static_cast<CgsNetwork::NetworkPlayerID>(-1);
            if (mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID))
            {
                PlayerParams lLocalPlayerParams;
                lLocalPlayerParams.Prepare();
                lpServerInterfaceGames->GetPlayerParametersByPlayerID(lLocalNetworkPlayerID, &lLocalPlayerParams);
                lLocalPlayerParams.SetPlaying(false);
                lpServerInterfaceGames->UpdatePlayerParameters(lLocalNetworkPlayerID, &lLocalPlayerParams);
            }
        }

        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    // ActionChangeGameParams  (X360 @ 0x82567978)
    //   The host re-pushes the game params with the mode forced to the post-game
    //   lobby mode (15), stashing the previous mode first.
    bool PostRoundManager::ActionChangeGameParams()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        CGS_ASSERT(lpServerInterface != nullptr, "lpServerInterface");

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames = lpServerInterface->GetGameComponent();
        CGS_ASSERT(lpServerInterfaceGames != nullptr, "lpServerInterfaceGames");

        if (lpServerInterfaceGames->IsLocalPlayerInGame())
        {
            GameParams lGameParams;
            lGameParams.Prepare();
            lpServerInterfaceGames->GetGameParameters(&lGameParams);

            const s32 liGameMode = lGameParams.GameMode();
            lGameParams.SetPreviousGameMode(liGameMode);
            lGameParams.SetGameMode(15);

            lpServerInterfaceGames->UpdateGameParameters(&lGameParams);
        }

        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }
}
