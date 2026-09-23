#pragma once

// ===================================================================================
// CgsNetwork::PingMessage / CgsNetwork::PingReplyMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsPingMessage.h
//
// SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsPingMessage.h:47 / :91):
//       struct PingMessage      : public CgsNetwork::Message { float mfPingTime; ... }
//       struct PingReplyMessage : public CgsNetwork::Message { float mfPingTime; ... }
// reusing the committed CgsNetwork::Message base (CgsMessage.h) BY NAME. Each carries a
// single f32 ping-time member after the 0x20-byte Message base (DWARF h:82 / h:126).
// The DWARF spells it float_t; the project scalar type is f32.
//
// GetPackedMessageSize, GetName and PackOrUnpack override the Message virtuals.
//
// Reconstructed here (ledger funcs for this TU -- 2):
//   PingMessage::GetName      @ 0x827DE0C8 -- inline; returns "Ping Message".
//     0x827DE0C8  lis  r11, aPingMessage@ha
//     0x827DE0CC  addi r3,  r11, aPingMessage@l        # "Ping Message"
//     0x827DE0D0  blr
//   PingReplyMessage::GetName @ 0x827DE0D8 -- inline; returns "Ping Reply Message".
//     0x827DE0D8  lis  r11, aPingReplyMessa@ha
//     0x827DE0DC  addi r3,  r11, aPingReplyMessa@l     # "Ping Reply Message"
//     0x827DE0E0  blr
// The remaining methods (Construct/PrepareForSend/Retrieve/Release/Destruct/
// GetPackedMessageSize/PackOrUnpack) are bodied in their own TU; declared here so the
// rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace CgsNetwork
{
    // A round-trip latency probe: carries the send timestamp so the reply can compute
    // elapsed ping time.
    struct PingMessage : Message
    {
        // Inlined at every call site on the console: the base field reset only.
        void               Construct() { Message::Construct(); }
        void               PrepareForSend(u16 lu16Frame, f32 lfPingTime);
        bool               Retrieve(f32* lpfPingTime);
        void               Release();
        void               Destruct();
        s32                GetPackedMessageSize() override;

        // Ledger func @ 0x827DE0C8 -- inline header-homed accessor.
        const char* GetName() const override { return "Ping Message"; }

        PackOrUnpackResult PackOrUnpack() override;

        f32 mfPingTime;     // DWARF h:82 (after 0x20 Message base)
    };

    // The matching reply, sent back by the pinged peer.
    struct PingReplyMessage : Message
    {
        // Inlined at every call site on the console: the base field reset only.
        void               Construct() { Message::Construct(); }
        void               PrepareForSend(u16 lu16Frame, f32 lfPingTime);
        bool               Retrieve(f32* lpfPingTime);
        void               Release();
        void               Destruct();
        s32                GetPackedMessageSize() override;

        // Ledger func @ 0x827DE0D8 -- inline header-homed accessor.
        const char* GetName() const override { return "Ping Reply Message"; }

        PackOrUnpackResult PackOrUnpack() override;

        f32 mfPingTime;     // DWARF h:126 (after 0x20 Message base)
    };

    // Console sizes (the RegisterMessageType lengths), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(PingMessage) == 0x24, "sizeof(PingMessage) == 0x24");
    static_assert(sizeof(void*) != 4 || sizeof(PingReplyMessage) == 0x24, "sizeof(PingReplyMessage) == 0x24");

} // namespace CgsNetwork
