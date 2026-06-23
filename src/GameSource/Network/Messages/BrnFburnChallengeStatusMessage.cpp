#include "types.hpp"

#include "GameSource/Network/Messages/BrnFburnChallengeStatusMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::FburnChallengeStatusMessage::Construct             @ 0x825806C0
//   BrnNetwork::FburnChallengeStatusMessage::GetName              @ 0x8257C808
//   BrnNetwork::FburnChallengeStatusMessage::GetPackedMessageSize  @ 0x8257FA08
//   BrnNetwork::FburnChallengeStatusMessage::PackOrUnpack          @ 0x8257C7B0
//   BrnNetwork::FburnChallengeStatusMessage::PrepareForSend        @ 0x8257C818
//   BrnNetwork::FburnChallengeStatusMessage::Retrieve              @ 0x8257FA30
//
// A reliable message carrying one player's freeburn-challenge completion bit set. The
// 256-byte payload is zero-filled at Construct (and re-zeroed by GetPackedMessageSize,
// which the X360 build uses as the "describe an empty instance" path before deferring to
// the base reliable size). PrepareForSend / Retrieve copy the whole 256-byte buffer in/out
// and gate on the inherited VALID flag (Message::mx8Flags bit at +0x19).
//
// The X360 GetPackedMessageSize tail-calls a COMDAT-folded copy of the reliable base size
// (the binary resolved the shared stub to TestConnectionMessage::GetPackedMessageSize);
// semantically it is CgsNetwork::ReliableMessage::GetPackedMessageSize(), reproduced as
// such. PackOrUnpack ORs the base reliable status with the buffer-field status.
//
// Message type id 35 (0x23) is the reliable freeburn-challenge-status wire type.

namespace BrnNetwork
{
    static const s32 KI_FBURN_CHALLENGE_STATUS_MESSAGE_TYPE = 35;

    void FburnChallengeStatusMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();

        // Zero the 256-byte completion payload (32 x 64-bit stores at +0x28).
        for (u32 luWord = 0; luWord < FburnChallengeStatusPayload::KU_NUM_BIT_WORDS; ++luWord)
            mCompletedChallenges.maxBits[luWord] = 0;
    }

    const char* FburnChallengeStatusMessage::GetName() const
    {
        return "Freeburn Challenge Completion Status Message";
    }

    s32 FburnChallengeStatusMessage::GetPackedMessageSize()
    {
        // The X360 build re-zeroes the payload before reporting the base reliable size.
        for (u32 luWord = 0; luWord < FburnChallengeStatusPayload::KU_NUM_BIT_WORDS; ++luWord)
            mCompletedChallenges.maxBits[luWord] = 0;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult FburnChallengeStatusMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        const CgsNetwork::PackOrUnpackResult lxBuffer =
            CgsNetwork::PackOrUnpackBuffer(this,
                                           reinterpret_cast<u8*>(&mCompletedChallenges),
                                           0x100);
        return lxBuffer | lxBase;
    }

    void FburnChallengeStatusMessage::PrepareForSend(u16 lu16FrameCount,
                                                     const FburnChallengeStatusPayload* lpCompletedChallenges)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            memcpy(&mCompletedChallenges, lpCompletedChallenges, 0x100);
            CgsNetwork::ReliableMessage::PrepareForSend(KI_FBURN_CHALLENGE_STATUS_MESSAGE_TYPE,
                                                        lu16FrameCount);
        }
    }

    bool FburnChallengeStatusMessage::Retrieve(FburnChallengeStatusPayload* lpCompletedChallenges)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        memcpy(lpCompletedChallenges, &mCompletedChallenges, 0x100);
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
} // namespace BrnNetwork
