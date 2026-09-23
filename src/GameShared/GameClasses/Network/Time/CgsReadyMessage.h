#pragma once

// ===================================================================================
// CgsNetwork::ReadyMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsReadyMessage.h
//
// A CgsNetwork::ReliableMessage subclass clients send to announce they are ready (the
// race-start ready handshake). SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsReadyMessage.h:36):
//       struct ReadyMessage : public CgsNetwork::ReliableMessage { ... }   // no own data
// reusing the committed CgsNetwork::ReliableMessage base (CgsReliableMessage.h) BY NAME.
// ReliableMessage adds no data of its own (its reliable id reuses the inherited
// Message::mu16Frame at +0x1C), and ReadyMessage likewise declares no data members in
// the DWARF, so sizeof(ReadyMessage) == sizeof(ReliableMessage) == 0x28.
//
// Message's five console vtable slots are real C++ virtuals; this leaf overrides
// GetPackedMessageSize, GetName and PackOrUnpack.
//
// Ledger funcs for this TU (both are identical-code-folded tail-call forwarders in the
// X360 image -- a bare `b <sibling>` with no own prologue):
//   GetPackedMessageSize @ 0x827DE0F8 -- folds onto
//       CgsNetwork::TestConnectionMessage::GetPackedMessageSize @ 0x827DE0F8
//     Both are bare ReliableMessage subclasses with no extra payload, so the packed size
//     is identical; the X360 linker collapsed the two bodies into one address. We express
//     it as the delegate it forwards to.
//   PackOrUnpack @ 0x827DE100 -- `b CgsNetwork__ReliableMessage__PackOrUnpack`: the leaf
//     adds no fields to (de)serialise, so it forwards straight to the base
//     ReliableMessage::PackOrUnpack (which (de)serialises the wrapped reliable id).
//
// GetName is the header-homed inline accessor ("Ready Message", the literal its vtable
// slot returns). The remaining lifecycle methods (Construct/PrepareForSend/Retrieve/
// Release/Destruct) are bodied in their own TUs; declared here so the rest of the
// hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace CgsNetwork
{
    struct ReadyMessage : ReliableMessage
    {
        void               Construct();
        void               PrepareForSend(u16 lu16Frame);
        bool               Retrieve();
        void               Release();
        void               Destruct();

        // Ledger func @ 0x827DE0F8 -- identical-code-folded onto
        // TestConnectionMessage::GetPackedMessageSize (same bare-ReliableMessage size).
        s32                GetPackedMessageSize() override;

        const char*        GetName() const override { return "Ready Message"; }

        // Ledger func @ 0x827DE100 -- forwards to the ReliableMessage base.
        PackOrUnpackResult PackOrUnpack() override;
    };
} // namespace CgsNetwork
