#pragma once

// ===================================================================================
// CgsNetwork::SyncTimeMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsSyncTimeMessage.h
//
// A CgsNetwork::Message subclass used to round-trip a clock-sync exchange between a
// client and the host: the client stamps its send time, the host stamps its own time,
// and both player ids ride along. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsSyncTimeMessage.h):
//       struct SyncTimeMessage : public CgsNetwork::Message
//       { Time mClientSendTime; Time mHostTime;
//         NetworkPlayerID mHostPlayerID; NetworkPlayerID mClientPlayerID; ... }
// reusing the committed CgsNetwork::Message base (CgsMessage.h) and the committed
// CgsSystem::Time value type (CgsTime.h) BY NAME. (Unlike StartTime/ConnectionStatus,
// this one derives directly from Message -- it is NOT a ReliableMessage.)
//
// Layout gated against the X360 binary (Prepare @ 0x8288B0B8 / Retrieve @ 0x8288B148):
// the Message base is 0x20 bytes, so the leaf members start at +0x20
//   +0x20  mClientSendTime  (CgsSystem::Time: seconds +0x20, fraction +0x24)
//   +0x28  mHostTime        (CgsSystem::Time: seconds +0x28, fraction +0x2C)
//   +0x30  mHostPlayerID    (NetworkPlayerID word)
//   +0x34  mClientPlayerID  (NetworkPlayerID word)
// matching the DWARF member order and the *(this+32/36)/(40/44)/(48)/(52) accesses.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Ledger func for this TU:
//   GetName @ 0x827DBC68 -- header-homed inline accessor; returns the literal
//                           "Sync Time Message".
// The remaining methods are bodied in their own TU (CgsSyncTimeMessage.cpp); they are
// declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsNetwork
{
    struct SyncTimeMessage : Message
    {
        void               Construct();
        void               Prepare(MessageWithPlayerIDs::NetworkPlayerID liClientPlayerID,
                                   CgsSystem::Time lClientSendTime,
                                   MessageWithPlayerIDs::NetworkPlayerID liHostPlayerID,
                                   CgsSystem::Time lHostTime);
        bool               Retrieve(MessageWithPlayerIDs::NetworkPlayerID* lpClientID,
                                    CgsSystem::Time* lpClientTime,
                                    MessageWithPlayerIDs::NetworkPlayerID* lpHostID,
                                    CgsSystem::Time* lpHostTime);
        s32                GetPackedMessageSize();

        // Ledger func @ 0x827DBC68 -- inline header-homed accessor.
        const char*        GetName() const { return "Sync Time Message"; }

        PackOrUnpackResult PackOrUnpack();

        // +0x20 .. +0x37 (after the 0x20-byte Message base).
        CgsSystem::Time                       mClientSendTime; // +0x20
        CgsSystem::Time                       mHostTime;       // +0x28
        MessageWithPlayerIDs::NetworkPlayerID mHostPlayerID;   // +0x30
        MessageWithPlayerIDs::NetworkPlayerID mClientPlayerID; // +0x34
    };
} // namespace CgsNetwork
