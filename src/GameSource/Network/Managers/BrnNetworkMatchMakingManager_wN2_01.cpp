// BrnNetwork::MatchMakingManager -- lifecycle, request entry points and the process/action
// state machine.
//
// A request (create / join / quick-join / leave / search) starts a "process": a fixed list of up
// to four actions run one after another. Each action kicks a server-interface component and moves
// the manager into a wait sub-state; the per-frame Update runs that sub-state's handler, which
// waits for the component used by the last action (and the one the next action needs) to go idle
// before starting the next action. When the list is exhausted, or an action or its component
// fails, the caller's completion callback fires.

#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT, CgsDev::Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                            // CgsDev::StrStream (StartProcess assert)
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                    // PlayerManager::GetTotalNumberPlayers
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"    // E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"     // ServerInterfaceGames
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"  // ServerInterfaceUsersets
#include "GameSource/Network/BrnNetworkManager.h"                                        // GetServerInterface / GetPlayerManager / GetTime
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"      // SetSessionFlags
#include "GameSource/Network/Debug Components/BrnNetworkServerInterfaceDebugComponent.h"   // Set/GetConnectionType
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                  // PlayerParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                     // PlayerInfoData
#include "GameSource/Network/Parameters/BrnNetworkQuickJoinParams.h"                    // QuickJoinParams
#include "GameSource/Network/Parameters/BrnNetworkUsersetParams.h"                      // UsersetParams
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetLocalPlayerInfo

namespace BrnNetwork
{
    // Minimum time between the searches of one search process.
    const f32 KF_SEARCH_INTERVAL = 1.0f;

    // -------------------------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------------------------

    void MatchMakingManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        InitialiseUpdateArray();
        InitialiseActionArray();

