#pragma once

// ===================================================================================
// BrnNetwork::StuntScoreUpdatedMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnStuntScoreUpdatedMessage.h
//
// A RELIABLE per-event message reporting one player's updated stunt score. Derives from
// CgsNetwork::ReliableMessage (which adds no data of its own; sizeof == 0x28), so the single
// leaf field lands directly after the 0x28-byte base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  s32 miStuntScore
//
// LAYOUT + behaviour are X360-AUTHORITATIVE (PackOrUnpack @ 0x8257D0E0, PrepareForSend
// @ 0x8257D158; asserts cited against GameSource/Network/Messages/BrnStuntScoreUpdatedMessage.cpp).
// The score is stored with a 32-bit `stw` at +0x28 (asm `stw r28, 0x28(r30)`).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    struct StuntScoreUpdatedMessage : CgsNetwork::ReliableMessage
    {
        s32 miStuntScore;        // +0x28

        // @ 0x8257D158 -- assert the inputs + that the slot is not already pending, store the
        // score, then stamp the reliable message (type 41) for the given frame.
        void                          PrepareForSend(u16 lu16FrameCount, s32 liStuntScore);
        // @ 0x8257D0E0 -- (de)serialise the score; ORs the base reliable id status with the
        // quantised int field status. 0 == success.
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;

        // Copy the score out and consume the message; an empty slot reports -1.
        bool                          Retrieve(s32* lpiStuntScore);
        s32                           GetPackedMessageSize() override;
        const char*                   GetName() const override;
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(StuntScoreUpdatedMessage) == 0x2C,
                  "sizeof(StuntScoreUpdatedMessage) == 0x2C");
} // namespace BrnNetwork
