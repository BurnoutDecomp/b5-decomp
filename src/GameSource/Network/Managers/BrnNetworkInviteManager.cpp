// ===================================================================================
// BrnNetwork::NetworkInviteManager -- behaviour
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkInviteManager.cpp
//
// The nine X360-recovered members of the Xbox-Live invite/join state machine. Layout, enum
// values and every constant below are grounded in the recovered asm (see the owning header for
// the per-offset map). Release shares Prepare's body; JoinGameComplete, GetGameID and
// SetPreparingForInvite are header inlines; ProcessGameStateActions has no body of its own.
// ===================================================================================
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkInviteManager.cpp"
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"

#include "GameSource/Network/BrnNetworkModule.h"   // BrnNetworkModule::GetNetworkManager / GetGameEventQueue / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"  // BrnNetworkManager::GetServerInterface
#include "GameSource/Network/BrnServerInterface.h" // BrnServerInterface::GetStatus / GetPlayerInfoComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // ServerInterfacePlayerInfo::GetPlayerInfoByName
#include "GameShared/GameClasses/System/CgsHardwareInit.h" // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16> / <14000,16>
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"             // CgsDev::StrStream (unknown-state assert)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // IsLocalPlayerInGame / GetGameID
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"         // StateManager::GetState
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"              // NetworkOutInviteFailed

#include <cstring>   // std::memcpy (X360 StartInviteOrJoin / DownloadPlayersGameIDFromServer)

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkInviteManager.cpp".

namespace BrnNetwork
{
    namespace
    {
        // The X360 module accessors return forward-declared queue types; the concrete queues are
        // the variable-event queues the AddEvent calls instantiate. The game event queue is the
        // 1536/16 queue (CompleteInvite/LogInComplete) and the network event queue is the 14000/16
        // queue (StartInviteOrJoin) -- both grounded by the demangled AddEvent template arguments
        // in the asm (VariableEventQueue<1536,16>::AddEvent / VariableEventQueue<14000,16>::AddEvent).
        typedef CgsModule::VariableEventQueue<1536, 16>  GameEventQueueConcrete;
        typedef CgsModule::VariableEventQueue<14000, 16> NetworkEventQueueConcrete;

        inline GameEventQueueConcrete* AsGameQueue(BrnGameState::GameStateModuleIO::GameEventQueue* lpQueue)
        {
            return reinterpret_cast<GameEventQueueConcrete*>(lpQueue);
        }

        inline NetworkEventQueueConcrete* AsNetworkQueue(BrnNetworkModuleIO::NetworkEventQueue* lpQueue)
        {
            return reinterpret_cast<NetworkEventQueueConcrete*>(lpQueue);
        }

        // Game-side event-type tag for the "invite complete" notification (X360 li r5, 0x3E in
        // CompleteInvite/LogInComplete). The payload is a single bool (success) appended with
        // size 1 (li r6, 1).
        const s32 KI_INVITE_COMPLETE_EVENT_TYPE = 0x3E;  // 62

        // Network-side event-type tag for the "invite failed" notification (X360 li r5, 8 in
        // StartInviteOrJoin). The payload is a single s32 reason code appended with size 4
        // (li r6, 4).
        const s32 KI_INVITE_FAILED_EVENT_TYPE = 8;

        // The reason code StartInviteOrJoin stores into the failed-event payload when the hard disk
        // is unavailable (X360 li r11, 4 ; stw r11, payload). Grounded immediate -- not named as an
        // enum member since no enum for it is recovered.
        const s32 KI_INVITE_FAILED_REASON_NO_HARD_DISK = 4;

        // The reason code Update posts when the local player is already in the target's game
        // (the console immediate 1).
        const s32 KI_INVITE_FAILED_REASON_SAME_GAME = 1;
    }

