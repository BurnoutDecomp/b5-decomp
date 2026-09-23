// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutPlayerEvents.h
// ============================================================================
// The four network-out player lifecycle records that
// BrnNetwork::BrnNetworkManager::PlayerManagerEventCallback builds on the stack and AddEvent()s
// onto the network output VariableEventQueue<14000,16> (console tag, AddEvent size):
//
//   NetworkOutPlayerAddedEvent       16, 40 bytes
//   NetworkOutPlayerRemovedEvent     17, 24 bytes
//   NetworkOutPlayerFinalisedEvent   20,  4 bytes
//   NetworkPlayerDisconnectedEvent   23, 12 bytes
//
// Every member offset is the one the callback's stack image and the GameBridgeNetworkToX
// translators use. None of the four holds a pointer, so the host layout is the console layout
// (pinned in AssertNetworkOutPlayerEventLayouts below).
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                 // CgsID (8 bytes)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // NetworkPlayerID, EActiveRaceCarIndex, NetworkEvent<N>
#include "GameSource/GameState/BrnGameStateSharedIO.h"        // GameStateModuleIO::EPlayerTeam
#include "GameSource/GameState/BrnCgsPlayerName.h"            // CgsNetwork::PlayerName (16 bytes)

#include <cstddef>   // offsetof (layout pins)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ------------------------------------------------------------------------
    // 23 -- a remote player lost contact, regained it or dropped. 12 bytes.
    // The member order is not the Construct argument order: Construct stores its first
    // argument to +0x00, its third (the status) to +0x04 and its second (the race-car index)
    // to +0x08.
    struct NetworkPlayerDisconnectedEvent : public NetworkEvent<23>
    {
        enum EDisconnectStatus
        {
            E_DISCONNECT_STATUS_CONNECTED    = 0,
            E_DISCONNECT_STATUS_LOST_CONTACT = 1,
            E_DISCONNECT_STATUS_DISCONNECTED = 2,
            E_DISCONNECT_STATUS_COUNT        = 3,
        };

        // Store the three fields, asserting the id is not the invalid sentinel.
        void Construct(NetworkPlayerID lNetworkPlayerID,
                       EActiveRaceCarIndex leActiveRaceCarIndex,
                       EDisconnectStatus leDisconnectStatus);

        EActiveRaceCarIndex GetActiveRaceCarIndex() const { return meActiveRaceCarIndex; }
        NetworkPlayerID     GetNetworkPlayerID()    const { return mNetworkPlayerID; }
        EDisconnectStatus   GetDisconnectStatus()   const { return meDisconnectStatus; }

    private:
        NetworkPlayerID     mNetworkPlayerID;     // +0x00
        EDisconnectStatus   meDisconnectStatus;   // +0x04
        EActiveRaceCarIndex meActiveRaceCarIndex; // +0x08

        friend void AssertNetworkOutPlayerEventLayouts();
    };

    // ------------------------------------------------------------------------
    // 16 -- a player joined the session. 40 bytes. The console record carries a float the
    // reference lacks (+0x18, the player menu data's float at +0x4C); the translator copies it
    // through to the game-state event unread.
    struct NetworkOutPlayerAddedEvent : public NetworkEvent<16>
    {
        CgsID                                        mModelID;                  // +0x00
        CgsID                                        mWheelID;                  // +0x08
        NetworkPlayerID                              mNetworkPlayerID;          // +0x10
        BrnGameState::GameStateModuleIO::EPlayerTeam meTeam;                    // +0x14
        f32                                          mf18;                      // +0x18 (console-only member; unnamed)
        u16                                          mu16CarColourIndex;        // +0x1C
        u16                                          mu16CarPaintFinishIndex;   // +0x1E
        bool                                         mbIsLocalPlayer;           // +0x20

        // Store the id, asserting it is not the invalid sentinel (out of line).
        void            SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; }
        void            SetModelID(CgsID lModelID) { mModelID = lModelID; }
        CgsID           GetModelID() const         { return mModelID; }
        void            SetWheelID(CgsID lWheelID) { mWheelID = lWheelID; }
        CgsID           GetWheelID() const         { return mWheelID; }
        void            SetTeam(BrnGameState::GameStateModuleIO::EPlayerTeam leTeam) { meTeam = leTeam; }
        BrnGameState::GameStateModuleIO::EPlayerTeam GetTeam() const { return meTeam; }
        void            SetLocalPlayer(bool lbIsLocalPlayer) { mbIsLocalPlayer = lbIsLocalPlayer; }
        bool            IsLocalPlayer() const      { return mbIsLocalPlayer; }
        void            SetCarColourIndex(u16 lu16CarColourIndex) { mu16CarColourIndex = lu16CarColourIndex; }
        u16             GetCarColourIndex() const  { return mu16CarColourIndex; }
        void            SetCarPaintFinishIndex(u16 lu16CarPaintFinishIndex) { mu16CarPaintFinishIndex = lu16CarPaintFinishIndex; }
        u16             GetCarPaintFinishIndex() const { return mu16CarPaintFinishIndex; }
    };

    // ------------------------------------------------------------------------
    // 20 -- a player's join completed. 4 bytes.
    struct NetworkOutPlayerFinalisedEvent : public NetworkEvent<20>
    {
        NetworkPlayerID mNetworkPlayerID;    // +0x00

        // Store the id, asserting it is not the invalid sentinel (out of line).
        void            SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; }
    };

    // ------------------------------------------------------------------------
    // 17 -- a player left the session. 24 bytes.
    struct NetworkOutPlayerRemovedEvent : public NetworkEvent<17>
    {
        NetworkPlayerID        mNetworkPlayerID;           // +0x00
        CgsNetwork::PlayerName mPlayerName;                // +0x04
        bool                   mbIsLocalPlayerInGame;      // +0x14
        bool                   mbLocalPlayerLeavingGame;   // +0x15

        // Store the id, asserting it is not the invalid sentinel (out of line).
        void            SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; }
        void            SetIsLocalPlayerInGame(bool lbIsLocalPlayerInGame) { mbIsLocalPlayerInGame = lbIsLocalPlayerInGame; }
        bool            IsLocalPlayerInGame() const { return mbIsLocalPlayerInGame; }
        void            SetIsLocalPlayerLeavingGame(bool lbLocalPlayerLeavingGame) { mbLocalPlayerLeavingGame = lbLocalPlayerLeavingGame; }
        bool            IsLocalPlayerLeavingGame() const { return mbLocalPlayerLeavingGame; }
        void            SetPlayerName(CgsNetwork::PlayerName* lpPlayerName) { mPlayerName = *lpPlayerName; }
        const CgsNetwork::PlayerName* GetPlayerName() const { return &mPlayerName; }
    };

    // ------------------------------------------------------------------------
    // Layout pins (console offsets and AddEvent sizes). Never called.
    inline void AssertNetworkOutPlayerEventLayouts()
    {
        static_assert(offsetof(NetworkPlayerDisconnectedEvent, meDisconnectStatus) == 0x04, "tag 23 +0x04");
        static_assert(offsetof(NetworkPlayerDisconnectedEvent, meActiveRaceCarIndex) == 0x08, "tag 23 +0x08");
        static_assert(sizeof(NetworkPlayerDisconnectedEvent) == 12, "tag 23 size");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mWheelID) == 0x08, "tag 16 +0x08");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mNetworkPlayerID) == 0x10, "tag 16 +0x10");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, meTeam) == 0x14, "tag 16 +0x14");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mf18) == 0x18, "tag 16 +0x18");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mu16CarColourIndex) == 0x1C, "tag 16 +0x1C");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mu16CarPaintFinishIndex) == 0x1E, "tag 16 +0x1E");
        static_assert(offsetof(NetworkOutPlayerAddedEvent, mbIsLocalPlayer) == 0x20, "tag 16 +0x20");
        static_assert(sizeof(NetworkOutPlayerAddedEvent) == 40, "tag 16 size");
        static_assert(sizeof(NetworkOutPlayerFinalisedEvent) == 4, "tag 20 size");
        static_assert(offsetof(NetworkOutPlayerRemovedEvent, mPlayerName) == 0x04, "tag 17 +0x04");
        static_assert(offsetof(NetworkOutPlayerRemovedEvent, mbIsLocalPlayerInGame) == 0x14, "tag 17 +0x14");
        static_assert(offsetof(NetworkOutPlayerRemovedEvent, mbLocalPlayerLeavingGame) == 0x15, "tag 17 +0x15");
        static_assert(sizeof(NetworkOutPlayerRemovedEvent) == 24, "tag 17 size");
    }
}
}