        mpNetworkManager    = lpNetworkManager;
        mbQuickJoinRanked   = false;
        mbQuickJoinFreeburn = false;
        mLastSearchTime.SetFloatVal(0.0f);
        mun8NextAction      = 0;
        mCallback           = nullptr;
        mpCallbackUserData  = nullptr;
        meSubState          = E_SUBSTATE_NONE;
        meCurrentProcess    = E_PROCESS_COUNT;
    }

    bool MatchMakingManager::Release()
    {
        meSubState          = E_SUBSTATE_NONE;
        mbQuickJoinRanked   = false;
        mbQuickJoinFreeburn = false;
        mLastSearchTime.SetFloatVal(0.0f);
        mun8NextAction      = 0;
        mCallback           = nullptr;
        mpCallbackUserData  = nullptr;
        meCurrentProcess    = E_PROCESS_COUNT;
        return true;
    }

    void MatchMakingManager::Destruct()
    {
        meSubState          = E_SUBSTATE_NONE;
        mbQuickJoinRanked   = false;
        mbQuickJoinFreeburn = false;
        mLastSearchTime.SetFloatVal(0.0f);
        mun8NextAction      = 0;
        mCallback           = nullptr;
        mpCallbackUserData  = nullptr;
        meCurrentProcess    = E_PROCESS_COUNT;
    }

    // The connection is gone: drop the running process without calling back.
    void MatchMakingManager::Disconnected()
    {
        meSubState         = E_SUBSTATE_NONE;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;
        mLastSearchTime.SetFloatVal(0.0f);
        mun8NextAction     = 0;
        meCurrentProcess   = E_PROCESS_COUNT;
    }

    // Run the current sub-state's handler.
    void MatchMakingManager::Update()
    {
        CGS_ASSERT(maUpdateFunctions[meSubState] != nullptr, "No update function supplied for the current substate");
        (this->*maUpdateFunctions[meSubState])();
    }

    // -------------------------------------------------------------------------------------------
    // Tables
    // -------------------------------------------------------------------------------------------

    void MatchMakingManager::InitialiseUpdateArray()
    {
        for (s32 liIndex = 0; liIndex < E_SUBSTATE_COUNT; ++liIndex)
        {
            maUpdateFunctions[liIndex] = nullptr;
        }

        maUpdateFunctions[E_SUBSTATE_NONE]           = &MatchMakingManager::UpdateNone;
        maUpdateFunctions[E_SUBSTATE_WAIT_IDLE]      = &MatchMakingManager::UpdateWaitIdle;
        maUpdateFunctions[E_SUBSTATE_WAIT_TO_SEARCH] = &MatchMakingManager::UpdateWaitToSearch;
        maUpdateFunctions[E_SUBSTATE_WAIT_IN_GAME]   = &MatchMakingManager::UpdateWaitInGame;

        for (s32 liIndex = 0; liIndex < E_SUBSTATE_COUNT; ++liIndex)
        {
            CGS_ASSERT(maUpdateFunctions[liIndex] != nullptr, "No update function supplied for this substate");
        }
    }

    // The action functions, the server-interface component each action waits on, and the ordered
    // action list of every process.
    void MatchMakingManager::InitialiseActionArray()
    {
        for (s32 liActionIndex = 0; liActionIndex < E_ACTION_COUNT; ++liActionIndex)
        {
            maActionFunctions[liActionIndex] = nullptr;
            maeComponentToUse[liActionIndex] = CgsNetwork::E_COMPONENTS_COUNT;
        }

        maActionFunctions[E_ACTION_LOCK_USERSET]          = &MatchMakingManager::ActionLockUserset;
        maActionFunctions[E_ACTION_CREATE_GAME]           = &MatchMakingManager::ActionCreateGame;
        maActionFunctions[E_ACTION_JOIN_GAME]             = &MatchMakingManager::ActionJoinGame;
        maActionFunctions[E_ACTION_LEAVE_GAME]            = &MatchMakingManager::ActionLeaveGame;
        maActionFunctions[E_ACTION_QUICK_JOIN_GAME]       = &MatchMakingManager::ActionQuickJoinGame;
        maActionFunctions[E_ACTION_SEARCH_FOR_GAMES]      = &MatchMakingManager::ActionSearchForGames;
        maActionFunctions[E_ACTION_WAIT_SEARCH_FOR_GAMES] = &MatchMakingManager::ActionWaitSearchForGames;
        maActionFunctions[E_ACTION_UNLOCK_USERSET]        = &MatchMakingManager::ActionUnLockUserset;
        maActionFunctions[E_ACTION_CREATE_USERSET]        = &MatchMakingManager::ActionCreateUserset;
        maActionFunctions[E_ACTION_JOIN_USERSET]          = &MatchMakingManager::ActionJoinUserset;
        maActionFunctions[E_ACTION_LEAVE_USERSET]         = &MatchMakingManager::ActionLeaveUserset;
        maActionFunctions[E_ACTION_WAIT_IN_GAME]          = &MatchMakingManager::ActionWaitInGame;

        maeComponentToUse[E_ACTION_LOCK_USERSET]          = CgsNetwork::E_COMPONENTS_USERSETS;
        maeComponentToUse[E_ACTION_CREATE_GAME]           = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_JOIN_GAME]             = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_LEAVE_GAME]            = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_QUICK_JOIN_GAME]       = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_SEARCH_FOR_GAMES]      = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_WAIT_SEARCH_FOR_GAMES] = CgsNetwork::E_COMPONENTS_GAMES;
        maeComponentToUse[E_ACTION_UNLOCK_USERSET]        = CgsNetwork::E_COMPONENTS_USERSETS;
        maeComponentToUse[E_ACTION_CREATE_USERSET]        = CgsNetwork::E_COMPONENTS_USERSETS;
        maeComponentToUse[E_ACTION_JOIN_USERSET]          = CgsNetwork::E_COMPONENTS_USERSETS;
        maeComponentToUse[E_ACTION_LEAVE_USERSET]         = CgsNetwork::E_COMPONENTS_USERSETS;
        maeComponentToUse[E_ACTION_WAIT_IN_GAME]          = CgsNetwork::E_COMPONENTS_GAMES;

        for (s32 liActionIndex = 0; liActionIndex < E_ACTION_COUNT; ++liActionIndex)
        {
            CGS_ASSERT(maActionFunctions[liActionIndex] != nullptr, "No function supplied for this action");
            CGS_ASSERT(maeComponentToUse[liActionIndex] != CgsNetwork::E_COMPONENTS_COUNT,
                       "No component to use supplied for this action");
        }

        for (s32 liProcessIndex = 0; liProcessIndex < E_PROCESS_COUNT; ++liProcessIndex)
        {
            for (s32 liActionIndex = 0; liActionIndex < ProcessActions::KI_MAX_ACTIONS; ++liActionIndex)
            {
                maProcessActions[liProcessIndex].maeAction[liActionIndex] = E_ACTION_COUNT;
            }
        }

        maProcessActions[E_PROCESS_CREATE_GAME].maeAction[0]               = E_ACTION_CREATE_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_CREATE_GAME].maeAction[0]     = E_ACTION_LEAVE_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_CREATE_GAME].maeAction[1]     = E_ACTION_CREATE_GAME;
        maProcessActions[E_PROCESS_JOIN_GAME].maeAction[0]                 = E_ACTION_JOIN_GAME;
        maProcessActions[E_PROCESS_LEAVE_GAME].maeAction[0]                = E_ACTION_LEAVE_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_JOIN_GAME].maeAction[0]       = E_ACTION_LEAVE_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_JOIN_GAME].maeAction[1]       = E_ACTION_JOIN_GAME;
        maProcessActions[E_PROCESS_QUICK_JOIN_GAME].maeAction[0]           = E_ACTION_QUICK_JOIN_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_QUICK_JOIN_GAME].maeAction[0] = E_ACTION_LEAVE_GAME;
        maProcessActions[E_PROCESS_LEAVE_AND_QUICK_JOIN_GAME].maeAction[1] = E_ACTION_QUICK_JOIN_GAME;
        maProcessActions[E_PROCESS_SEARCH_FOR_GAMES].maeAction[0]          = E_ACTION_WAIT_SEARCH_FOR_GAMES;
        maProcessActions[E_PROCESS_SEARCH_FOR_GAMES].maeAction[1]          = E_ACTION_SEARCH_FOR_GAMES;

        for (s32 liProcessIndex = 0; liProcessIndex < E_PROCESS_COUNT; ++liProcessIndex)
        {
            CGS_ASSERT(maProcessActions[liProcessIndex].maeAction[0] != E_ACTION_COUNT,
                       "No actions supplied for this process");
        }
    }

    // -------------------------------------------------------------------------------------------
    // Requests
    // -------------------------------------------------------------------------------------------

    void MatchMakingManager::CreateGame(const GameParams* lpParams, Callback lCallback, void* lpCallbackUserData)
    {
        mGameParameters        = *lpParams;
        mbCreateGameServerGame = true;

        const bool lbInGame = mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();
        StartProcess(lbInGame ? E_PROCESS_LEAVE_AND_CREATE_GAME : E_PROCESS_CREATE_GAME,
                     lCallback, lpCallbackUserData);
    }

    // The invite path asks the game component whether we are in a game first; when that says no,
    // the common test below asks again.
    void MatchMakingManager::JoinGame(const GameParams* lpParams, bool lbPerformingInvite,
                                      Callback lCallback, void* lpCallbackUserData)
    {
        mGameParameters = *lpParams;

        EProcess leProcess;
        if (lbPerformingInvite && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            leProcess = E_PROCESS_LEAVE_AND_JOIN_GAME;
        }
        else if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            leProcess = E_PROCESS_LEAVE_AND_JOIN_GAME;
        }
        else
        {
            leProcess = E_PROCESS_JOIN_GAME;
        }

        StartProcess(leProcess, lCallback, lpCallbackUserData);
    }

    void MatchMakingManager::QuickJoinGame(bool lbRanked, bool lbFreeburn, Callback lCallback, void* lpCallbackUserData)
    {
        mbQuickJoinRanked   = lbRanked;
        mbQuickJoinFreeburn = lbFreeburn;

        const bool lbInGame = mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();
        StartProcess(lbInGame ? E_PROCESS_LEAVE_AND_QUICK_JOIN_GAME : E_PROCESS_QUICK_JOIN_GAME,
                     lCallback, lpCallbackUserData);
    }

    // -------------------------------------------------------------------------------------------
    // Process state machine
    // -------------------------------------------------------------------------------------------

    void MatchMakingManager::StartProcess(EProcess leProcess, Callback lCallback, void* lpCallbackUserData)
    {
        CGS_ASSERT(lCallback != nullptr, "lCallback");
        if (mCallback != nullptr)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "meCurrentProcess=" << static_cast<s32>(meCurrentProcess)
                       << " leProcess=" << static_cast<s32>(leProcess);
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        mCallback          = lCallback;
        mpCallbackUserData = lpCallbackUserData;
        meCurrentProcess   = leProcess;
        mun8NextAction     = 0;
        meSubState         = E_SUBSTATE_WAIT_IDLE;
    }

    // Hand the result to the caller's callback. The callback is cleared before it runs, so it may
    // start a new process.
    void MatchMakingManager::ProcessComplete(bool lbSuccess)
    {
        CGS_ASSERT(mCallback != nullptr, "mCallback");

        Callback lCallback          = mCallback;
        void*    lpCallbackUserData = mpCallbackUserData;
        mCallback          = nullptr;
        mpCallbackUserData = nullptr;

        lCallback(lbSuccess, lpCallbackUserData);
    }

    // Start the process's next action. The process succeeds once its list is exhausted and fails
    // when an action refuses to start.
    void MatchMakingManager::SetNextAction()
    {
        bool lbSuccess;
        if (mun8NextAction == ProcessActions::KI_MAX_ACTIONS
            || maProcessActions[meCurrentProcess].maeAction[mun8NextAction] == E_ACTION_COUNT)
        {
            lbSuccess = true;
        }
        else
        {
            const EAction leNextAction = maProcessActions[meCurrentProcess].maeAction[mun8NextAction];
            if ((this->*maActionFunctions[leNextAction])())
            {
                ++mun8NextAction;
                return;
            }
            lbSuccess = false;
        }

        meSubState = E_SUBSTATE_NONE;
        ProcessComplete(lbSuccess);
    }

    // True once the component used by the last action, and the one the next action will use, are
    // idle. A component error ends the process, except that a failed create with a game server is
    // retried once as a plain create.
    bool MatchMakingManager::ServerInterfaceIdle()
    {
        CgsNetwork::ServerInterfaceDirtySock::EStatus leStatus = CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE;
        if (mun8NextAction != 0)
        {
            const EAction leLastAction = maProcessActions[meCurrentProcess].maeAction[mun8NextAction - 1];
            leStatus = mpNetworkManager->GetServerInterface()->GetStatus(maeComponentToUse[leLastAction]);
        }

        switch (leStatus)
        {
        case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_BUSY:
            break;

        case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR:
            if ((meCurrentProcess == E_PROCESS_CREATE_GAME || meCurrentProcess == E_PROCESS_LEAVE_AND_CREATE_GAME)
                && mpNetworkManager->GetServerInterface()->GetAndClearLastError(CgsNetwork::E_COMPONENTS_GAMES)
                       == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_NO_GAMES_FOUND
                && mbCreateGameServerGame)
            {
                Callback lCallback          = mCallback;
                void*    lpCallbackUserData = mpCallbackUserData;
                mCallback              = nullptr;
                mpCallbackUserData     = nullptr;
                mbCreateGameServerGame = false;
                StartProcess(E_PROCESS_CREATE_GAME, lCallback, lpCallbackUserData);
            }
            else
            {
                meSubState = E_SUBSTATE_NONE;
                ProcessComplete(false);
            }
            break;

        case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE:
        {
            if (mun8NextAction == ProcessActions::KI_MAX_ACTIONS)
            {
                return true;
            }
            const EAction leNextAction = maProcessActions[meCurrentProcess].maeAction[mun8NextAction];
            if (leNextAction == E_ACTION_COUNT
                || mpNetworkManager->GetServerInterface()->GetStatus(maeComponentToUse[leNextAction])
                       == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                return true;
            }
            break;
        }

        default:
            CGS_ASSERT(false, "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in MatchMakingManager::ServerInterfaceIdle");
            break;
        }

        return false;
    }

    // Registered as the game component's search-result sort callback: the results keep the
    // server's order.
    s32 MatchMakingManager::SortGameSearchResultsCallback(void* /*lpUserData*/,
                                                          CgsNetwork::ServerInterfaceGameParamsBase* /*lpGameA*/,
                                                          CgsNetwork::ServerInterfaceGameParamsBase* /*lpGameB*/)
    {
        return 0;
    }

    // -------------------------------------------------------------------------------------------
    // Sub-state handlers
    // -------------------------------------------------------------------------------------------

    void MatchMakingManager::UpdateNone()
    {
    }

    void MatchMakingManager::UpdateWaitIdle()
    {
        if (ServerInterfaceIdle())
        {
            SetNextAction();
        }
    }

    // Space the searches of a search process at least KF_SEARCH_INTERVAL apart; the first search
    // (no search time stamped yet) runs straight away.
    void MatchMakingManager::UpdateWaitToSearch()
    {
        if ((mpNetworkManager->GetTime() - mLastSearchTime) >= CgsSystem::Time(KF_SEARCH_INTERVAL)
            || mLastSearchTime <= CgsSystem::Time(0.0f))
        {
            if (ServerInterfaceIdle())
            {
                SetNextAction();
            }
        }
    }

    // Wait until the game has players in it.
    void MatchMakingManager::UpdateWaitInGame()
    {
        if (mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS) > 0
            && ServerInterfaceIdle())
        {
            SetNextAction();
        }
    }

    // -------------------------------------------------------------------------------------------
    // Actions
    // -------------------------------------------------------------------------------------------

    bool MatchMakingManager::ActionLeaveGame()
    {
        mpNetworkManager->GetServerInterface()->GetGameComponent()->LeaveGame(false, true);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    // Stamp the search time, then hand the stored search parameters to the game component.
    bool MatchMakingManager::ActionSearchForGames()
    {
        mLastSearchTime = mpNetworkManager->GetTime();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->SearchForGames(&mGameSearchParameters);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    bool MatchMakingManager::ActionWaitSearchForGames()
    {
        meSubState = E_SUBSTATE_WAIT_TO_SEARCH;
        return true;
    }

    bool MatchMakingManager::ActionLeaveUserset()
    {
        static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent())->LeaveUserset();
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    bool MatchMakingManager::ActionWaitInGame()
    {
        meSubState = E_SUBSTATE_WAIT_IN_GAME;
        return true;
    }

    // -------------------------------------------------------------------------------------------
    // Prepare and the parameter-building actions
    // -------------------------------------------------------------------------------------------

    // The game-server connection type Prepare hands the server interface's debug component.
    const CgsNetwork::ServerInterfaceGames::EGameServerConnectionType KE_GAME_SERVER_CONNECTION_TYPE =
        CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY;

    // Paint finishes a lobby player may carry (the create / join / quick-join assert).
    const u16 KU16_NUM_PAINT_FINISHES = 4;

    // A free-burn car at least this deformed is advertised as deformed.
    const f32 KF_CAR_DEFORMED_THRESHOLD = 0.85f;

    // Reset the requests' state, seed the parameter blocks (the search block asks for any game
    // waiting for players), register the search-result sort callback and set the connection type.
    bool MatchMakingManager::Prepare()
    {
        meSubState = E_SUBSTATE_NONE;
        mGameParameters.Prepare();
        mGameSearchParameters.Prepare(0, E_GAMESTATE_WAITING_FOR_PLAYERS, 0, 0, false, false, 1,
                                      CgsNetwork::E_FIREWALL_OPEN, mpNetworkManager, 2);
        mbQuickJoinRanked   = false;
        mbQuickJoinFreeburn = false;
        mLastSearchTime.SetFloatVal(0.0f);
        mun8NextAction      = 0;
        mCallback           = nullptr;
        mpCallbackUserData  = nullptr;
        meCurrentProcess    = E_PROCESS_COUNT;
        mFoundGameA.Prepare();
        mFoundGameB.Prepare();

        mpNetworkManager->GetServerInterface()->GetGameComponent()->RegisterGameSearchSortCallback(
            &SortGameSearchResultsCallback, this, &mFoundGameA, &mFoundGameB);
        mpNetworkManager->GetServerInterface()->GetDebugComponent()->SetConnectionType(KE_GAME_SERVER_CONNECTION_TYPE);
        return true;
    }

    // Lock the userset we host so nobody else joins it.
    bool MatchMakingManager::ActionLockUserset()
    {
        CgsNetwork::ServerInterfaceUsersets* lpUsersets = static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent());
        if (lpUsersets->IsLocalPlayerInUserset() && lpUsersets->IsLocalPlayerHost())
        {
            UsersetParams lParams;
            lParams.Prepare();
            lpUsersets->GetUserSetParams(&lParams);
            lParams.Lock();
            lpUsersets->UpdateUserSetParams(&lParams);
            meSubState = E_SUBSTATE_WAIT_IDLE;
        }
        return true;
    }

    bool MatchMakingManager::ActionUnLockUserset()
    {
        CgsNetwork::ServerInterfaceUsersets* lpUsersets = static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent());
        if (lpUsersets->IsLocalPlayerInUserset() && lpUsersets->IsLocalPlayerHost())
        {
            CGS_ASSERT(lpUsersets->IsLocalPlayerHost(),
                       "mpNetworkManager->GetServerInterface()->GetUsersetsComponent()->IsLocalPlayerHost()");

            UsersetParams lParams;
            lParams.Prepare();
            lpUsersets->GetUserSetParams(&lParams);
            if (lParams.IsLocked())
            {
                lParams.Unlock();
                lpUsersets->UpdateUserSetParams(&lParams);
            }
            meSubState = E_SUBSTATE_WAIT_IDLE;
        }
        return true;
    }

    // Create a userset named after the game we are in, sized to its player count.
    bool MatchMakingManager::ActionCreateUserset()
    {
        UsersetParams lUsersetParams;
        GameParams    lGameParams;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
        lUsersetParams.Prepare();
        lUsersetParams.SetName(lGameParams.GetName());
        lUsersetParams.SetMaxPlayers(lGameParams.GetMaxPlayers());
        static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent())->CreateUserset(&lUsersetParams);

        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    // Join the userset named after the game we are in.
    bool MatchMakingManager::ActionJoinUserset()
    {
        UsersetParams lUsersetParams;
        GameParams    lGameParams;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
        lUsersetParams.Prepare();
        lUsersetParams.SetName(lGameParams.GetName());
        static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent())->JoinUserset(&lUsersetParams);

        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    // The create / join / quick-join actions build the local player's lobby parameters the same
    // way: rank from the player info, then the free-burn car, its deformation, colour and paint
    // finish, fever and developer flags from the network manager.

    bool MatchMakingManager::ActionCreateGame()
    {
        PlayerParams   lPlayerParams;
        PlayerInfoData lPlayerInfo;

        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        lPlayerParams.Prepare();
        lPlayerParams.SetConsoleFrameRate(mpNetworkManager->GetLocalConsoleFrameRate());
        lPlayerParams.SetRank(lPlayerInfo.GetRank());
        lPlayerParams.SetFreeBurnCarID(mpNetworkManager->GetFreeBurnCarID());
        lPlayerParams.SetIsCarDeformed(!(mpNetworkManager->GetFreeBurnCarDeformation() < KF_CAR_DEFORMED_THRESHOLD));
        lPlayerParams.SetCarColourIndex(mpNetworkManager->GetCurrentCarColourIndex());
        lPlayerParams.SetPaintFinishIndex(mpNetworkManager->GetCurrentPaintFinishIndex());
        lPlayerParams.SetHasFever(mpNetworkManager->HasFever());
        lPlayerParams.SetIsDeveloper(mpNetworkManager->IsDeveloper());

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        if (mbCreateGameServerGame)
        {
            // One extra slot for the game server.
            mGameParameters.SetMaxPlayers(mGameParameters.GetMaxPlayers() + 1);
            mGameParameters.SetTotalSlots(mGameParameters.GetMaxPlayers(), 0);
            lpGames->SetGameServerConnectionType(
                mpNetworkManager->GetServerInterface()->GetDebugComponent()->GetConnectionType());
        }
        else
        {
            lpGames->SetGameServerConnectionType(CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_NONE);
            mGameParameters.SetMaxPlayers(6);
            mGameParameters.SetTotalSlots(6, 0);
        }

        // A closed game (security 2) takes the extra session flag.
        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(lpGames)->SetSessionFlags(
            mGameParameters.Security() == 2 ? 0x600 : 0x400);

        if (!(lPlayerParams.GetPaintFinishIndex() < KU16_NUM_PAINT_FINISHES))
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid Paint Finish: " << static_cast<s32>(lPlayerParams.GetPaintFinishIndex()) << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        lpGames->CreateGame(&mGameParameters, &lPlayerParams);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    bool MatchMakingManager::ActionJoinGame()
    {
        PlayerParams   lPlayerParams;
        PlayerInfoData lPlayerInfo;

        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        lPlayerParams.Prepare();
        lPlayerParams.SetConsoleFrameRate(mpNetworkManager->GetLocalConsoleFrameRate());
        lPlayerParams.SetRank(lPlayerInfo.GetRank());
        lPlayerParams.SetFreeBurnCarID(mpNetworkManager->GetFreeBurnCarID());
        lPlayerParams.SetIsCarDeformed(!(mpNetworkManager->GetFreeBurnCarDeformation() < KF_CAR_DEFORMED_THRESHOLD));
        lPlayerParams.SetCarColourIndex(mpNetworkManager->GetCurrentCarColourIndex());
        lPlayerParams.SetPaintFinishIndex(mpNetworkManager->GetCurrentPaintFinishIndex());
        lPlayerParams.SetHasFever(mpNetworkManager->HasFever());
        lPlayerParams.SetIsDeveloper(mpNetworkManager->IsDeveloper());

        mGameParameters.SetJoinUserset(static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent())->IsLocalPlayerInUserset());

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        lpGames->SetGameServerConnectionType(
            mpNetworkManager->GetServerInterface()->GetDebugComponent()->GetConnectionType());

        if (!(lPlayerParams.GetPaintFinishIndex() < KU16_NUM_PAINT_FINISHES))
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid Paint Finish: " << static_cast<s32>(lPlayerParams.GetPaintFinishIndex()) << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        lpGames->JoinGame(&mGameParameters, &lPlayerParams);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }

    bool MatchMakingManager::ActionQuickJoinGame()
    {
        QuickJoinParams lQuickJoinParams;
        PlayerParams    lPlayerParams;
        PlayerInfoData  lPlayerInfo;

        lPlayerParams.Prepare();
        lQuickJoinParams.Prepare();
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

        lQuickJoinParams.SetMatchmakingParameter(E_QUICKJOIN_SKILL_LEVEL, lPlayerInfo.GetRank());
        lQuickJoinParams.SetMatchmakingParameter(E_QUICKJOIN_FIREWALL_SETTING,
                                                 static_cast<s32>(lPlayerParams.GetFirewallSettings()));
        lQuickJoinParams.SetMatchmakingParameter(E_QUICKJOIN_RANKED, mbQuickJoinRanked);
        lQuickJoinParams.SetMatchmakingParameter(E_QUICKJOIN_FREEBURN, mbQuickJoinFreeburn);
        lQuickJoinParams.SetMatchmakingParameter(E_QUICKJOIN_PARAMETER_4, static_cast<s32>(2));
        lQuickJoinParams.SetJoinUserset(static_cast<CgsNetwork::ServerInterfaceUsersets*>(
            mpNetworkManager->GetServerInterface()->GetUsersetsComponent())->IsLocalPlayerInUserset());

        lPlayerParams.SetConsoleFrameRate(mpNetworkManager->GetLocalConsoleFrameRate());
        lPlayerParams.SetRank(lPlayerInfo.GetRank());
        lPlayerParams.SetFreeBurnCarID(mpNetworkManager->GetFreeBurnCarID());
        lPlayerParams.SetIsCarDeformed(!(mpNetworkManager->GetFreeBurnCarDeformation() < KF_CAR_DEFORMED_THRESHOLD));
        lPlayerParams.SetCarColourIndex(mpNetworkManager->GetCurrentCarColourIndex());
        lPlayerParams.SetPaintFinishIndex(mpNetworkManager->GetCurrentPaintFinishIndex());
        lPlayerParams.SetHasFever(mpNetworkManager->HasFever());
        lPlayerParams.SetIsDeveloper(mpNetworkManager->IsDeveloper());

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        lpGames->SetGameServerConnectionType(
            mpNetworkManager->GetServerInterface()->GetDebugComponent()->GetConnectionType());

        if (!(lPlayerParams.GetPaintFinishIndex() < KU16_NUM_PAINT_FINISHES))
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid Paint Finish: " << static_cast<s32>(lPlayerParams.GetPaintFinishIndex()) << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        lpGames->QuickJoinGame(&lQuickJoinParams, &lPlayerParams);
        meSubState = E_SUBSTATE_WAIT_IDLE;
        return true;
    }
}
