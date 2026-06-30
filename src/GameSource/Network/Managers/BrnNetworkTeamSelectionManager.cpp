#include "types.hpp"

#include "GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h"
#include "GameSource/Network/BrnNetworkManager.h"          // BrnNetwork::BrnNetworkManager (GetPlayerManager/GetServerInterface/GetActiveRaceCarIndex/GetCurrentFrame)
#include "GameSource/Network/BrnServerInterface.h"         // BrnNetwork::BrnServerInterface::GetGameComponent
#include "GameSource/Network/BrnNetworkModuleIO.h"         // BrnNetworkModuleIO::OutputBuffer / NetworkEventQueue
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"   // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // CgsNetwork::NetworkPlayer (Register/UnRegisterMessageType)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // ServerInterfaceGames
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // CgsModule::VariableEventQueue<14000,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cstddef>                                         // offsetof (uncalled layout pin)

// Reconstructed from BURNOUT_X360_ARTIST.XEX (no DecFIGS DWARF for this TU):
//   Construct                          @ 0x82573938   AddPlayer                     @ 0x82556F00
//   Destruct                           @ 0x8254B7E8   RemovePlayer                  @ 0x82557018
//   Disconnected                       @ 0x82557100   InitialiseUpdateArray         @ 0x8255E738
//   InitialiseActionArray              @ 0x8256FF10   StartTeamSelection            @ 0x8254BB58
//   StartProcess                       @ 0x8254B978   SetNextAction                 @ 0x8254BA70
//   ProcessBeforeSimulation            @ 0x825656D8   ProcessAfterSimulation        @ 0x8254B8B0
//   ActionAssignCoopStuntRunTeams      @ 0x8254BC48   ActionBroadcastFinalTeamSelection @ 0x82557178
//   _TeamSelectionMessageArrivedCallback @ 0x8254BE78
//   (UpdateWaitIdle @ 0x82556EE8 / ActionWaitFinalTeamSelection @ 0x8254BE60 are inline in the header.)
//
// DECLARATION-ONLY (no faithful body): ActionAssignFFAStuntRunTeams @ 0x8256C278 and
//   ActionAutobalanceStuntRunTeams @ 0x8256C4C8. Both derive each player's team by building a
//   BrnNetwork::PlayerParams subclass on the stack (vtable off_82083550), calling PreparePattern,
//   GetPlayerParametersByPlayerID, and reading a per-player team byte out of that subclass'
//   un-homed field layout. They also reach the in-game predicate sub_82878620 on the still-`todo`
//   ServerInterfaceGamesX360. Rather than fabricate the PlayerParams field offset / vtable, these
//   two actions are left as the header declaration only (same precedent as
//   GamerPictureManagerX360::AddPlayer, which dead-ends on the same un-homed dependencies).

namespace BrnNetwork
{
    // The concrete network event queue behind the OutputBuffer's incomplete NetworkEventQueue
    // forward (same pattern as BrnChallengeSuccessManager.cpp / BrnEventScoresManager.cpp).
    namespace
    {
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline NetworkEventQueueConcrete* AsConcreteQueue(
            BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }
    }

    // ---------------------------------------------------------------------------------
    // InitialiseUpdateArray @ 0x8255E738
    //   Seeds the two update-state drivers (idle no-op @ [0], UpdateWaitIdle @ [1]) and asserts
    //   neither slot is null.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::InitialiseUpdateArray()
    {
        maUpdateFunctions[0] = 0;
        maUpdateFunctions[1] = 0;
        // X360 [0] is CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct -- the
        // COMDAT-folded `return 1` no-op the idle update-state runs (a function that simply
        // succeeds). Modelled here by a named local no-op so the table is a real callable.
        maUpdateFunctions[0] = &TeamSelectionManager::UpdateIdleNoOp;
        maUpdateFunctions[1] = &TeamSelectionManager::UpdateWaitIdleThunk;

        for (s32 liIndex = 0; liIndex < KI_NUM_UPDATE_FUNCTIONS; ++liIndex)
            CGS_ASSERT(maUpdateFunctions[liIndex] != 0, "No update function supplied for this sub state");
    }

