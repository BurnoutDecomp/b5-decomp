#pragma once

// ===================================================================================
// CgsNetwork::HeadsetStatusMessage -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h
//
// A CgsNetwork::Message subclass that carries a single byte of voice-chat headset
// status for a player. SHAPE from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../CgsHeadsetStatusMessage.h):
//       struct HeadsetStatusMessage : public CgsNetwork::Message { uint8 mu8HeadsetStatus; ... }
// reusing the committed CgsNetwork::Message base (CgsMessage.h) BY NAME. The one
// data member (mu8HeadsetStatus, DWARF CgsHeadsetStatusMessage.h:73) lands after the
// 0x20-byte Message base.
//
// GetPackedMessageSize, GetName and PackOrUnpack override the Message virtuals.
//
// Reconstructed here (ledger func for this TU):
//   GetName @ 0x827DBC78 -- header-homed inline accessor; returns the literal
//                           "Headset Status Message".
// The remaining methods are bodied in their own TUs (CgsHeadsetStatusMessage.cpp);
// they are declared here so the rest of the hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace CgsNetwork
{
    // Per-player headset state carried by a HeadsetStatusMessage (DWARF enum).
    enum ENetworkHeadsetPlayerStatus
    {
        E_NETWORK_HEADSET_PLAYER_STATUS_NONE,
        E_NETWORK_HEADSET_PLAYER_STATUS_HAS_HEADSET,
        E_NETWORK_HEADSET_PLAYER_STATUS_TALKING,

        E_NETWORK_HEADSET_PLAYER_STATUS_COUNT
    };

    struct HeadsetStatusMessage : Message
    {
        Message*    Construct();
        void        PrepareForSend(u16 lu16CurrentFrame, u8 lu8HeadsetStatus);
        bool        Retrieve(u8* lpu8HeadsetStatus);
        void        Destruct();
        s32         GetPackedMessageSize() override;

        // Ledger func @ 0x827DBC78 -- inline header-homed accessor.
        const char* GetName() const override { return "Headset Status Message"; }

        PackOrUnpackResult PackOrUnpack() override;

        u8 mu8HeadsetStatus;        // DWARF CgsHeadsetStatusMessage.h:73 (after 0x20 base)
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(HeadsetStatusMessage) == 0x24, "sizeof(HeadsetStatusMessage) == 0x24");

} // namespace CgsNetwork
