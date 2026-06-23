// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutPlayerEvents.h
// ============================================================================
// The three BrnNetwork::BrnNetworkModuleIO network-out player lifecycle events that
// BrnNetwork::BrnNetworkManager::PlayerManagerEventCallback (X360 0x82599430) builds on the
// stack and AddEvent()s onto the network output VariableEventQueue<14000,16>:
//
//   NetworkOutPlayerAddedEvent       event-type 0x10, AddEvent size 0x28 (40 bytes)
//   NetworkOutPlayerFinalisedEvent   event-type 0x14, AddEvent size 0x04 (4 bytes)
//   NetworkOutPlayerRemovedEvent     event-type 0x11, AddEvent size 0x18 (24 bytes)
//
// Each carries the player's network id, written through SetNetworkPlayerID (this TU's owned
// function; see the .cpp). The store OFFSET for the id is X360-authoritative from the asm:
//   PlayerAdded::SetNetworkPlayerID     @ 0x825810E0 -- stw r31, 0x10(r30)  -> id at +0x10
//   PlayerFinalised::SetNetworkPlayerID @ 0x82581140 -- stw r31, 0(r30)     -> id at +0x00
//   PlayerRemoved::SetNetworkPlayerID   @ 0x825811A0 -- stw r31, 0(r30)     -> id at +0x00
//
// MINIMAL-SLICE / NO-FABRICATION NOTE: only SetNetworkPlayerID is in scope for these TUs, so
// only the player-id field is a named member (placed at its proven offset). The remaining
// payload bytes -- the team/score/finish/host-state fields the callback fills in directly at
// the call site (PlayerAdded: team/score/finish/active-race-car words around +0..+0x14;
// Removed: two host-state bools + a 16-byte block) -- are NOT set by any function this TU owns
// and are not recoverable as named members from SetNetworkPlayerID alone. They are modelled as
// offset-pinned opaque storage sized to the X360-confirmed AddEvent byte size, with no
// fabricated field names/types. Replace the opaque spans with real named members (preserving
// the +0x10 / +0 id offset and the total sizes) when those events' Construct/build TUs land.
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ------------------------------------------------------------------------
    // NetworkOutPlayerAddedEvent -- event-type 0x10, 40-byte AddEvent payload.
    // SetNetworkPlayerID stores the id at +0x10.
    // ------------------------------------------------------------------------
    struct NetworkOutPlayerAddedEvent
    {
        u8              maOpaqueHead[0x10];  // +0x00  team/score/finish/active-race-car payload (built at the call site)
        NetworkPlayerID mNetworkPlayerID;    // +0x10  set by SetNetworkPlayerID (stw @0x82581124)
        u8              maOpaqueTail[0x28 - 0x14]; // +0x14 .. +0x28  remaining payload to the 40-byte AddEvent size

        // @ 0x825810E0 -- store the network player id (asserts id != K_INVALID_PLAYER_ID).
        void SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
    };
    static_assert(sizeof(NetworkOutPlayerAddedEvent) == 0x28, "NetworkOutPlayerAddedEvent size (event-type 0x10, 40 bytes)");

    // ------------------------------------------------------------------------
    // NetworkOutPlayerFinalisedEvent -- event-type 0x14, 4-byte AddEvent payload
    // (just the player id). SetNetworkPlayerID stores the id at +0.
    // ------------------------------------------------------------------------
    struct NetworkOutPlayerFinalisedEvent
    {
        NetworkPlayerID mNetworkPlayerID;    // +0x00  set by SetNetworkPlayerID (stw @0x82581184)

        // @ 0x82581140 -- store the network player id (asserts id != K_INVALID_PLAYER_ID).
        void SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
    };
    static_assert(sizeof(NetworkOutPlayerFinalisedEvent) == 0x04, "NetworkOutPlayerFinalisedEvent size (event-type 0x14, 4 bytes)");

    // ------------------------------------------------------------------------
    // NetworkOutPlayerRemovedEvent -- event-type 0x11, 24-byte AddEvent payload.
    // SetNetworkPlayerID stores the id at +0; the call site fills the trailing
    // host-state bools + 16-byte block.
    // ------------------------------------------------------------------------
    struct NetworkOutPlayerRemovedEvent
    {
        NetworkPlayerID mNetworkPlayerID;    // +0x00  set by SetNetworkPlayerID (stw @0x825811E4)
        u8              maOpaqueTail[0x18 - 0x04]; // +0x04 .. +0x18  host-state bools + 16B block (built at the call site)

        // @ 0x825811A0 -- store the network player id (asserts id != K_INVALID_PLAYER_ID).
        void SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID);
    };
    static_assert(sizeof(NetworkOutPlayerRemovedEvent) == 0x18, "NetworkOutPlayerRemovedEvent size (event-type 0x11, 24 bytes)");
}
}
