#ifndef BRN_NETWORK_POST_ROUND_MANAGER_H
#define BRN_NETWORK_POST_ROUND_MANAGER_H

#include "types.hpp"

// ===========================================================================
// BrnNetwork::PostRoundManager
//   Home: GameSource/Network/Managers/BrnNetworkPostRoundManager.{h,cpp}
//
// The online post-round / post-game state machine. After a round (or game)
// ends it drives a fixed sequence of "actions" (send results, change params,
// leave/end game, resume the server interface...) selected by the "process"
// that was started, waiting on the DirtySock server interface to go idle
// between steps and finally firing the caller's completion callback.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (per-function addresses noted in
// the .cpp). Member layout follows the DecFIGS DWARF declaration shape for
// BrnNetworkPostRoundManager.h; the X360 byte offsets (mpNetworkManager @+168,
// the dispatch tables @+120/+140, etc.) are a 32-bit-pointer layout fact and
// are intentionally NOT static_asserted on the 64-bit host (function pointers /
// pointers widen there). Members are reached BY NAME.
//
// The X360 build dispatches the per-substate / per-action handlers through two
// tables of plain function pointers it fills in at Construct time
// (InitialiseUpdateArray / InitialiseActionArray). They are modelled here as
// tables of member-function pointers so each handler is still reached through
// the table (matching the binary) while staying type-safe.
//
// The pervasive CgsDev::StrStream debug logging in the X360 functions ("Starting
// action ...", "ProcessComplete ...") has no observable effect on the state
// machine and references the un-reconstructed global debug stream, so -- as with
// the other committed BrnNetwork manager TUs -- it is not reproduced here.
// ===========================================================================

namespace GameStateModuleIO
{
    struct OnlineGameResults;   // ProcessRaceResults input (home: BrnGameActions.h)
}

namespace BrnNetwork
{
    class BrnNetworkManager;
    class GameResults;     // embedded by value (own home: BrnNetworkGameResults.h)
}

#include "GameSource/Network/BrnNetworkGameResults.h"   // BrnNetwork::GameResults (embedded mGameResults)

namespace BrnNetwork
{
    class PostRoundManager
    {
    public:
        // Completion callback fired when the started process finishes.
        typedef void (*Callback)(bool lbSuccess, void* lpUserData);

        // BrnNetworkPostRoundManager.h:94
        enum ESubState
        {
            E_SUBSTATE_NONE              = 0,
            E_SUBSTATE_WAIT_IDLE         = 1,
            E_SUBSTATE_WAIT_CALLBACK     = 2,
            E_SUBSTATE_WAIT_GAME_UNSTART = 3,
            E_SUBSTATE_WAIT_RESUME       = 4,
            E_SUBSTATE_COUNT             = 5
        };

        // BrnNetworkPostRoundManager.h:105
        enum EProcess
        {
            E_PROCESS_END_ROUND        = 0,
            E_PROCESS_END_GAME_HOST    = 1,
            E_PROCESS_END_GAME_CLIENT  = 2,
            E_PROCESS_END_GAME_LEAVE_HOST   = 3,
            E_PROCESS_END_GAME_LEAVE_CLIENT = 4,
            E_PROCESS_COUNT            = 5
        };

        // BrnNetworkPostRoundManager.h:116
        enum EAction
        {
            E_ACTION_SEND_RESULTS        = 0,
            E_ACTION_RESUME              = 1,
            E_ACTION_LEAVE_GAME          = 2,
            E_ACTION_WAIT_UNSTART_GAME   = 3,
            E_ACTION_END_GAME            = 4,
            E_ACTION_CHANGE_PLAYER_PARMS = 5,
            E_ACTION_CHANGE_GAME_PARMS   = 6,
            E_ACTION_COUNT               = 7
        };

        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void Update();
        void StartEndOfRound(Callback lpfCallback, void* lpCallbackUserData);
        void StartEndOfGame(bool lbQuit, bool lbLeaveGame,
                            Callback lpfCallback, void* lpCallbackUserData);
        void Disconnected();
        void ProcessRaceResults(const GameStateModuleIO::OnlineGameResults* lpRaceResults);

    private:
        // BrnNetworkPostRoundManager.h:130 -- the ordered action list for one process.
        struct ProcessActions
        {
            // BrnNetworkPostRoundManager.h:132
            static const s32 KI_MAX_ACTIONS = 6;

            // BrnNetworkPostRoundManager.h:134
            EAction maeAction[KI_MAX_ACTIONS];
        };

        typedef void (PostRoundManager::*UpdateFunction)();
        typedef bool (PostRoundManager::*ActionFunction)();

        void InitialiseUpdateArray();
        void InitialiseActionArray();
        void StartProcess(EProcess leProcess, Callback lpfCallback, void* lpCallbackUserData);
        void ProcessComplete(bool lbSuccess);
        void SetNextAction();

        void UpdateNone();
        void UpdateWaitIdle();
        void UpdateWaitCallback();
        void UpdateWaitResume();
        void UpdateWaitGameUnstart();

        bool ActionResume();
        bool ActionLeaveGame();
        bool ActionSendResults();
        bool ActionUnstartGame();
        bool ActionEndGame();
        bool ActionChangePlayerParams();
        bool ActionChangeGameParams();

        bool ServerInterfaceIdle();

        static void ResumeCompleteCallback(bool lbSuccess, void* lpUserData);

        // ---- Data members (DWARF order). X360 offsets in comments. ----------------
        ProcessActions  maProcessActions[E_PROCESS_COUNT];          // +0
        UpdateFunction  maUpdateFunctions[E_SUBSTATE_COUNT];        // +120
        ActionFunction  maActionFunctions[E_ACTION_COUNT];          // +140
        BrnNetworkManager* mpNetworkManager;                        // +168
        Callback        mCallback;                                  // +172
        void*           mpCallbackUserData;                         // +176
        ESubState       meSubState;                                 // +180
        u8              mun8CurrentAction;                          // +184
        EProcess        meCurrentProcess;                           // +188
        bool            mbDidTheUserQuit;                           // +192
        GameResults     mGameResults;                               // +196
    };
}

#endif // BRN_NETWORK_POST_ROUND_MANAGER_H
