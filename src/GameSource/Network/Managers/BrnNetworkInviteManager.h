#pragma once

// ===================================================================================
// BrnNetwork::NetworkInviteManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkInviteManager.{h,cpp}
//
// The Xbox-Live "invite / join a friend's game" state machine owned by BrnNetworkManager.
// When the local player accepts an invite (or asks to join a buddy) the manager copies the
// invite/join parameters in, logs in to the online server, downloads the target player's
// current game id from the server's player-info component, verifies it, then starts joining
// that game session. Failures at any step post a NetworkOutInviteFailed event onto the network
// event queue and an InviteComplete(success?) event onto the game event queue.
//
// Layout is recovered from the X360 Construct/Destruct asm (0x825488B0 / 0x825488E0) and the
// member accesses across the sibling functions (all offsets are absolute from `this`):
//
//   +0x000 (160) mInviteOrJoinParams      BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams.
//                                          StartInviteOrJoin memcpy's the 156-byte (0x9C) params
//                                          block to this+0 (X360 `memcpy(this, &params, 156)`);
//                                          the next member sits at +0xA0 so the embedded params +
//                                          its tail padding occupy 160 bytes.
//   +0x0A0 (256) mPlayerInfoData          BrnNetwork::PlayerInfoData -- the per-player info record
//                                          the server-interface player-info component fills in.
//                                          UpdateGettingGameID passes &this+0xA0 as the
//                                          ServerInterfacePlayerInfoDataBase* out-param of
//                                          GetPlayerInfoByName. The full record's layout is homed
//                                          in BrnNetworkPlayerInfoData.{h,cpp}; here the 256-byte
//                                          span (ends at the player-name @ +0x1A0) is reserved so
//                                          later members keep their pinned offsets.
//   +0x1A0 (16)  mPlayerNameToGetGameID   BrnNetwork::PlayerName (16B, X360-authoritative width).
//                                          DownloadPlayersGameIDFromServer memcpy's the caller's
//                                          name here (this+0x1A0, 16 bytes); UpdateGettingGameID
//                                          passes &this+0x1A0 as the lpcPlayerName lookup key.
//   +0x1B0 (4)   meGetGameIDState         EGetGameIDState (X360 stw/lwz 0x1B0). Construct stores 2
//                                          (IDLE), Prepare stores 2, DownloadPlayers... stores 0
//                                          (START), UpdateGettingGameID drives 0->1->2.
//   +0x1B4 (4)   meInviteState            EInviteState (X360 stw/lwz 0x1B4). Construct stores 7
//                                          (COUNT == "not in invite"), Destruct stores 7,
//                                          LogInComplete/CompleteInvite drive the rest.
//   +0x1B8 (4)   mpNetworkModule          BrnNetwork::BrnNetworkModule* (Construct a2, X360 stw
//                                          0x1B8; every sibling loads it from 0x1B8).
//
// Enum values are DWARF-authoritative (BrnNetworkInviteManager.h:103/116) and corroborated by the
// X360 stores/compares (Construct stores 7 into meInviteState and 2 into meGetGameIDState;
// IsInInvite compares meInviteState against 7). Naming follows
// references/CXX_NAMING_CONVENTIONS.md.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"        // InviteOrJoinParams, PlayerName
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h" // BrnNetwork::PlayerInfoData (homed type)

namespace BrnNetwork
{
    class BrnNetworkModule;

    class NetworkInviteManager
    {
    public:
        // The high-level invite/join state machine. Values are DWARF-authoritative
        // (BrnNetworkInviteManager.h:103) and grounded by the X360 stores (Construct stores 7;
        // IsInInvite/CompleteInvite/LogInComplete compare against 7; LogInComplete stores 5).
        enum EInviteState
        {
            E_INVITE_STATE_PREPARING_FOR_INVITE   = 0,
            E_INVITE_STATE_LOGGING_IN_TO_SERVER   = 1,
            E_INVITE_STATE_GETTING_GAME_ID        = 2,
            E_INVITE_STATE_WAITING_FOR_GAME_ID    = 3,
            E_INVITE_STATE_VERIFYING_GAME_ID      = 4,
            E_INVITE_STATE_START_JOIN_GAME_SESSION = 5,
            E_INVITE_STATE_JOINING_GAME_SESSION   = 6,
            E_INVITE_STATE_COUNT                  = 7,
        };

        // The "download the target player's game id" sub state machine. Values are
        // DWARF-authoritative (BrnNetworkInviteManager.h:116) and grounded by the X360 stores in
        // UpdateGettingGameID (0->1 once the server interface is idle, 1->2 once the player-info
        // query is idle) and Construct/Prepare (store 2 == IDLE).
        enum EGetGameIDState
        {
            E_GET_ID_STATE_START                    = 0,
            E_GET_ID_STATE_WAITING_FOR_SERVER_INTERFACE = 0,
            E_GET_ID_STATE_WAITING_FOR_PLAYER_INFO  = 1,
            E_GET_ID_STATE_IDLE                     = 2,
            E_GET_ID_STATE_COUNT                    = 2,
        };

