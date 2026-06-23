#pragma once

// ===================================================================================
// CgsNetwork::ConnectionStatusMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsConnectionStatusMessage.h
//
// A CgsNetwork::ReliableMessage subclass that carries the per-player connection state
// for the up-to-7 remote players in a session. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsConnectionStatusMessage.h):
//       struct ConnectionStatusMessage : public CgsNetwork::ReliableMessage
//       { PlayerConnectionData maConnectionData[7]; ... }
// reusing the committed CgsNetwork::ReliableMessage base (CgsReliableMessage.h) BY NAME.
//
// Layout gated against the X360 binary:
//   ReliableMessage base is 0x28 bytes (Message 0x20 + the two MessageWithPlayerIDs
//   player-id words at +0x20 / +0x24), so the first leaf member lands at +0x28.
//   ConnectionStatusMessage::PrepareForSend @ 0x82883138 streams the incoming records
//   into the array starting at *(this + 0x28) in 8-byte (two-dword) strides, and
//   GetPackedMessageSize @ 0x82883360 walks the same 7 records resetting each pair to
//   { mPlayerID = -1, meConnectionStatus = 0 }. So PlayerConnectionData is 8 bytes:
//   mPlayerID (NetworkPlayerID, +0) then meConnectionStatus (EConnectionStatus, +4),
//   and maConnectionData[7] occupies +0x28 .. +0x5F.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Ledger func for this TU:
//   GetName @ 0x827DE368 -- header-homed inline accessor; returns the literal
//                           "Connection Status Message".
// The remaining methods are bodied in their own TU (CgsConnectionStatusMessage.cpp);
// they are declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace CgsNetwork
{
    // DWARF CgsConnectionStatusMessage.h:37 -- per-player connection lifecycle state.
    enum EConnectionStatus
    {
        E_NOT_STARTED            = 0,
        E_CONNAPI_PROTOMANGLING  = 1,
        E_CHECKING_CONNECTION    = 2,
        E_CONNECTION_SUCCESS     = 3,
        E_CONNECTION_FAILURE     = 4,

        E_CONNECTION_STATUS_COUNT = 5,
    };

    // DWARF CgsConnectionStatusMessage.h:48 -- one (player, status) pair. The asm
    // stores it as two 32-bit words (player id then status), so it is 8 bytes.
    struct PlayerConnectionData
    {
        MessageWithPlayerIDs::NetworkPlayerID mPlayerID;          // +0x00
        EConnectionStatus                     meConnectionStatus; // +0x04
    };

    // The connection status message carries one entry per remote player slot.
    const s32 KI_CONNECTION_STATUS_PLAYER_COUNT = 7;

    struct ConnectionStatusMessage : ReliableMessage
    {
        void               Construct();
        void               PrepareForSend(const PlayerConnectionData* lpaConnectionData,
                                          u16 lu16Frame);
        bool               Retrieve(PlayerConnectionData* lpaConnectionData);
        void               Update();
        void               Release();
        void               Destruct();
        s32                GetPackedMessageSize();

        // Ledger func @ 0x827DE368 -- inline header-homed accessor.
        const char*        GetName() const { return "Connection Status Message"; }

        PackOrUnpackResult PackOrUnpack();

        // +0x28 .. +0x5F (after the 0x28-byte ReliableMessage base).
        PlayerConnectionData maConnectionData[KI_CONNECTION_STATUS_PLAYER_COUNT];
    };
} // namespace CgsNetwork
