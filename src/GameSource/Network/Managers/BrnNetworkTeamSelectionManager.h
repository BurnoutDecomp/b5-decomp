#pragma once

// ===================================================================================
// BrnNetwork::TeamSelectionManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h
//
// The online team-selection manager. It registers a per-player reliable
// BrnNetwork::TeamSelectMessage (home: BrnTeamSelectMessage.h, REUSED BY NAME) on every
// joined player, runs a small process/action state machine that assigns each in-game
// player to a team (co-op / free-for-all / autobalance), and -- on the host -- broadcasts
// the final per-player {player id, team} table to everyone. Remote machines receive that
// broadcast in _TeamSelectionMessageArrivedCallback and latch the result.
//
// There is NO DecFIGS DWARF for this TU (dwarfdump none found for this source path); the
// class shape is recovered directly from the X360 asm (BURNOUT_X360_ARTIST.XEX), modelled
// on the committed sibling BrnNetwork::SelectedRoutesManager (same per-player reliable-
// message-slot pattern). Members are accessed BY NAME; the C++ struct uses native x64
// widths and the compiler lays it out naturally -- semantic parity, not byte-exact X360
// layout (the project rule). The X360 byte offsets are kept as documentation.
//
// X360 LAYOUT (32-bit-pointer ABI, documentation only -- from the Construct/AddPlayer asm):
//   +0x000  maTeamSelectionData[7]   (TeamSelectionData, 236B each -> +0x674 == 1652)
//   +0x674  maActiveRaceCarTeam[8]   (ActiveRaceCarTeam, 8B each   -> +0x6B4 == 1716)
//   +0x6B4  miNumPlayersAssigned     (s32)
//   +0x6B8  maProcessActions[8]      (s32[8], the per-process action-index table)
//   +0x6D8  maActionFunctions[5]     (ActionFunction[5])
//   +0x6EC  maUpdateFunctions[2]     (UpdateFunction[2])
//   +0x6F4  meSubState               (s32)  -- output/idle substate (Construct: 2)
//   +0x6F8  meUpdateState            (s32)  -- index into maUpdateFunctions (Construct: 0)
//   +0x6FC  meCurrentProcess         (s32)  -- index into maProcessActions  (Construct: 4)
//   +0x700  miActionIndex            (s32)  -- action cursor within the current process
//   +0x704  mpfCompletionCallback    (CompletionCallback)
//   +0x708  mpCompletionUserData     (s32)
//   +0x70C  mpNetworkManager         (BrnNetworkManager*)
//   +0x710  mbReceivedFinalSelection (bool, trailing byte)
//
//   TeamSelectionData (236B X360 stride):
//     +0x00  mMessageSend   (TeamSelectMessage, 116B)
//     +0x74  mMessageRecv   (TeamSelectMessage, 116B)
//     +0xE8  mPlayerID      (NetworkPlayerID s32, sentinel -1)
//
//   ActiveRaceCarTeam (8B):
//     +0x00  mPlayerID      (NetworkPlayerID s32, sentinel -1)
//     +0x04  miTeam         (s32, 0 == unassigned)
// ===================================================================================

#include "types.hpp"                                              // s32, u8, bool
#include "GameSource/Network/Messages/BrnTeamSelectMessage.h"     // BrnNetwork::TeamSelectMessage (committed home)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"       // BrnNetwork::NetworkPlayerID (committed typedef)

namespace CgsNetwork
{
    // Pointer-only on the reliable-message arrived callback (no member access through the
    // base here); forward-declared to avoid pulling the message hierarchy into this header.
    struct ReliableMessage;
}

namespace BrnNetwork
{
    // Pointer-only member -- declared-only here (full type in BrnNetworkManager.h, included by
    // the .cpp).
    class BrnNetworkManager;