    // ---------------------------------------------------------------------------------
    // InitialiseActionArray @ 0x8256FF10
    //   Seeds the five process-action drivers, asserts none is null, fills the per-process action
    //   table with the KI_NO_ACTION sentinel, then writes the real action indices and asserts no
    //   active process slot is left as the sentinel.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::InitialiseActionArray()
    {
        for (s32 liInit = 0; liInit < KI_NUM_ACTION_FUNCTIONS; ++liInit)
            maActionFunctions[liInit] = 0;

        maActionFunctions[0] = &TeamSelectionManager::ActionAssignFFAThunk;
        maActionFunctions[1] = &TeamSelectionManager::ActionAssignCoopThunk;
        maActionFunctions[2] = &TeamSelectionManager::ActionAutobalanceThunk;
        maActionFunctions[3] = &TeamSelectionManager::ActionBroadcastFinalThunk;
        maActionFunctions[4] = &TeamSelectionManager::ActionWaitFinalThunk;

        for (s32 liCheck = 0; liCheck < KI_NUM_ACTION_FUNCTIONS; ++liCheck)
            CGS_ASSERT(maActionFunctions[liCheck] != 0, "No update function supplied for this substate");

        // Fill every action slot with the "no action" sentinel (X360 fills 8 slots with 5).
        for (s32 liFill = 0; liFill < KI_NUM_PROCESSES * KI_ACTIONS_PER_PROCESS; ++liFill)
            maProcessActions[liFill] = KI_NO_ACTION;

        // Write the real action indices per process (X360 stores: [0]=0,[2]=1,[4]=2,[5]=3,[6]=4).
        maProcessActions[0] = 0;   // FFA:               action 0
        maProcessActions[2] = 1;   // co-op:             action 1
        maProcessActions[4] = 2;   // autobalance host:  action 2
        maProcessActions[5] = 3;   // autobalance host:  then broadcast
        maProcessActions[6] = 4;   // autobalance client: wait-for-final

        // Assert no active process row was left with the sentinel still in slot 0 (X360 walks the
        // 4 process rows at stride 2 and asserts slot[0] != 5).
        for (s32 liProcess = 0; liProcess < KI_NUM_PROCESSES; ++liProcess)
            CGS_ASSERT(maProcessActions[liProcess * KI_ACTIONS_PER_PROCESS] != KI_NO_ACTION,
                       "No actions supplied for this process");
    }

