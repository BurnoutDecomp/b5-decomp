#include "types.hpp"
#include "GameSource/Network/Messages/BrnCheckpointTriggeredMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CheckpointTriggeredMessage::GetName              @ 0x8257CEC8
//   BrnNetwork::CheckpointTriggeredMessage::GetPackedMessageSize @ 0x8257D0D0
//   BrnNetwork::CheckpointTriggeredMessage::PackOrUnpack         @ 0x8257CE50
//   BrnNetwork::CheckpointTriggeredMessage::PrepareForSend       @ 0x8257CED8
//   BrnNetwork::CheckpointTriggeredMessage::Retrieve             @ 0x8257FE20
//
// A reliable per-event message carrying one triggered checkpoint index (@+0x28, directly
// after the 0x28-byte CgsNetwork::ReliableMessage base). Message type 39 (0x27).

namespace BrnNetwork
{
    const char* CheckpointTriggeredMessage::GetName() const
    {
        return "Checkpoint Triggered Message";
    }

    s32 CheckpointTriggeredMessage::GetPackedMessageSize()
    {
        miCheckpointIndex = 0;   // li r11,0 ; stw r11,0x28(this)
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult CheckpointTriggeredMessage::PackOrUnpack()
    {
        CGS_ASSERT(miCheckpointIndex != -1, "miCheckpointIndex != -1");

        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        return CgsNetwork::PackOrUnpackInt(this, &miCheckpointIndex, 0, 16) | lxBase;
    }

    void CheckpointTriggeredMessage::PrepareForSend(u16 lu16FrameCount, s32 liCheckpointIndex)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");
        CGS_ASSERT(liCheckpointIndex != -1, "liCheckpointIndex != -1");
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");

        miCheckpointIndex = liCheckpointIndex;   // stw r28,0x28(this)
        CgsNetwork::ReliableMessage::PrepareForSend(KI_CHECKPOINT_TRIGGERED_MESSAGE_TYPE, lu16FrameCount);
    }

    // Retrieve @ 0x8257FE20
    // If this message is still marked valid (a checkpoint-triggered event is pending),
    // hand the triggered checkpoint index to the caller, clear the valid flag so the
    // event is consumed exactly once, and report success. Otherwise emit the sentinel
    // (-1) and report that nothing was pending. (No asserts in the X360 body.)
    bool CheckpointTriggeredMessage::Retrieve(s32* lpiCheckpointIndex)
    {
        if (IsMessageValid())                        // lbz +0x19; clrlwi ,31; beq
        {
            *lpiCheckpointIndex = miCheckpointIndex; // lwz r10,0x28 ; stw r10,0(r4)
            SetMessageInvalid();                     // lbz +0x19; clrrwi ,1; stb +0x19
            return true;                             // li r3,1
        }

        *lpiCheckpointIndex = -1;                     // li r11,-1 ; stw r11,0(r4)
        return false;                                // li r3,0
    }
} // namespace BrnNetwork
