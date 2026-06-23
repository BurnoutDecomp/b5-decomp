#pragma once

// ===================================================================================
// CgsNetwork::StartTimeMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsStartTimeMessage.h
//
// A CgsNetwork::ReliableMessage subclass that announces the host-chosen race start
// time and the set of client ids that have acknowledged it. SHAPE from the DecFIGS
// DWARF (references/DecFIGS/dwarfdump/.../CgsStartTimeMessage.h):
//       struct StartTime { NetworkPlayerID mHostID; Time mStartTime; };
//       struct StartTimeMessage : public CgsNetwork::ReliableMessage
//       { StartTime mStartTime; NetworkPlayerID maReceivedClientsIDs[8]; ... }
// reusing the committed CgsNetwork::ReliableMessage base (CgsReliableMessage.h) and the
// committed CgsSystem::Time value type (CgsTime.h) BY NAME.
//
// Layout gated against the X360 binary (PrepareForSend @ 0x8288A498 / Retrieve
// @ 0x8288A558): the ReliableMessage base is 0x28 bytes, so the leaf members start at
//   +0x28  StartTime::mHostID    (NetworkPlayerID word; *(this+40) = *a3)
//   +0x2C  StartTime::mStartTime (CgsSystem::Time: seconds +0x2C, fraction +0x30;
//                                 *(this+44)/*(this+48) = a3[1]/a3[2])
//   +0x34  maReceivedClientsIDs[8] (eight NetworkPlayerID words; *(this+52..+80) = a4[0..7])
// So StartTime is 12 bytes and the trailing id array occupies +0x34 .. +0x53.
//
// The X360 build models the vtable as the explicit Message::mpVTable member (no C++
// `virtual`), matching the committed base; these are therefore plain methods.
//
// Ledger func for this TU:
//   GetName @ 0x827DE0E8 -- header-homed inline accessor; returns the literal
//                           "Start Time Message".
// The remaining methods are bodied in their own TU (CgsStartTimeMessage.cpp); they are
// declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

namespace CgsNetwork
{
    // DWARF CgsStartTimeMessage.h:38 -- the host id paired with the chosen start time.
    struct StartTime
    {
        MessageWithPlayerIDs::NetworkPlayerID mHostID;     // +0x00
        CgsSystem::Time                       mStartTime;  // +0x04 (seconds +0x04, fraction +0x08)
    };

    // The start-time message tracks acknowledgements from up to 8 clients.
    const s32 KI_START_TIME_CLIENT_COUNT = 8;

    struct StartTimeMessage : ReliableMessage
    {
        void               Construct();
        bool               PrepareForSend(u16 lu16Frame,
                                          const StartTime* lpStartTime,
                                          MessageWithPlayerIDs::NetworkPlayerID* lpaReceivedClientsIDs);
        void               Release();
        void               Destruct();
        bool               Retrieve(StartTime* lpStartTime,
                                    MessageWithPlayerIDs::NetworkPlayerID* lpaReceivedClientsIDs);
        s32                GetPackedMessageSize();

        // Ledger func @ 0x827DE0E8 -- inline header-homed accessor.
        const char*        GetName() const { return "Start Time Message"; }

        PackOrUnpackResult PackOrUnpack();

        // +0x28 .. (after the 0x28-byte ReliableMessage base).
        StartTime                             mStartTime;            // +0x28 .. +0x33
        MessageWithPlayerIDs::NetworkPlayerID maReceivedClientsIDs[KI_START_TIME_CLIENT_COUNT]; // +0x34 .. +0x53
    };
} // namespace CgsNetwork