    // X360 0x825488B0 -- store the owning module and bring both state machines to their idle/not-in-
    // invite values.
    void NetworkInviteManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        mpNetworkModule  = lpNetworkModule;           // result[110] @ +0x1B8
        meInviteState    = E_INVITE_STATE_COUNT;      // result[109] = 7 @ +0x1B4
        meGetGameIDState = E_GET_ID_STATE_IDLE;       // result[108] = 2 @ +0x1B0
    }

    // X360 0x825488C8 -- arm the get-game-id sub-machine to idle; always succeeds.
    bool NetworkInviteManager::Prepare()
    {
        meGetGameIDState = E_GET_ID_STATE_IDLE;       // *(a1 + 432) = 2
        return true;                                  // return 1
    }

    // The release step re-arms the get-game-id sub-machine exactly as Prepare does (the network
    // manager's release stage reaches the same body).
    bool NetworkInviteManager::Release()
    {
        meGetGameIDState = E_GET_ID_STATE_IDLE;
        return true;
    }

    // X360 0x825488E0 -- reset both state machines to their not-in-invite / idle values.
    void NetworkInviteManager::Destruct()
    {
        meInviteState    = E_INVITE_STATE_COUNT;      // *(result + 436) = 7
        meGetGameIDState = E_GET_ID_STATE_IDLE;       // *(result + 432) = 2
    }

    // X360 0x82548A90 -- an invite/join is in progress whenever the state is not the "count"
    // sentinel.
    bool NetworkInviteManager::IsInInvite() const
    {
        return meInviteState != E_INVITE_STATE_COUNT; // *(a1 + 436) != 7
    }

    // Drive the invite: fetch the target player's game id, check the local player
    // is not already in that game, then (once the state manager is not matchmaking) join the
    // target's session. The get-game-id sub-machine is ticked on every path.
    void NetworkInviteManager::Update()
    {
        switch (meInviteState)
        {
        case E_INVITE_STATE_PREPARING_FOR_INVITE:
        case E_INVITE_STATE_LOGGING_IN_TO_SERVER:
        case E_INVITE_STATE_JOINING_GAME_SESSION:
        case E_INVITE_STATE_COUNT:
            break;

        case E_INVITE_STATE_GETTING_GAME_ID:
            DownloadPlayersGameIDFromServer(&mInviteOrJoinParams.mPlayerName);
            // fall through
        case E_INVITE_STATE_WAITING_FOR_GAME_ID:
            meInviteState = E_INVITE_STATE_WAITING_FOR_GAME_ID;
            if (!GetGameID(&mInviteOrJoinParams.miGameID))
            {
                break;
            }
            // fall through
        case E_INVITE_STATE_VERIFYING_GAME_ID:
        {
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()", CGS_ARTIST_FILE, 171);
            BrnServerInterface* lpServerInterface = mpNetworkModule->GetNetworkManager()->GetServerInterface();
            CGS_ASSERT_AT(lpServerInterface, "lpServerInterface", CGS_ARTIST_FILE, 173);
            CGS_ASSERT_AT(lpServerInterface->GetGameComponent(), "lpServerInterface->GetGameComponent()", CGS_ARTIST_FILE, 174);

            s32 liCurrentGameID = -1;
            if (lpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
            {
                liCurrentGameID = lpServerInterface->GetGameComponent()->GetGameID();
            }

            if (liCurrentGameID == mPlayerInfoData.GetGameID())
            {
                // Already in the target's game: fail the invite.
                BrnNetworkModuleIO::NetworkOutInviteFailed lInviteFailedEvent;
                lInviteFailedEvent.meFailReason = KI_INVITE_FAILED_REASON_SAME_GAME;
                AsNetworkQueue(mpNetworkModule->GetNetworkEventQueue())
                    ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInviteFailedEvent),
                               KI_INVITE_FAILED_EVENT_TYPE, 4);
                CompleteInvite(false);
                break;
            }
        }
            // fall through
        case E_INVITE_STATE_START_JOIN_GAME_SESSION:
            meInviteState = E_INVITE_STATE_START_JOIN_GAME_SESSION;
            CGS_ASSERT_AT(mpNetworkModule, "mpNetworkModule", CGS_ARTIST_FILE, 202);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()", CGS_ARTIST_FILE, 203);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager()->GetStateManager(),
                       "mpNetworkModule->GetNetworkManager()->GetStateManager()", CGS_ARTIST_FILE, 204);
            if (mpNetworkModule->GetNetworkManager()->GetStateManager()->GetState()
                == StateManager::E_STATE_WAIT_MATCHMAKING)
            {
                break;
            }
            meInviteState = E_INVITE_STATE_JOINING_GAME_SESSION;
            mpNetworkModule->GetNetworkManager()->JoinGameSession(mInviteOrJoinParams.macSessionID);
            break;

        default:
            CGS_FAIL_STREAM_AT("Unknown state in NetworkInviteManager: " << static_cast<s32>(meInviteState) << "\n",
                CGS_ARTIST_FILE, 226);
            break;
        }

        UpdateGettingGameID();
    }

    // X360 0x82548AA8 -- remember the player whose game id we want and kick the get-game-id
    // sub-machine off from its start state.
    void NetworkInviteManager::DownloadPlayersGameIDFromServer(const PlayerName* lpPlayerName)
    {
        CGS_ASSERT_AT(meGetGameIDState == E_GET_ID_STATE_IDLE,
            "meGetGameIDState == E_GET_ID_STATE_IDLE", CGS_ARTIST_FILE, 423 /* ARTIST 0x82548AD8 */);

        CGS_ASSERT_AT(lpPlayerName != 0,
            "lpPlayerName", CGS_ARTIST_FILE, 425 /* ARTIST 0x82548AFC */);
        // X360 checks the first name byte is non-zero (the inlined PlayerName::IsEmpty()).
        CGS_ASSERT_AT(lpPlayerName->macName[0] != '\0',
            "!lpPlayerName->IsEmpty()", CGS_ARTIST_FILE, 426 /* ARTIST 0x82548B24 */);


        std::memcpy(&mPlayerNameToGetGameID, lpPlayerName, sizeof(PlayerName)); // memcpy(a1 + 416, a2, 16)
        meGetGameIDState = E_GET_ID_STATE_START;       // *(a1 + 432) = 0
    }

    // X360 0x82563210 -- begin an invite/join. With the hard disk available, latch the params and
    // move to LOGGING_IN_TO_SERVER; otherwise post a NetworkOutInviteFailed (no-disk) event.
    void NetworkInviteManager::StartInviteOrJoin(BrnNetworkModuleIO::InviteOrJoinParams lInviteOrJoinParams)
    {
        if (CgsSystem::HardwareInit::IsHardDiskAvailable())
        {
            meInviteState = E_INVITE_STATE_LOGGING_IN_TO_SERVER;  // *(v14 + 436) = 1
            std::memcpy(&mInviteOrJoinParams, &lInviteOrJoinParams, 0x9C); // memcpy(v14, &a8, 156)
        }
        else
        {
            // No hard disk: fail the invite immediately with the no-disk reason code.
            struct NetworkOutInviteFailed
            {
                s32 miReason;
            } lInviteFailedEvent;
            lInviteFailedEvent.miReason = KI_INVITE_FAILED_REASON_NO_HARD_DISK; // v18[0] = 4

            AsNetworkQueue(mpNetworkModule->GetNetworkEventQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInviteFailedEvent),
                           KI_INVITE_FAILED_EVENT_TYPE, 4);
        }
    }

    // X360 0x8256E028 -- the login attempt completed. Success while logging in advances to the
    // join-session start; otherwise (failure, or a later login drop) the invite is abandoned with
    // an InviteComplete(false) game event.
    void NetworkInviteManager::LogInComplete(bool lbLoggedIn)
    {
        if (lbLoggedIn && meInviteState == E_INVITE_STATE_LOGGING_IN_TO_SERVER)
        {
            meInviteState = E_INVITE_STATE_START_JOIN_GAME_SESSION; // *(result + 436) = 5
        }
        else if (meInviteState != E_INVITE_STATE_COUNT)
        {
            // 1-byte event payload: the invite-complete success flag (false here).
            u8 lInviteCompleteEvent[1];
            lInviteCompleteEvent[0] = 0;                 // v8[0] = 0
            meInviteState = E_INVITE_STATE_COUNT;        // *(v3 + 436) = 7
            AsGameQueue(mpNetworkModule->GetGameEventQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(lInviteCompleteEvent),
                           KI_INVITE_COMPLETE_EVENT_TYPE, 1);
        }

        // The X360 re-tests the (possibly just-updated) state: a non-login means the invite must be
        // abandoned if it is still live.
        if (!lbLoggedIn && meInviteState != E_INVITE_STATE_COUNT)
        {
            u8 lInviteCompleteEvent[1];
            lInviteCompleteEvent[0] = 0;                 // v8[0] = 0
            meInviteState = E_INVITE_STATE_COUNT;        // *(v3 + 436) = 7
            AsGameQueue(mpNetworkModule->GetGameEventQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(lInviteCompleteEvent),
                           KI_INVITE_COMPLETE_EVENT_TYPE, 1);
        }
    }

    // X360 0x82568F70 -- finish the current invite (once): mark it not-in-invite and notify the
    // game side with the success flag.
    void NetworkInviteManager::CompleteInvite(bool lbSuccess)
    {
        if (meInviteState != E_INVITE_STATE_COUNT)
        {
            // 1-byte event payload: the invite-complete success flag.
            u8 lInviteCompleteEvent[1];
            lInviteCompleteEvent[0] = static_cast<u8>(lbSuccess); // v5[0] = a2
            meInviteState = E_INVITE_STATE_COUNT;                 // *(v2 + 436) = 7
            AsGameQueue(mpNetworkModule->GetGameEventQueue())
                ->AddEvent(reinterpret_cast<const CgsModule::Event*>(lInviteCompleteEvent),
                           KI_INVITE_COMPLETE_EVENT_TYPE, 1);
        }
    }

    // X360 0x825488F8 -- drive the download-game-id sub-machine.
    //   START (0): wait for the server interface to go idle; once idle, issue GetPlayerInfoByName
    //              and FALL THROUGH into the WAITING_FOR_PLAYER_INFO body below.
    //   WAITING_FOR_PLAYER_INFO (1): (re-)latch state 1, then once the server interface reports idle
    //              fall to IDLE (2).
    //   IDLE (2): nothing to do. A switch with the fall-through: ARTIST 0x82548928 dispatches
    //   with cmplwi 1 / blt / beq and reloads the network manager at every use.
    void NetworkInviteManager::UpdateGettingGameID()
    {
        switch (meGetGameIDState)
        {
        case E_GET_ID_STATE_START:
            CGS_ASSERT_AT(mpNetworkModule,
                "mpNetworkModule",
                CGS_ARTIST_FILE, 250 /* ARTIST 0x8254895C */);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager(),
                "mpNetworkModule->GetNetworkManager()",
                CGS_ARTIST_FILE, 251 /* ARTIST 0x82548984 */);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager()->GetServerInterface(),
                "mpNetworkModule->GetNetworkManager()->GetServerInterface()",
                CGS_ARTIST_FILE, 252 /* ARTIST 0x825489AC */);

            if (mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_PLAYER_INFO)
                != BrnServerInterface::E_STATUS_IDLE)
            {
                break;
            }
            // BrnNetwork::PlayerName and CgsNetwork::PlayerName share the same 16-byte char[16]
            // layout (BrnNetworkSharedIO.h mirrors the committed CgsNetwork::PlayerName); the
            // X360 passes the raw this+0x1A0 pointer to the Cgs query.
            mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetPlayerInfoComponent()->GetPlayerInfoByName(
                reinterpret_cast<const CgsNetwork::PlayerName*>(&mPlayerNameToGetGameID),
                &mPlayerInfoData);
            // fall through
        case E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO:
            meGetGameIDState = E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO;

            CGS_ASSERT_AT(mpNetworkModule,
                "mpNetworkModule",
                CGS_ARTIST_FILE, 268 /* ARTIST 0x82548A0C */);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager(),
                "mpNetworkModule->GetNetworkManager()",
                CGS_ARTIST_FILE, 269 /* ARTIST 0x82548A34 */);
            CGS_ASSERT_AT(mpNetworkModule->GetNetworkManager()->GetServerInterface(),
                "mpNetworkModule->GetNetworkManager()->GetServerInterface()",
                CGS_ARTIST_FILE, 270 /* ARTIST 0x82548A5C */);

            if (mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_PLAYER_INFO)
                == BrnServerInterface::E_STATUS_IDLE)
            {
                meGetGameIDState = E_GET_ID_STATE_IDLE;
            }
            break;
        }
    }
}
