#pragma once

// ===================================================================================
// BrnNetwork::CheckpointTriggeredMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCheckpointTriggeredMessage.h
//
// A RELIABLE per-event message reporting that a player triggered a checkpoint (carries the
// triggered checkpoint index). Derives from CgsNetwork::ReliableMessage (which adds no data
// of its own; sizeof == 0x28), so the single leaf field lands directly after the base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x19  x8 mx8Flags (inherited) -- bit0 == KX8_FLAGS_VALID (message-pending flag)
//   +0x28  s32 miCheckpointIndex
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnCheckpointTriggeredMessage.h:44), gated against the
// X360 binary. The packing/name virtuals and PrepareForSend are X360-verified
//   (GetName @ 0x8257CEC8, GetPackedMessageSize @ 0x8257D0D0, PackOrUnpack @ 0x8257CE50,
//    PrepareForSend @ 0x8257CED8, Retrieve @ 0x8257FE20).
// The message-type id stamped by PrepareForSend is 39 (0x27; asm `li r4, 0x27`).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // EMessageType id for the checkpoint-triggered message (asm `li r4, 0x27`).
    static const s32 KI_CHECKPOINT_TRIGGERED_MESSAGE_TYPE = 39;

    struct CheckpointTriggeredMessage : CgsNetwork::ReliableMessage
    {
        // DWARF BrnCheckpointTriggeredMessage.cpp:39/55 -- ctor/dtor helpers. Declared-only
        // (bodies live with this message type's own TU).
        void Construct();
        void Destruct();

        // @ 0x8257CED8 -- assert the inputs + that the slot is not already pending, store the
        // index, then stamp the reliable message (type 39) for the given frame.
        void PrepareForSend(u16 lu16FrameCount, s32 liCheckpointIndex);

        // @ 0x8257FE20 -- unpack the received checkpoint index into the caller's out-slot;
        // returns true on a valid retrieval (clears the VALID flag).
        bool Retrieve(s32* lpiCheckpointIndex);

    private:
        // @ 0x8257D0D0 -- zero miCheckpointIndex then delegate to the reliable base.
        s32                            GetPackedMessageSize();
        // @ 0x8257CE50 -- (de)serialise the index in [0, 16]; ORs the base reliable id status
        // with the quantised int field status. 0 == success.
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        // @ 0x8257CEC8 -- returns the literal message name.
        const char*                    GetName() const;
        // DWARF BrnCheckpointTriggeredMessage.cpp:122 -- declared-only.
        bool                           OldMessagesAreValid() const;

        s32 miCheckpointIndex;   // +0x28 (DWARF BrnCheckpointTriggeredMessage.h:68)
    };
} // namespace BrnNetwork
