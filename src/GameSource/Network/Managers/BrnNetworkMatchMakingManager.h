#ifndef BRN_NETWORK_MATCH_MAKING_MANAGER_H
#define BRN_NETWORK_MATCH_MAKING_MANAGER_H

#include <cstddef>                                                                  // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                           // CgsSystem::Time
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // CgsNetwork::EComponents
#include "GameSource/Network/BrnNetworkGameParams.h"                               // BrnNetwork::GameParams

// ===========================================================================
// BrnNetwork::MatchMakingManager
//   Home: GameSource/Network/Managers/BrnNetworkMatchMakingManager.{h,cpp}
//
// The online create / join / quick-join / leave / search state machine. A started
// "process" runs a fixed list of up to four "actions" (lock/create/join/leave
// user-sets and games, search, wait) through a table of member-function pointers,
// waiting on the server interface to go idle between steps, then fires the
// caller's completion callback.
//
// Member names, types and order follow the reference declaration; the console offsets
// below are read from Construct / InitialiseUpdateArray / InitialiseActionArray /
// Prepare / Release / StartProcess / QuickJoinGame / JoinGame:
//   +0x000  maProcessActions[8]      (4 actions each)
//   +0x080  maUpdateFunctions[4]     (member-function pointers, 4 bytes each)
//   +0x090  maActionFunctions[12]
//   +0x0C0  maeComponentToUse[12]
//   +0x0F0  meSubState
//   +0x0F4  mpNetworkManager
//   +0x0F8  mCallback
//   +0x0FC  mpCallbackUserData
//   +0x100  mGameParameters          (GameParams, 0x5D0)
//   +0x6D0  mGameSearchParameters    (GameSearchParams, 0x390)
//   +0xA60  mFoundGameA
//   +0x1030 mFoundGameB
//   +0x1600 mbQuickJoinRanked, +0x1601 mbQuickJoinFreeburn
//   +0x1604 mLastSearchTime
//   +0x160C meCurrentProcess         (E_PROCESS_COUNT when idle)
//   +0x1610 mun8NextAction, +0x1611 mbCreateGameServerGame
//   sizeof 0x1614
//
// The constructor is the compiler-generated one: the console body only runs the
// member constructors (the three GameParams, the GameSearchParams and the Time).
//
// FLAG: mGameSearchParameters is held as console-sized storage. Its type,
// BrnNetwork::GameSearchParams (Parameters/BrnNetworkGameSearchParams.h), is still
// abstract (GetCustomFlagsMask / GetCustomFlagsValue are not overridden) and 4 bytes
// larger than its 0x390-byte console span, so it cannot be embedded by value yet.
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkManager;
    class GameSearchParams;     // SearchForGames param; see the FLAG above for the member

    class MatchMakingManager
    {
    public:
        // Completion callback fired when the started process finishes.
        typedef void (*Callback)(bool lbSuccess, void* lpUserData);

        enum ESubState
        {
            E_SUBSTATE_NONE          = 0,
            E_SUBSTATE_WAIT_IDLE     = 1,
            E_SUBSTATE_WAIT_TO_SEARCH = 2,
            E_SUBSTATE_WAIT_IN_GAME  = 3,
            E_SUBSTATE_COUNT         = 4,
        };

        enum EProcess
        {
            E_PROCESS_CREATE_GAME               = 0,
            E_PROCESS_LEAVE_AND_CREATE_GAME     = 1,
            E_PROCESS_JOIN_GAME                 = 2,
            E_PROCESS_LEAVE_GAME                = 3,
            E_PROCESS_LEAVE_AND_JOIN_GAME       = 4,
            E_PROCESS_QUICK_JOIN_GAME           = 5,
            E_PROCESS_LEAVE_AND_QUICK_JOIN_GAME = 6,
            E_PROCESS_SEARCH_FOR_GAMES          = 7,
            E_PROCESS_COUNT                     = 8,
        };

        enum EAction
        {
            E_ACTION_LOCK_USERSET          = 0,
            E_ACTION_CREATE_GAME           = 1,
            E_ACTION_JOIN_GAME             = 2,
            E_ACTION_LEAVE_GAME            = 3,
            E_ACTION_QUICK_JOIN_GAME       = 4,
            E_ACTION_SEARCH_FOR_GAMES      = 5,
            E_ACTION_WAIT_SEARCH_FOR_GAMES = 6,
            E_ACTION_UNLOCK_USERSET        = 7,
            E_ACTION_CREATE_USERSET        = 8,
            E_ACTION_JOIN_USERSET          = 9,
            E_ACTION_LEAVE_USERSET         = 10,
            E_ACTION_WAIT_IN_GAME          = 11,
            E_ACTION_COUNT                 = 12,
        };

        // ---- lifecycle -----------------------------------------------------------
        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void Update();

        // ---- requests (each starts a process) --------------------------------------
        void CreateGame(const GameParams* lpGameParams, Callback lpfCallback, void* lpCallbackUserData);
        // Copies the parameters into mGameParameters, then starts JOIN_GAME, or
        // LEAVE_AND_JOIN_GAME when the local player is already in a game.
        void JoinGame(const GameParams* lpGameParams, bool lbJoinFlag,
                      Callback lpfCallback, void* lpCallbackUserData);
        void LeaveGame(Callback lpfCallback, void* lpCallbackUserData);
        // Latches the two flags into mbQuickJoinRanked / mbQuickJoinFreeburn, then starts
        // QUICK_JOIN_GAME (or LEAVE_AND_QUICK_JOIN_GAME when already in a game).
        void QuickJoinGame(bool lbRanked, bool lbFreeburn,
                           Callback lpfCallback, void* lpCallbackUserData);
        void SearchForGames(const GameSearchParams* lpGameSearchParams,
                            Callback lpfCallback, void* lpCallbackUserData);
        void Disconnected();

    private:
        // The ordered action list of one process.
        struct ProcessActions
        {
            static const s32 KI_MAX_ACTIONS = 4;
            EAction maeAction[KI_MAX_ACTIONS];
        };

        typedef void (MatchMakingManager::*UpdateFunction)();
        typedef bool (MatchMakingManager::*ActionFunction)();

        void InitialiseUpdateArray();
        void InitialiseActionArray();
        void StartProcess(EProcess leProcess, Callback lpfCallback, void* lpCallbackUserData);
        void ProcessComplete(bool lbSuccess);
        void SetNextAction();

        // Registered with the server interface's game component as its search-result sort
        // callback (the user data is the manager).
        static s32 SortGameSearchResultsCallback(void* lpUserData,
                                                 CgsNetwork::ServerInterfaceGameParamsBase* lpGameA,
                                                 CgsNetwork::ServerInterfaceGameParamsBase* lpGameB);

        void UpdateNone();
        void UpdateWaitIdle();
        void UpdateWaitToSearch();
        void UpdateWaitInGame();

        bool ActionLockUserset();
        bool ActionCreateGame();
        bool ActionJoinGame();
        bool ActionLeaveGame();
        bool ActionQuickJoinGame();
        bool ActionSearchForGames();
        bool ActionWaitSearchForGames();
        bool ActionUnLockUserset();
        bool ActionCreateUserset();
        bool ActionJoinUserset();
        bool ActionLeaveUserset();
        bool ActionWaitInGame();

        bool ServerInterfaceIdle();

        // Console layout, pinned in a 32-bit build; inert on the x64 host.
        static void _AssertLayout();

        // ---- data members --------------------------------------------------------
        ProcessActions         maProcessActions[E_PROCESS_COUNT];   // +0x000
        UpdateFunction         maUpdateFunctions[E_SUBSTATE_COUNT]; // +0x080
        ActionFunction         maActionFunctions[E_ACTION_COUNT];   // +0x090
        CgsNetwork::EComponents maeComponentToUse[E_ACTION_COUNT];  // +0x0C0
        ESubState              meSubState;                          // +0x0F0
        BrnNetworkManager*     mpNetworkManager;                    // +0x0F4
        Callback               mCallback;                           // +0x0F8
        void*                  mpCallbackUserData;                  // +0x0FC
        GameParams             mGameParameters;                     // +0x100
        u8                     maGameSearchParametersStorage[0x390]; // +0x6D0 GameSearchParams (FLAG above)
        GameParams             mFoundGameA;                         // +0xA60
        GameParams             mFoundGameB;                         // +0x1030
        bool                   mbQuickJoinRanked;                   // +0x1600
        bool                   mbQuickJoinFreeburn;                 // +0x1601
        CgsSystem::Time        mLastSearchTime;                     // +0x1604
        EProcess               meCurrentProcess;                    // +0x160C
        u8                     mun8NextAction;                      // +0x1610
        bool                   mbCreateGameServerGame;              // +0x1611
    };

    inline void MatchMakingManager::_AssertLayout()
    {
#define BRN_MMM_AT(member, off) \
        static_assert(sizeof(void*) != 4 || offsetof(MatchMakingManager, member) == (off), #member " @ " #off)
        BRN_MMM_AT(maUpdateFunctions,     0x080);
        BRN_MMM_AT(maActionFunctions,     0x090);
        BRN_MMM_AT(maeComponentToUse,     0x0C0);
        BRN_MMM_AT(meSubState,            0x0F0);
        BRN_MMM_AT(mpNetworkManager,      0x0F4);
        BRN_MMM_AT(mCallback,             0x0F8);
        BRN_MMM_AT(mpCallbackUserData,    0x0FC);
        BRN_MMM_AT(mGameParameters,       0x100);
        BRN_MMM_AT(maGameSearchParametersStorage, 0x6D0);
        BRN_MMM_AT(mFoundGameA,           0xA60);
        BRN_MMM_AT(mFoundGameB,           0x1030);
        BRN_MMM_AT(mbQuickJoinRanked,     0x1600);
        BRN_MMM_AT(mbQuickJoinFreeburn,   0x1601);
        BRN_MMM_AT(mLastSearchTime,       0x1604);
        BRN_MMM_AT(meCurrentProcess,      0x160C);
        BRN_MMM_AT(mun8NextAction,        0x1610);
        BRN_MMM_AT(mbCreateGameServerGame, 0x1611);
#undef BRN_MMM_AT
        static_assert(sizeof(void*) != 4 || sizeof(GameParams) == 0x5D0, "GameParams is 0x5D0 bytes");
        static_assert(sizeof(void*) != 4 || sizeof(MatchMakingManager) == 0x1614, "MatchMakingManager is 0x1614 bytes");
    }
}

#endif
