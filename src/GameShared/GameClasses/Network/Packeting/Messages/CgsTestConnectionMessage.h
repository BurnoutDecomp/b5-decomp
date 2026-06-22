#pragma once

// ===================================================================================
// CgsNetwork::TestConnectionMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h
//
// SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsTestConnectionMessage.h:40):
//       struct TestConnectionMessage : public CgsNetwork::ReliableMessage { ... }
// reusing the committed CgsNetwork::ReliableMessage base (CgsReliableMessage.h) BY
// NAME. ReliableMessage adds no data of its own (its reliable id reuses the inherited
// Message::mu16Frame), and TestConnectionMessage likewise declares no data members in
// the DWARF -- it is a bare keep-the-link-warm probe -- so
// sizeof(TestConnectionMessage) == sizeof(ReliableMessage) == 0x28.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Reconstructed here (ledger func for this TU):
//   GetName @ 0x827DE358 -- header-homed inline accessor; returns the literal
//                           "Test Connection Message".
//     0x827DE358  lis  r11, aTestConnection@ha
//     0x827DE35C  addi r3,  r11, aTestConnection@l  # "Test Connection Message"
//     0x827DE360  blr
// The remaining methods (Construct/PrepareForSend/Retrieve/Update/Release/Destruct/
// GetPackedMessageSize/PackOrUnpack) are bodied in their own TU; declared here so the
// rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace CgsNetwork
{
    struct TestConnectionMessage : ReliableMessage
    {
        void               Construct();
        void               PrepareForSend(u16 lu16Frame);
        bool               Retrieve();
        void               Update();
        void               Release();
        void               Destruct();
        s32                GetPackedMessageSize();

        // Ledger func @ 0x827DE358 -- inline header-homed accessor.
        const char* GetName() const { return "Test Connection Message"; }

        PackOrUnpackResult PackOrUnpack();
    };
} // namespace CgsNetwork
