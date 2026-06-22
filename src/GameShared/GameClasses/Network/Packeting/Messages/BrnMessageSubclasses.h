#pragma once

#include "types.hpp"
#include "CgsMessage.h"

// BrnNetwork message subclasses (CgsMessage.h hierarchy, Burnout-game layer).
//
// These are CgsNetwork::Message subclasses living in the BrnNetwork namespace.
// Their Release() overrides clear the inherited Message base flag byte
// (mx8Flags @+0x19); HullSyncMessage additionally clears a 4-byte member at the
// fixed base offset +0x54.
//
// FLAGGED: only the members each reconstructed function dereferences are modelled
// by name. The bytes between the Message base scalars (which end at +0x1E) and the
// +0x54 field HullSyncMessage::Release zeroes are reserved as an opaque sized
// placeholder so that field lands at the exact offset the asm stores to
// (stw r10, 0x54). The rest of each subclass body is not modelled by this TU.

namespace BrnNetwork
{
    // BrnNetwork::HullSyncMessage  (Release @ 0x8257DD40)
    struct HullSyncMessage : CgsNetwork::Message
    {
        // Message base occupies 0x00..0x1E (mu16Frame @+0x1C). Reserve up to +0x54.
        u8  mReserved[0x54 - 0x20];     // 0x20 .. 0x53 (opaque, FLAGGED placeholder)
        s32 miSyncState;                // 0x54  -- cleared to 0 on Release()

        void Release();
    };

    // BrnNetwork::UpdateMessage  (Release @ 0x8257D990)
    struct UpdateMessage : CgsNetwork::Message
    {
        void Release();
    };
}
