#pragma once

// ===================================================================================
// CgsNetwork::SignalMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h
//
// SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsSignalMessage.h:46):
//       struct SignalMessage : public CgsNetwork::MessageWithPlayerIDs { ... }
// reusing the committed CgsNetwork::MessageWithPlayerIDs base (CgsMessageWithPlayerIDs.h)
// BY NAME. SignalMessage carries no data members of its own in the DWARF -- it is an
// ack/nack control message whose payload is entirely the inherited sending/receiving
// player-id pair -- so sizeof(SignalMessage) == sizeof(MessageWithPlayerIDs) == 0x28.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Reconstructed here (ledger func for this TU):
//   GetName @ 0x827DDF40 -- header-homed inline accessor; returns the literal
//                           "Signal Message".
//     0x827DDF40  lis  r11, aSignalMessage@ha
//     0x827DDF44  addi r3,  r11, aSignalMessage@l   # "Signal Message"
//     0x827DDF48  blr
// The remaining methods (PrepareAck/PrepareNack/GetPackedMessageSize/PackOrUnpack)
// are bodied in their own TU; declared here so the rest of the hierarchy can call
// them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"

namespace CgsNetwork
{
    struct SignalMessage : MessageWithPlayerIDs
    {
        // The ack/nack control payload reuses the committed NetworkPlayerID typedef.
        typedef MessageWithPlayerIDs::NetworkPlayerID NetworkPlayerID;

        // DWARF h:96 / h:124 -- stamp this as an ACK / NACK aimed at a player pair.
        void               PrepareAck(Message* lpMessage,
                                      NetworkPlayerID liSendingPlayerID,
                                      NetworkPlayerID liRecvingPlayerID);
        void               PrepareNack(Message* lpMessage,
                                       NetworkPlayerID liSendingPlayerID,
                                       NetworkPlayerID liRecvingPlayerID);
        s32                GetPackedMessageSize();

        // Ledger func @ 0x827DDF40 -- inline header-homed accessor.
        const char* GetName() const { return "Signal Message"; }

        PackOrUnpackResult PackOrUnpack();
    };
} // namespace CgsNetwork
