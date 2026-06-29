#pragma once

// ===================================================================================
// BrnNetwork::CheckpointTriggeredMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCheckpointTriggeredMessage.h
//
// A RELIABLE per-event message reporting that a player triggered a checkpoint (carries the
// triggered checkpoint index). Derives from CgsNetwork::ReliableMessage (which adds no data
// of its own; sizeof == 0x28), so the single leaf field lands directly after the base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  s32 miCheckpointIndex
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnCheckpointTriggeredMessage.h:44), gated against the
// X360 binary (the BrnNetworkPlayer TU calls Retrieve @ 0x8259460C). Only the cross-TU
// entry points the BrnNetworkPlayer TU reaches are declared here; the remaining DWARF
// methods (Construct/Destruct/PrepareForSend + the four packing virtuals) are declared-only
// API whose bodies live in this message type's own, not-yet-reconstructed, TU.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    struct CheckpointTriggeredMessage : CgsNetwork::ReliableMessage
    {
        // DWARF BrnCheckpointTriggeredMessage.cpp:163 -- unpack the received checkpoint index
        // into the caller's out-slot; returns true on a valid retrieval. Declared-only (body
        // lands with this message type's own TU).
        bool Retrieve(s32* lpiCheckpointIndex);

        // DWARF BrnCheckpointTriggeredMessage.cpp:139 -- stamp the send-slot. Declared-only.
        void PrepareForSend(u16 lu16FrameCount, s32 liCheckpointIndex);

    private:
        s32 miCheckpointIndex;   // +0x28 (DWARF BrnCheckpointTriggeredMessage.h:68)
    };
} // namespace BrnNetwork