    // The BrnNetworkModuleIO::OutputBuffer that ProcessBeforeSimulation publishes the final team
    // table onto; pointer-only param. BrnNetworkModuleIO is a NAMESPACE (not a class) -- forward-
    // declare the struct inside it (full type in BrnNetworkModuleIO.h, included by the .cpp).
    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;
    }

    struct TeamSelectionManager
    {
        // "No player" sentinel (== CgsNetwork::K_INVALID_PLAYER_ID, -1).
        static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        // Per-player reliable-message slots tracked (Construct's outer loop runs 7 times;
        // AddPlayer's free-slot scan bounds at 7).
        static const s32 KI_MAX_PLAYERS = 7;

        // Active-race-car team slots (Construct's inner loop / the E_ACTIVE_RACE_CAR_INDEX_COUNT
        // assert bound).
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

        // The wire EMessageType + packed length stamped onto a team-select reliable registration
        // (X360 AddPlayer: li r4,0x2B == 43 ; li r5,0x74 == 116).
        static const s32 KI_TEAM_SELECT_MESSAGE_TYPE   = 43;
        static const s32 KI_TEAM_SELECT_MESSAGE_LENGTH = 116;

        // Number of distinct processes (maProcessActions is indexed [process][action], 2 actions
        // per process; InitialiseActionArray validates 4 process rows).
        static const s32 KI_NUM_PROCESSES        = 4;
        static const s32 KI_ACTIONS_PER_PROCESS  = 2;
        static const s32 KI_NUM_ACTION_FUNCTIONS = 5;
        static const s32 KI_NUM_UPDATE_FUNCTIONS = 2;

        // Sentinel stored in every maProcessActions slot before the real action indices are
        // written (InitialiseActionArray: li r8,5, fill 8 slots; the validation asserts no slot
        // is left == 5 within an active process).
        static const s32 KI_NO_ACTION = 5;

        // The wire event id + byte size of the final team table published onto the OutputBuffer's
        // network event queue (X360 ProcessBeforeSimulation: li r5,0x15 == 21 ; li r6,0x20 == 32).
        static const s32 KI_OUTEVENT_TEAM_SELECTION      = 21;
        static const s32 KI_OUTEVENT_TEAM_SELECTION_SIZE = 32;

        // The substate sentinel UpdateWaitIdle / ProcessBeforeSimulation gate on (Construct seeds
        // meSubState = 2; the do-nothing "idle/done" value).
        static const s32 KI_SUBSTATE_IDLE = 2;

        // The leProcess selectors StartTeamSelection maps the game-state launch type onto.
        static const s32 KI_PROCESS_FFA          = 0;
        static const s32 KI_PROCESS_COOP         = 1;
        static const s32 KI_PROCESS_AUTOBALANCE_HOST   = 2; // local player IS host
        static const s32 KI_PROCESS_AUTOBALANCE_CLIENT = 3; // local player is NOT host
        static const s32 KI_PROCESS_NONE         = 4;       // Construct idle value

        // The game-state launch types StartTeamSelection switches on (X360 case 12/14/17).
        static const s32 KI_LAUNCH_TYPE_AUTOBALANCE = 12;
        static const s32 KI_LAUNCH_TYPE_FFA         = 14;
        static const s32 KI_LAUNCH_TYPE_COOP        = 17;

        // The team id co-op assigns every in-game player to (X360 ActionAssignCoop: li 2).
        static const s32 KI_COOP_TEAM = 2;

        // Completion callback invoked when a process finishes (1 == success, 0 == failure) with
        // the user-data StartProcess captured. X360: v5(liResult, lUserData).
        typedef int (*CompletionCallback)(int liResult, int liUserData);

        // The per-substate process-action driver (the maActionFunctions[] elements) and the
        // per-update-state driver (the maUpdateFunctions[] elements). Both take the manager.
        typedef int (*ActionFunction)(TeamSelectionManager* lpThis);
        typedef int (*UpdateFunction)(TeamSelectionManager* lpThis);

        // Per-player reliable-message slot (X360 236B stride): the outbound + inbound team-select
        // messages plus the player this slot tracks. mPlayerID @ +0xE8 (sentinel -1) is the
        // free-slot marker AddPlayer scans for.
        struct TeamSelectionData
        {
            TeamSelectMessage mMessageSend;   // +0x00 (116B)
            TeamSelectMessage mMessageRecv;   // +0x74 (116B)
            NetworkPlayerID   mPlayerID;      // +0xE8 (sentinel -1)
        };

        // Per-active-race-car team assignment (8B): which network player occupies the active
        // race-car slot and the team they were placed on (0 == unassigned).
        struct ActiveRaceCarTeam
        {
            NetworkPlayerID mPlayerID;   // +0x00 (sentinel -1)
            s32             miTeam;      // +0x04 (0 == unassigned)
        };

        // === lifecycle ===
        void Construct( BrnNetworkManager* lpNetworkManager );
        void Destruct();

        // === per-player registration ===
        bool AddPlayer( NetworkPlayerID lPlayerID );
        bool RemovePlayer( NetworkPlayerID lPlayerID );
        void Disconnected();

        // === driving the state machine ===
        // Kick off team selection for the given launch type; a3/a4 are the completion
        // callback + its user data routed to StartProcess.
        int  StartTeamSelection( s32 liLaunchType, CompletionCallback lpfCompletionCallback,
                                 s32 liUserData );
        void StartProcess( s32 leProcess, CompletionCallback lpfCompletionCallback, s32 liUserData );
        int  SetNextAction();

        // === per-frame module hooks ===
        void ProcessBeforeSimulation( BrnNetworkModuleIO::OutputBuffer* lpOutput );
        int  ProcessAfterSimulation();

        // === update-state drivers (registered into maUpdateFunctions) ===
        // @ 0x82556EE8 / @ 0x8254BE60 -- both inline below.
        int  UpdateWaitIdle();
        int  ActionWaitFinalTeamSelection();

        // === process-action drivers (registered into maActionFunctions) ===
        // Co-op assigns every in-game player to one shared team (KI_COOP_TEAM).
        int  ActionAssignCoopStuntRunTeams();
        // FFA / autobalance derive each player's team from their server player-parameters; both
        // dead-end on the un-homed ServerInterfaceGamesX360 in-game predicate + the un-homed
        // BrnNetwork::PlayerParams subclass team field -- DECLARATION-ONLY (see the .cpp note).
        int  ActionAssignFFAStuntRunTeams();
        int  ActionAutobalanceStuntRunTeams();
        // Broadcasts the assembled per-player team table to every other player (host).
        int  ActionBroadcastFinalTeamSelection();

    private:
        // Find the slot tracking lPlayerID, or null. (Mirrors the X360 RemovePlayer scan.)
        TeamSelectionData* GetTeamSelectionDataEntry( NetworkPlayerID lPlayerID );

        // The reliable-message arrived callback registered per player (userData == manager).
        static void _TeamSelectionMessageArrivedCallback( CgsNetwork::ReliableMessage* lpMessage,
                                                          NetworkPlayerID lSendingPlayerID,
                                                          void* lpUserData );

        // Free-function thunks stored into maUpdateFunctions / maActionFunctions. On the X360 the
        // member addresses go straight into the tables (the platform C ABI passes `this` first);
        // these thunks are the faithful PC equivalent of those table entries.
        static int UpdateIdleNoOp( TeamSelectionManager* lpThis );
        static int UpdateWaitIdleThunk( TeamSelectionManager* lpThis );
        static int ActionAssignFFAThunk( TeamSelectionManager* lpThis );
        static int ActionAssignCoopThunk( TeamSelectionManager* lpThis );
        static int ActionAutobalanceThunk( TeamSelectionManager* lpThis );
        static int ActionBroadcastFinalThunk( TeamSelectionManager* lpThis );
        static int ActionWaitFinalThunk( TeamSelectionManager* lpThis );

        // --- members (X360-order) -------------------------------------------------------------
        TeamSelectionData  maTeamSelectionData[KI_MAX_PLAYERS];                     // +0x000
        ActiveRaceCarTeam  maActiveRaceCarTeam[KI_MAX_ACTIVE_RACE_CARS];            // +0x674
        s32                miNumPlayersAssigned;                                    // +0x6B4
        s32                maProcessActions[KI_NUM_PROCESSES * KI_ACTIONS_PER_PROCESS]; // +0x6B8
        ActionFunction     maActionFunctions[KI_NUM_ACTION_FUNCTIONS];             // +0x6D8
        UpdateFunction     maUpdateFunctions[KI_NUM_UPDATE_FUNCTIONS];             // +0x6EC
        s32                meSubState;                                             // +0x6F4
        s32                meUpdateState;                                          // +0x6F8
        s32                meCurrentProcess;                                       // +0x6FC
        s32                miActionIndex;                                          // +0x700
        CompletionCallback mpfCompletionCallback;                                  // +0x704
        s32                mCompletionUserData;                                    // +0x708
        BrnNetworkManager* mpNetworkManager;                                       // +0x70C
        bool               mbReceivedFinalSelection;                              // +0x710

        // Initialise the two function-pointer tables (called from Construct).
        void InitialiseUpdateArray();
        void InitialiseActionArray();

        // Uncalled layout pin; befriend it so the static_asserts can name the private members.
        friend void TeamSelectionManager_AssertLayout();
    };

    // ---- UpdateWaitIdle @ 0x82556EE8 -----------------------------------------------------------
    // X360: if (meSubState == 2) return SetNextAction(); else return (int)this.
    inline int TeamSelectionManager::UpdateWaitIdle()
    {
        if (meSubState == KI_SUBSTATE_IDLE)
            return SetNextAction();
        // X360 returns r3 == `this` here; the value is only ever truthiness-tested by the caller,
        // so a non-zero success token is the faithful (pointer-width-safe) equivalent.
        return 1;
    }

    // ---- ActionWaitFinalTeamSelection @ 0x8254BE60 ---------------------------------------------
    // X360: *(this+1780) = 0; return 1;  (clears meSubState, reports success).
    inline int TeamSelectionManager::ActionWaitFinalTeamSelection()
    {
        meSubState = 0;
        return 1;
    }
}