    // ---------------------------------------------------------------------------------
    // Construct @ 0x82573938
    //   Constructs every per-player slot's two messages (and frees the slot), clears the active-
    //   race-car team table, seeds the state-machine scalars, asserts the network manager, and
    //   builds the two function-pointer tables.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::Construct( BrnNetworkManager* lpNetworkManager )
    {
        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            maTeamSelectionData[liPlayer].mPlayerID = KI_INVALID_PLAYER_ID;
            maTeamSelectionData[liPlayer].mMessageSend.Construct();
            maTeamSelectionData[liPlayer].mMessageRecv.Construct();
        }

        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            maActiveRaceCarTeam[liCar].mPlayerID = KI_INVALID_PLAYER_ID;
            maActiveRaceCarTeam[liCar].miTeam    = 0;
        }

        miNumPlayersAssigned     = 0;
        mbReceivedFinalSelection = false;
        mpNetworkManager         = lpNetworkManager;
        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");

        miActionIndex         = 0;
        mpfCompletionCallback = 0;
        mCompletionUserData   = 0;
        meUpdateState         = 0;
        meCurrentProcess      = KI_PROCESS_NONE;   // 4
        meSubState            = KI_SUBSTATE_IDLE;   // 2

        InitialiseUpdateArray();
        InitialiseActionArray();
    }

    // ---------------------------------------------------------------------------------
    // Destruct @ 0x8254B7E8
    //   Resets the state-machine scalars and the network-manager pointer, re-Constructs every
    //   slot's two messages (and frees the slot), and clears the active-race-car team table.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::Destruct()
    {
        meSubState            = KI_SUBSTATE_IDLE;   // 2
        meCurrentProcess      = KI_PROCESS_NONE;    // 4
        meUpdateState         = 0;
        mpfCompletionCallback = 0;
        mCompletionUserData   = 0;
        miActionIndex         = 0;
        mpNetworkManager      = 0;

        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            maTeamSelectionData[liPlayer].mPlayerID = KI_INVALID_PLAYER_ID;
            maTeamSelectionData[liPlayer].mMessageSend.Construct();
            maTeamSelectionData[liPlayer].mMessageRecv.Construct();
        }

        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            maActiveRaceCarTeam[liCar].mPlayerID = KI_INVALID_PLAYER_ID;
            maActiveRaceCarTeam[liCar].miTeam    = 0;
        }

        miNumPlayersAssigned     = 0;
        mbReceivedFinalSelection = false;
    }

    // ---------------------------------------------------------------------------------
    // GetTeamSelectionDataEntry  (the slot-scan shared by RemovePlayer / the broadcast)
    // ---------------------------------------------------------------------------------
    TeamSelectionManager::TeamSelectionData*
    TeamSelectionManager::GetTeamSelectionDataEntry( NetworkPlayerID lPlayerID )
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        {
            if (maTeamSelectionData[liIndex].mPlayerID == lPlayerID)
                return &maTeamSelectionData[liIndex];
        }
        return 0;
    }

    // ---------------------------------------------------------------------------------
    // AddPlayer @ 0x82556F00
    //   Looks the joined player up in the registry; if present, claims the first FREE slot
    //   (mPlayerID == -1, asserting one exists), registers the team-select reliable message type
    //   on that player (type 43, length 116, the slot's send/recv messages, the arrived callback,
    //   a no-op delivered callback, manager `this` as user data), then stamps the slot id.
    // ---------------------------------------------------------------------------------
    bool TeamSelectionManager::AddPlayer( NetworkPlayerID lPlayerID )
    {
        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);

        if (lpNetworkPlayer != 0)
        {
            TeamSelectionData* lpDataEntry = 0;
            for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
            {
                if (maTeamSelectionData[liIndex].mPlayerID == KI_INVALID_PLAYER_ID)
                {
                    lpDataEntry = &maTeamSelectionData[liIndex];
                    break;
                }
            }
            CGS_ASSERT(lpDataEntry != 0, "lpDataEntry");

            lpNetworkPlayer->RegisterMessageType(
                KI_TEAM_SELECT_MESSAGE_TYPE,
                KI_TEAM_SELECT_MESSAGE_LENGTH,
                &lpDataEntry->mMessageSend,
                &lpDataEntry->mMessageRecv,
                _TeamSelectionMessageArrivedCallback,
                // X360 passes the COMDAT-folded no-op delivered callback (a `return` thunk);
                // a null delivered callback is the same observable behaviour.
                0,
                this);

            lpDataEntry->mPlayerID = lPlayerID;
        }
        return lpNetworkPlayer != 0;
    }

    // ---------------------------------------------------------------------------------
    // RemovePlayer @ 0x82557018
    //   Frees the slot tracking lPlayerID (mark free + re-Construct its two messages), then -- if
    //   the player is still in the registry -- unregisters the reliable message type from it.
    // ---------------------------------------------------------------------------------
    bool TeamSelectionManager::RemovePlayer( NetworkPlayerID lPlayerID )
    {
        TeamSelectionData* lpEntry = GetTeamSelectionDataEntry(lPlayerID);
        if (lpEntry != 0)
        {
            lpEntry->mPlayerID = KI_INVALID_PLAYER_ID;
            lpEntry->mMessageSend.Construct();
            lpEntry->mMessageRecv.Construct();
        }

        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != 0)
            lpNetworkPlayer->UnRegisterMessageType(KI_TEAM_SELECT_MESSAGE_TYPE);

        return lpNetworkPlayer != 0;
    }

    // ---------------------------------------------------------------------------------
    // Disconnected @ 0x82557100
    //   Session-wide teardown: RemovePlayer every still-tracked slot (asserting it was freed).
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::Disconnected()
    {
        for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            if (maTeamSelectionData[liPlayerIndex].mPlayerID != KI_INVALID_PLAYER_ID)
            {
                RemovePlayer(maTeamSelectionData[liPlayerIndex].mPlayerID);
                CGS_ASSERT(maTeamSelectionData[liPlayerIndex].mPlayerID == KI_INVALID_PLAYER_ID,
                           "maTeamSelectionData[liPlayerIndex].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID");
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // StartProcess @ 0x8254B978
    //   Asserts no process is already running, captures the completion callback + user data + the
    //   process selector, resets the action cursor, and arms the update state machine.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::StartProcess( s32 leProcess, CompletionCallback lpfCompletionCallback,
                                             s32 liUserData )
    {
        CGS_ASSERT(mpfCompletionCallback == 0, "Process already started!");

        mpfCompletionCallback = lpfCompletionCallback;
        mCompletionUserData   = liUserData;
        meCurrentProcess      = leProcess;
        miActionIndex         = 0;
        meUpdateState         = 1;
    }

    // ---------------------------------------------------------------------------------
    // StartTeamSelection @ 0x8254BB58
    //   Maps the game-state launch type onto a process selector (autobalance splits on whether the
    //   local player is the host) and starts it. For any other launch type just fires the
    //   completion callback with success.
    // ---------------------------------------------------------------------------------
    int TeamSelectionManager::StartTeamSelection( s32 liLaunchType,
                                                  CompletionCallback lpfCompletionCallback,
                                                  s32 liUserData )
    {
        s32 leProcess;
        switch (liLaunchType)
        {
            case KI_LAUNCH_TYPE_AUTOBALANCE: // 12
            {
                CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");

                const bool lbIsLocalPlayerHost =
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost();
                leProcess = lbIsLocalPlayerHost ? KI_PROCESS_AUTOBALANCE_HOST    // 2
                                                : KI_PROCESS_AUTOBALANCE_CLIENT; // 3
                StartProcess(leProcess, lpfCompletionCallback, liUserData);
                return 1; // X360 returns r3==`this`; caller truthiness-tests only -> non-zero token
            }
            case KI_LAUNCH_TYPE_FFA: // 14
                StartProcess(KI_PROCESS_FFA, lpfCompletionCallback, liUserData);
                return 1; // X360 returns r3==`this`; caller truthiness-tests only -> non-zero token
            case KI_LAUNCH_TYPE_COOP: // 17
                StartProcess(KI_PROCESS_COOP, lpfCompletionCallback, liUserData);
                return 1; // X360 returns r3==`this`; caller truthiness-tests only -> non-zero token
            default:
                if (lpfCompletionCallback != 0)
                    return lpfCompletionCallback(1, liUserData);
                return 1; // X360 returns r3==`this`; caller truthiness-tests only -> non-zero token
        }
    }

    // ---------------------------------------------------------------------------------
    // SetNextAction @ 0x8254BA70
    //   Drives one action of the current process. If the action cursor reached the end of the
    //   process (index == 2) or the slot holds the no-action sentinel, the process is finished:
    //   reset and fire the completion callback with success. Otherwise run the action; on success
    //   advance the cursor, on failure reset and fire the completion callback with failure.
    // ---------------------------------------------------------------------------------
    int TeamSelectionManager::SetNextAction()
    {
        const s32 liActionCursor = miActionIndex;
        const s32 leAction = maProcessActions[KI_ACTIONS_PER_PROCESS * meCurrentProcess + liActionCursor];

        if (liActionCursor == KI_ACTIONS_PER_PROCESS || leAction == KI_NO_ACTION)
        {
            CompletionCallback lpfCallback = mpfCompletionCallback;
            const s32          liUserData  = mCompletionUserData;
            meUpdateState         = 0;
            mpfCompletionCallback = 0;
            mCompletionUserData   = 0;
            if (lpfCallback == 0)
                return 1; // X360 returns r3==`this`; caller truthiness-tests only -> non-zero token
            return lpfCallback(1, liUserData);
        }

        const int liResult = maActionFunctions[leAction](this);
        if (liResult != 0)
        {
            ++miActionIndex;
            return liResult;
        }

        CompletionCallback lpfCallback = mpfCompletionCallback;
        const s32          liUserData  = mCompletionUserData;
        meUpdateState         = 0;
        mpfCompletionCallback = 0;
        mCompletionUserData   = 0;
        if (lpfCallback != 0)
            return lpfCallback(0, liUserData);
        return liResult;
    }

    // ---------------------------------------------------------------------------------
    // ProcessAfterSimulation @ 0x8254B8B0
    //   Runs the current update-state driver (asserting it is present).
    // ---------------------------------------------------------------------------------
    int TeamSelectionManager::ProcessAfterSimulation()
    {
        CGS_ASSERT(maUpdateFunctions[meUpdateState] != 0,
                   "No update function supplied for the current process substate");
        return maUpdateFunctions[meUpdateState](this);
    }

    // ---------------------------------------------------------------------------------
    // ProcessBeforeSimulation @ 0x825656D8
    //   Substate 0: if a final selection arrived, advance to substate 1.
    //   Substate 1: copy the active-race-car team ids into a stack event, publish it onto the
    //               OutputBuffer's network event queue (event id 21, 32 bytes), advance to
    //               substate 2, and assert every active-race-car slot now holds a team.
    //   Substate >= 2: nothing.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::ProcessBeforeSimulation( BrnNetworkModuleIO::OutputBuffer* lpOutput )
    {
        if (meSubState < 1)
        {
            if (mbReceivedFinalSelection)
            {
                meSubState               = 1;
                mbReceivedFinalSelection = false;
            }
            return;
        }
        if (meSubState != 1)
            return;

        // Copy each active-race-car slot's team id into the 32-byte event payload (8 s32 teams).
        s32 laTeams[KI_MAX_ACTIVE_RACE_CARS];
        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
            laTeams[liCar] = maActiveRaceCarTeam[liCar].miTeam;

        CGS_ASSERT(lpOutput != 0, "lpOutput");
        CGS_ASSERT(lpOutput->GetNetworkEventQueue() != 0, "lpOutput->GetNetworkEventQueue()");

        AsConcreteQueue(lpOutput->GetNetworkEventQueue())->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(laTeams),
            KI_OUTEVENT_TEAM_SELECTION, KI_OUTEVENT_TEAM_SELECTION_SIZE);

        meSubState = 2;

        // Assert every active-race-car slot now holds a (non-zero) team.
        s32 liNumOnTeams = 0;
        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            if (maActiveRaceCarTeam[liCar].miTeam != 0)
                ++liNumOnTeams;
        }
        CGS_ASSERT(miNumPlayersAssigned == liNumOnTeams,
                   "Outputting teams when some players are not on one");
    }

    // ---------------------------------------------------------------------------------
    // ActionAssignCoopStuntRunTeams @ 0x8254BC48
    //   Clears the active-race-car team table, then assigns every in-game player to the single
    //   co-op team (KI_COOP_TEAM), keyed by their active-race-car index, and arms the output.
    // ---------------------------------------------------------------------------------
    int TeamSelectionManager::ActionAssignCoopStuntRunTeams()
    {
        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");
        CGS_ASSERT(mpNetworkManager->GetServerInterface() != 0, "mpNetworkManager->GetServerInterface()");

        CgsNetwork::PlayerManager*       lpPlayerManager = mpNetworkManager->GetPlayerManager();
        CgsNetwork::ServerInterfaceGames* lpGames =
            mpNetworkManager->GetServerInterface()->GetGameComponent();

        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            maActiveRaceCarTeam[liCar].mPlayerID = KI_INVALID_PLAYER_ID;
            maActiveRaceCarTeam[liCar].miTeam    = 0;
        }
        miNumPlayersAssigned = 0;

        NetworkPlayerID lPlayerID = KI_INVALID_PLAYER_ID;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
        {
            if (lpGames->IsPlayerInGameByID(lPlayerID))
            {
                const s32 liActiveRaceCarIndex = mpNetworkManager->GetActiveRaceCarIndex(lPlayerID);
                CGS_ASSERT(liActiveRaceCarIndex != -1,
                           "Trying to assign a team to a player with no race car index");

                maActiveRaceCarTeam[liActiveRaceCarIndex].miTeam    = KI_COOP_TEAM;
                maActiveRaceCarTeam[liActiveRaceCarIndex].mPlayerID = lPlayerID;
                ++miNumPlayersAssigned;
            }
        }

        meSubState = 1;
        return 1;
    }

    // ---------------------------------------------------------------------------------
    // ActionBroadcastFinalTeamSelection @ 0x82557178
    //   For every finalised player that is NOT the local player, find its team-selection slot and
    //   send it the final team table (the active-race-car team table reinterpreted as the message
    //   PlayerTeamInfo[] payload; numTeams 1, autobalance false, finalSelection true). Arms output.
    // ---------------------------------------------------------------------------------
    int TeamSelectionManager::ActionBroadcastFinalTeamSelection()
    {
        CGS_ASSERT(mpNetworkManager != 0, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetPlayerManager() != 0, "mpNetworkManager->GetPlayerManager()");

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
        const NetworkPlayerID lLocalPlayerID = lpPlayerManager->GetLocalPlayerID();

        // The active-race-car team table IS the wire PlayerTeamInfo[] payload (both are the 8-byte
        // {NetworkPlayerID, s32 team} record); reinterpret it for PrepareForSend.
        const TeamSelectMessage::PlayerTeamInfo* lpaPlayerTeamInfo =
            reinterpret_cast<const TeamSelectMessage::PlayerTeamInfo*>(maActiveRaceCarTeam);

        const u16 lu16Frame = static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu);

        NetworkPlayerID lPlayerID = KI_INVALID_PLAYER_ID;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID,
                                                CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            const bool lbIsLocalPlayer = (lLocalPlayerID == lPlayerID) && (lPlayerID != KI_INVALID_PLAYER_ID);
            if (!lbIsLocalPlayer)
            {
                TeamSelectionData* lpTeamSelectionData = GetTeamSelectionDataEntry(lPlayerID);
                CGS_ASSERT(lpTeamSelectionData != 0, "lpTeamSelectionData");

                lpTeamSelectionData->mMessageSend.PrepareForSend(
                    lu16Frame, lpaPlayerTeamInfo, miNumPlayersAssigned,
                    /*liNumTeams*/ 1, /*lbAutobalance*/ false, /*lbFinalSelection*/ true);
            }
        }

        meSubState = 1;
        return 1;
    }

    // ---------------------------------------------------------------------------------
    // _TeamSelectionMessageArrivedCallback @ 0x8254BE78
    //   Remote receipt of the host's final team table: Retrieve the message, and (only for the
    //   FINAL selection) clear the active-race-car team table, then for each player in the table
    //   look its active-race-car index up and store {player id, team} there. Latch the count and
    //   the "received final selection" flag.
    // ---------------------------------------------------------------------------------
    void TeamSelectionManager::_TeamSelectionMessageArrivedCallback(
        CgsNetwork::ReliableMessage* lpMessage,
        NetworkPlayerID /*lSendingPlayerID*/,
        void* lpUserData )
    {
        CGS_ASSERT(lpMessage != 0, "lpTeamSelectMessage");

        TeamSelectMessage* lpTeamSelectMessage = reinterpret_cast<TeamSelectMessage*>(lpMessage);
        TeamSelectMessage::PlayerTeamInfo laPlayerTeamInfo[KI_TEAM_SELECT_MAX_PLAYERS];
        s32  liNumPlayers   = 0;
        s32  liNumTeams     = 0;
        bool lbAutobalance  = false;
        bool lbFinalSelection = false;

        const bool lbRetrieved = lpTeamSelectMessage->Retrieve(
            laPlayerTeamInfo, &liNumPlayers, &liNumTeams, &lbAutobalance, &lbFinalSelection);
        CGS_ASSERT(lbRetrieved,
                   "lpTeamSelectMessage->Retrieve( laPlayerTeamInfo, &liNumPlayers, &liNumTeams, &lbAutobalance, &lbFinalSelection )");
        CGS_ASSERT(lbFinalSelection, "lbFinalSelection");

        if (lbFinalSelection)
        {
            TeamSelectionManager* lpTeamSelectionManager =
                reinterpret_cast<TeamSelectionManager*>(lpUserData);
            CGS_ASSERT(lpTeamSelectionManager != 0, "lpTeamSelectionManager");

            lpTeamSelectionManager->miNumPlayersAssigned = 0;
            for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
            {
                lpTeamSelectionManager->maActiveRaceCarTeam[liCar].miTeam    = 0;
                lpTeamSelectionManager->maActiveRaceCarTeam[liCar].mPlayerID = KI_INVALID_PLAYER_ID;
            }

            for (s32 liPlayer = 0; liPlayer < liNumPlayers; ++liPlayer)
            {
                CGS_ASSERT(lpTeamSelectionManager->mpNetworkManager != 0,
                           "lpTeamSelectionManager->mpNetworkManager");

                const s32 liActiveRaceCarIndex =
                    lpTeamSelectionManager->mpNetworkManager->GetActiveRaceCarIndex(laPlayerTeamInfo[liPlayer].mPlayerID);
                CGS_ASSERT(liActiveRaceCarIndex != -1,
                           "Trying to assign a team to a player with no race car index");

                ActiveRaceCarTeam& lSlot =
                    lpTeamSelectionManager->maActiveRaceCarTeam[liActiveRaceCarIndex];
                lSlot.mPlayerID = laPlayerTeamInfo[liPlayer].mPlayerID;
                lSlot.miTeam    = laPlayerTeamInfo[liPlayer].miTeam;
            }

            lpTeamSelectionManager->miNumPlayersAssigned     = liNumPlayers;
            lpTeamSelectionManager->mbReceivedFinalSelection = true;
        }
    }

    // ---- thunks bridging the (TeamSelectionManager*) table signature to the methods -----------
    // The X360 stores each method's address directly into the function-pointer tables (the C ABI
    // there passes `this` as the first integer arg, so a free `(TeamSelectionManager*)` thunk is
    // the faithful PC equivalent; it has no body of its own beyond the forward).
    int TeamSelectionManager::UpdateIdleNoOp( TeamSelectionManager* /*lpThis*/ ) { return 1; }
    int TeamSelectionManager::UpdateWaitIdleThunk( TeamSelectionManager* lpThis ) { return lpThis->UpdateWaitIdle(); }
    int TeamSelectionManager::ActionAssignFFAThunk( TeamSelectionManager* lpThis ) { return lpThis->ActionAssignFFAStuntRunTeams(); }
    int TeamSelectionManager::ActionAssignCoopThunk( TeamSelectionManager* lpThis ) { return lpThis->ActionAssignCoopStuntRunTeams(); }
    int TeamSelectionManager::ActionAutobalanceThunk( TeamSelectionManager* lpThis ) { return lpThis->ActionAutobalanceStuntRunTeams(); }
    int TeamSelectionManager::ActionBroadcastFinalThunk( TeamSelectionManager* lpThis ) { return lpThis->ActionBroadcastFinalTeamSelection(); }
    int TeamSelectionManager::ActionWaitFinalThunk( TeamSelectionManager* lpThis ) { return lpThis->ActionWaitFinalTeamSelection(); }

    // Uncalled layout pin. The committed TeamSelectMessage base is reconstructed larger than the
    // X360 original, so the ABSOLUTE byte offsets do NOT survive onto the LLP64 gate host; only the
    // pointer-INVARIANT intra-record facts the asm relies on are pinned -- the 8-byte
    // {NetworkPlayerID, s32} stride of both the team table and the broadcast PlayerTeamInfo payload
    // (they alias each other in ActionBroadcastFinalTeamSelection), and the per-slot field order.
    void TeamSelectionManager_AssertLayout()
    {
        static_assert(sizeof(TeamSelectionManager::ActiveRaceCarTeam) == 8,
                      "ActiveRaceCarTeam is an 8-byte {NetworkPlayerID, s32 team} pair");
        static_assert(offsetof(TeamSelectionManager::ActiveRaceCarTeam, mPlayerID) == 0,
                      "ActiveRaceCarTeam.mPlayerID @ +0x00");
        static_assert(offsetof(TeamSelectionManager::ActiveRaceCarTeam, miTeam) == 4,
                      "ActiveRaceCarTeam.miTeam @ +0x04");
        // The broadcast aliases maActiveRaceCarTeam onto the message PlayerTeamInfo payload, so the
        // two records must share their size + field order.
        static_assert(sizeof(TeamSelectMessage::PlayerTeamInfo) == sizeof(TeamSelectionManager::ActiveRaceCarTeam),
                      "ActiveRaceCarTeam aliases the wire PlayerTeamInfo record in the broadcast");
        static_assert(offsetof(TeamSelectionManager::TeamSelectionData, mMessageRecv)
                          == offsetof(TeamSelectionManager::TeamSelectionData, mMessageSend) + sizeof(TeamSelectMessage),
                      "the recv message immediately follows the send message in a TeamSelectionData slot");
    }
} // namespace BrnNetwork
