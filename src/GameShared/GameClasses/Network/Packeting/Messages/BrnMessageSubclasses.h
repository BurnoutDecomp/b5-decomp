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
    // BrnNetwork::HullSyncMessage now has its real layout in its owning header
    // (BrnHullSyncMessage.h): a CgsNetwork::ReliableMessage carrying a
    // BufferedHullsToActivate array. The field this TU's Release() zeroes (+0x54) is the
    // array's live-element count, so Release() clears the array. (It used to be modelled
    // here as an opaque struct with a bare miSyncState @ +0x54.)

    // BrnNetwork::UpdateMessage  (Release @ 0x8257D990)
    struct UpdateMessage : CgsNetwork::Message
    {
        void Release();
    };
}
