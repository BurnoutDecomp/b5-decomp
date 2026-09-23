#pragma once

// ===================================================================================
// CgsNetwork::HostKeepAliveMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsHostKeepAliveMessage.h
//
// A CgsNetwork::Message subclass the session host periodically broadcasts so clients
// know the host is still alive. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsHostKeepAliveMessage.h):
//       struct HostKeepAliveMessage : public CgsNetwork::Message { ... }   // no own data
// reusing the committed CgsNetwork::Message base (CgsMessage.h) BY NAME. The class adds
// no data members of its own (sizeof == sizeof(Message) == 0x20); the heartbeat frame
// reuses the inherited Message::mu16Frame field.
//
// GetPackedMessageSize, GetName and PackOrUnpack override the Message virtuals.
//
// Reconstructed here (ledger func for this TU):
//   GetName @ 0x827DBC48 -- header-homed inline accessor; returns the literal
//                           "Keep Host Alive Message".
// The remaining methods are bodied in their own TUs (CgsHostKeepAliveMessage.cpp);
// they are declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace CgsNetwork
{
    struct HostKeepAliveMessage : Message
    {
        Message*    Construct();
        void        PrepareForSend(u16 lu16CurrentFrame);
        bool        Retrieve();
        s32         GetPackedMessageSize() override;

        // Ledger func @ 0x827DBC48 -- inline header-homed accessor.
        const char* GetName() const override { return "Keep Host Alive Message"; }

        PackOrUnpackResult PackOrUnpack() override;
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(HostKeepAliveMessage) == 0x20, "sizeof(HostKeepAliveMessage) == 0x20");

} // namespace CgsNetwork