        // X360 0x825488B0 -- bring the manager to its constructed state (called by
        // BrnNetworkManager::Construct). Stores mpNetworkModule, meInviteState = COUNT (7),
        // meGetGameIDState = IDLE (2).
        void Construct(BrnNetworkModule* lpNetworkModule);

        // X360 0x825488C8 -- arm the get-game-id sub-machine to IDLE; returns true (called by
        // BrnNetworkManager::Prepare and ::Release).
        bool Prepare();

        // DWARF BrnNetworkInviteManager.h:58 -- release counterpart of Prepare. Body not recovered
        // in this TU (declared-only; resolved by its own slice / inlined at the caller).
        bool Release();

        // X360 0x825488E0 -- tear the manager back down to its zero/idle state (called by
        // BrnNetworkManager::Destruct). meInviteState = COUNT (7), meGetGameIDState = IDLE (2).
        void Destruct();

        // DWARF BrnNetworkInviteManager.h:66 -- per-frame tick / invite state-machine dispatch.
        // Body not recovered in this TU (no X360 asm in the recovered set); declared-only.
        void Update();

        // DWARF BrnNetworkInviteManager.h:70 -- force the machine into PREPARING_FOR_INVITE. Body
        // not recovered in this TU; declared-only.
        void SetPreparingForInvite();

        // X360 0x82563210 -- begin an invite/join: if the hard disk is available copy the params in
        // and enter LOGGING_IN_TO_SERVER, otherwise post a NetworkOutInviteFailed event (no disk).
        // Called by BrnNetwork::StateManager::ProcessGameStateActions.
        void StartInviteOrJoin(BrnNetworkModuleIO::InviteOrJoinParams lInviteOrJoinParams);

        // X360 0x8256E028 -- the login attempt finished: on success while LOGGING_IN_TO_SERVER
        // advance to START_JOIN_GAME_SESSION (5); on failure (or a login drop) abandon the invite
        // and post an InviteComplete(false) game event. Called from BrnNetwork::StateManager.
        void LogInComplete(bool lbLoggedIn);

        // DWARF BrnNetworkInviteManager.h:85 -- the join attempt finished. Body not recovered in
        // this TU; declared-only.
        void JoinGameComplete(bool lbJoinedGame);

        // X360 0x82548A90 -- true while an invite/join is in progress (meInviteState != COUNT).
        // Called by BrnNetwork::BrnNetworkModule::ProcessBeforeSimulation.
        bool IsInInvite() const;

        // X360 0x82548AA8 -- record the player whose game id we want and arm the get-game-id
        // sub-machine (asserts the sub-machine is idle and the name is non-empty).
        void DownloadPlayersGameIDFromServer(const PlayerName* lpPlayerName);

        // DWARF BrnNetworkInviteManager.h:99 -- read back the downloaded game id. Body not
        // recovered in this TU; declared-only.
        bool GetGameID(s32* lpiGameID);

    private:
        // DWARF BrnNetworkInviteManager.h:137 -- act on the game-state actions. Body not recovered
        // in this TU; declared-only.
        void ProcessGameStateActions();

        // X360 0x82568F70 -- finish the current invite: unless already finished, set
        // meInviteState = COUNT (7) and post an InviteComplete(lbSuccess) event onto the game
        // event queue.
        void CompleteInvite(bool lbSuccess);

        // X360 0x825488F8 -- drive the download-game-id sub-machine: wait for the server interface
        // to go idle, fetch the player info by name, then wait for that query to go idle.
        void UpdateGettingGameID();

        // -- members (offsets pinned above) ----------------------------------------------------
        BrnNetworkModuleIO::InviteOrJoinParams mInviteOrJoinParams;     // +0x000 (sizeof 0x9C)
        u8                                     maInviteParamsPad[0xA0 - sizeof(BrnNetworkModuleIO::InviteOrJoinParams)]; // -> +0xA0
        // The server player-info record (BrnNetwork::PlayerInfoData) embedded by value. Its full
        // 256-byte layout is homed in BrnNetworkPlayerInfoData.{h,cpp}; reserved as opaque storage
        // here so the later members keep their X360-pinned offsets and we do not fork the type.
        u8                                     maPlayerInfoData[0x1A0 - 0xA0];  // +0x0A0 (256B)
        PlayerName                             mPlayerNameToGetGameID; // +0x1A0 (16B)
        EGetGameIDState                        meGetGameIDState;       // +0x1B0
        EInviteState                           meInviteState;          // +0x1B4
        BrnNetworkModule*                      mpNetworkModule;        // +0x1B8
    };
}
