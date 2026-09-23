#include "types.hpp"

#include "GameSource/Network/Messages/BrnStuntScoreUpdatedMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::StuntScoreUpdatedMessage::PackOrUnpack    @ 0x8257D0E0
//   BrnNetwork::StuntScoreUpdatedMessage::PrepareForSend  @ 0x8257D158
//
// A reliable per-event message carrying one player's updated stunt score (a single int at
// +0x28, directly after the 0x28-byte CgsNetwork::ReliableMessage base).
//
// PackOrUnpack asserts the score is initialised (miStuntScore != -1), then ORs the base
// ReliableMessage pack/unpack status with the quantised-int field status for the score in
// [0, 0x7FFFFFFF] (the X360 routes the field through PackOrUnpackInt == sub_82881370). Both
// statuses are u8; 0 == success. PrepareForSend asserts the frame is valid, the score is
// initialised, and the message slot is not already pending (the RELIABLE/VALID flag bit at
// +0x19 must be clear -- !IsMessageValid()), then stores the score and stamps the reliable
// message with type 41 (0x29) for the given frame.

namespace BrnNetwork
{
    // EMessageType id for the stunt-score-updated message (asm `li r4, 0x29`).
    static const s32 KI_STUNT_SCORE_UPDATED_MESSAGE_TYPE = 41;

    CgsNetwork::PackOrUnpackResult StuntScoreUpdatedMessage::PackOrUnpack()
    {
        CGS_ASSERT(miStuntScore != -1, "miStuntScore != -1");

        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        return CgsNetwork::PackOrUnpackInt(this, &miStuntScore, 0, 0x7FFFFFFF) | lxBase;
    }

    void StuntScoreUpdatedMessage::PrepareForSend(u16 lu16FrameCount, s32 liStuntScore)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");
        CGS_ASSERT(liStuntScore != -1, "liStuntScore != -1");
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");

        miStuntScore = liStuntScore;   // stw r28, 0x28(this)
        CgsNetwork::ReliableMessage::PrepareForSend(KI_STUNT_SCORE_UPDATED_MESSAGE_TYPE, lu16FrameCount);
    }

    // The console body is one folded copy shared with CheckpointTriggeredMessage::Retrieve
    // (same layout: one s32 at +0x28).
    bool StuntScoreUpdatedMessage::Retrieve(s32* lpiStuntScore)
    {
        if (IsMessageValid())
        {
            *lpiStuntScore = miStuntScore;
            SetMessageInvalid();
            return true;
        }

        *lpiStuntScore = -1;
        return false;
    }

    // Zero the score, then size through the reliable base (one folded copy shared with
    // CheckpointTriggeredMessage::GetPackedMessageSize).
    s32 StuntScoreUpdatedMessage::GetPackedMessageSize()
    {
        miStuntScore = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // The console slot is the same folded code as CheckpointTriggeredMessage::GetName, so
    // this message names itself with that string.
    const char* StuntScoreUpdatedMessage::GetName() const
    {
        return "Checkpoint Triggered Message";
    }
} // namespace BrnNetwork
