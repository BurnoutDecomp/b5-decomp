// ===================================================================================
// BrnNetwork::NetworkInviteManager -- behaviour
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkInviteManager.cpp
//
// The nine X360-recovered members of the Xbox-Live invite/join state machine. Layout, enum
// values and every constant below are grounded in the recovered asm (see the owning header for
// the per-offset map). The four further DWARF-declared members (Release/Update/JoinGameComplete/
// GetGameID and the private ProcessGameStateActions/SetPreparingForInvite) have no recovered X360
// body in this TU and are declared-only in the header -- their bodies are resolved by their own
// slices / inlined at the call sites; no body is fabricated here.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"

#include "GameSource/Network/BrnNetworkModule.h"   // BrnNetworkModule::GetNetworkManager / GetGameEventQueue / GetNetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"  // BrnNetworkManager::GetServerInterface
#include "GameSource/Network/BrnServerInterface.h" // BrnServerInterface::GetStatus / GetPlayerInfoComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // ServerInterfacePlayerInfo::GetPlayerInfoByName
#include "GameShared/GameClasses/System/CgsHardwareInit.h" // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16> / <14000,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

#include <cstring>   // std::memcpy (X360 StartInviteOrJoin / DownloadPlayersGameIDFromServer)

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

        // The "idle" status the get-game-id sub-machine waits for from the server interface and the
        // player-info query (X360 GetStatus(...) == 2; BrnServerInterface::E_STATUS_IDLE == 2). The
        // queried component index is the X360 immediate li r4, 2.
        const s32 KI_SERVER_INTERFACE_COMPONENT = 2;
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

    // X360 0x82548AA8 -- remember the player whose game id we want and kick the get-game-id
    // sub-machine off from its start state.
    void NetworkInviteManager::DownloadPlayersGameIDFromServer(const PlayerName* lpPlayerName)
    {
        CGS_ASSERT(meGetGameIDState == E_GET_ID_STATE_IDLE, "meGetGameIDState == E_GET_ID_STATE_IDLE");
        CGS_ASSERT(lpPlayerName != nullptr, "lpPlayerName");
        // X360 checks the first name byte is non-zero (the inlined PlayerName::IsEmpty()).
        CGS_ASSERT(lpPlayerName->macName[0] != '\0', "!lpPlayerName->IsEmpty()");

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
    //   IDLE (2): nothing to do (X360 early-out at LABEL_3).
    void NetworkInviteManager::UpdateGettingGameID()
    {
        // X360: `if (v2) { if (v2 != 1) return; goto LABEL_12; }` -- any state other than 0/1 (i.e.
        // IDLE) returns immediately; state 1 jumps straight to the LABEL_12 body (skipping the first
        // status gate + the player-info query).
        if (meGetGameIDState != E_GET_ID_STATE_START &&
            meGetGameIDState != E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO)
        {
            return;
        }

        bool lbReachedLabel12 = (meGetGameIDState == E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO);

        if (meGetGameIDState == E_GET_ID_STATE_START)
        {
            CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                       "mpNetworkModule->GetNetworkManager()");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetServerInterface() != nullptr,
                       "mpNetworkModule->GetNetworkManager()->GetServerInterface()");

            BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
            if (lpNetworkManager->GetServerInterface()->GetStatus(KI_SERVER_INTERFACE_COMPONENT)
                == BrnServerInterface::E_STATUS_IDLE)
            {
                // Server interface is idle: issue the by-name player-info query into mPlayerInfoData,
                // then fall into the LABEL_12 body.
                BrnNetworkManager* lpQueryManager = mpNetworkModule->GetNetworkManager();
                // BrnNetwork::PlayerName and CgsNetwork::PlayerName share the same 16-byte char[16]
                // layout (BrnNetworkSharedIO.h mirrors the committed CgsNetwork::PlayerName); the
                // X360 passes the raw this+0x1A0 pointer to the Cgs query.
                lpQueryManager->GetServerInterface()->GetPlayerInfoComponent()->GetPlayerInfoByName(
                    reinterpret_cast<const CgsNetwork::PlayerName*>(&mPlayerNameToGetGameID),
                    reinterpret_cast<CgsNetwork::ServerInterfacePlayerInfoDataBase*>(maPlayerInfoData));
                lbReachedLabel12 = true;
            }
        }

        if (lbReachedLabel12)
        {
            // LABEL_12: latch WAITING_FOR_PLAYER_INFO, then fall to IDLE once the server interface
            // reports idle.
            meGetGameIDState = E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO; // *(v1 + 432) = 1

            CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr,
                       "mpNetworkModule->GetNetworkManager()");
            CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetServerInterface() != nullptr,
                       "mpNetworkModule->GetNetworkManager()->GetServerInterface()");

            BrnNetworkManager* lpWaitManager = mpNetworkModule->GetNetworkManager();
            if (lpWaitManager->GetServerInterface()->GetStatus(KI_SERVER_INTERFACE_COMPONENT)
                == BrnServerInterface::E_STATUS_IDLE)
            {
                meGetGameIDState = E_GET_ID_STATE_IDLE;  // *(v1 + 432) = 2
            }
        }
    }
}
