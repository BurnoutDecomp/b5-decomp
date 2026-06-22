#pragma once

// ===================================================================================
// CgsNetwork::ReliableMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsReliableMessage.h:47):
//       struct ReliableMessage : public MessageWithPlayerIDs { ... }
// gated against the X360 binary. ReliableMessage adds no data members of its own --
// the reliable sequence id reuses the inherited Message::mu16Frame field (+0x1C),
// which ReliableMessage::PackOrUnpack @ 0x828821A8 (de)serialises in [0, 65534]. So
// sizeof(ReliableMessage) == sizeof(MessageWithPlayerIDs) == 0x28, and the first leaf
// member lands at +0x28 (confirmed by every leaf ctor, e.g.
// BrnNetwork::CameraRequestMessage::Construct stores its mbReleaseFeed at +0x28).
//
// The X360 build models the vtable as Message::mpVTable (no C++ `virtual`); these are
// plain methods. Bodies live in CgsReliableMessage.cpp / CgsMessageFrameUtils.cpp.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"

namespace CgsNetwork
{
    struct ReliableMessage : MessageWithPlayerIDs
    {
        // Stamps the message type + frame and flags it RELIABLE; chains to the
        // MessageWithPlayerIDs base. liType is an EMessageType.
        void               PrepareForSend(s32 liType, u16 lu16Frame);
        bool               IsReliable() const;
        s32                GetPackedMessageSize();

        // Serialises the wrapped 16-bit reliable id (stored in the inherited
        // mu16Frame field) through the shared u16 primitive. 0 == success.
        PackOrUnpackResult PackOrUnpack();
    };
} // namespace CgsNetwork
