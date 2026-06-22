#pragma once

// ===================================================================================
// CgsNetwork::NewHostMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsNewHostMessage.h
//
// A CgsNetwork::Message subclass broadcast during host migration: it names the player
// that is becoming the new host and the set of clients whose acknowledgements have
// been received. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsNewHostMessage.h):
//       struct NewHostMessage : public CgsNetwork::Message {
//           MessageWithPlayerIDs::NetworkPlayerID mNewHostID;            // h:75
//           MessageWithPlayerIDs::NetworkPlayerID maReceivedClientsIDs[8]; // h:76
//           ...
//       }
// reusing the committed CgsNetwork::Message base (CgsMessage.h) BY NAME, and the
// committed CgsNetwork::MessageWithPlayerIDs::NetworkPlayerID typedef
// (CgsMessageWithPlayerIDs.h) for the player-id fields. Both id members land after the
// 0x20-byte Message base.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Reconstructed here (ledger func for this TU):
//   GetName @ 0x827DBC58 -- header-homed inline accessor; returns the literal
//                           "New Host Message".
// The remaining methods are bodied in their own TUs (CgsNewHostMessage.cpp); they are
// declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"

namespace CgsNetwork
{
    struct NewHostMessage : Message
    {
        // The player-id payload reuses the committed NetworkPlayerID typedef.
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // Max client acknowledgements tracked in a single migration message.
        static const s32 KI_MAX_RECEIVED_CLIENTS = 8;

        Message*    Construct();
        void        PrepareForSend(u16 lu16CurrentFrame, NetworkPlayerID liNewHostID,
                                   NetworkPlayerID* lpReceivedClientsIDs);
        bool        Retrieve(NetworkPlayerID* lpNewHostID, NetworkPlayerID* lpReceivedClientsIDs);
        s32         GetPackedMessageSize();

        // Ledger func @ 0x827DBC58 -- inline header-homed accessor.
        const char* GetName() const { return "New Host Message"; }

        PackOrUnpackResult PackOrUnpack();

        NetworkPlayerID mNewHostID;                                  // DWARF h:75 (after 0x20 base)
        NetworkPlayerID maReceivedClientsIDs[KI_MAX_RECEIVED_CLIENTS]; // DWARF h:76
    };
} // namespace CgsNetwork
