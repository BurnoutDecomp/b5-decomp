#pragma once

// ===================================================================================
// BrnNetwork::FburnChallengeStatusMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnFburnChallengeStatusMessage.h
//
// SHAPE from DecFIGS DWARF (BrnFburnChallengeStatusMessage.h:45) gated against the X360
// binary. A reliable broadcast of which freeburn challenges a player has completed: it
// carries a single 256-byte completion bit set after the 0x28-byte ReliableMessage base.
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x825806C0 / PackOrUnpack @ 0x8257C7B0):
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28 -- the +0x20/+0x24 player ids set
//           to -1 by this ctor, then chains to Message::Construct)
//   +0x28  CompletedFburnChallenges mCompletedChallenges   (256 bytes / 0x100)
//
// Construct zero-fills the 256-byte payload as 32 x 64-bit stores starting at +0x28
// (stdx loop, 8*(i+5)+this for i in [0,0x20)); PackOrUnpack routes the payload through
// the shared fixed-length buffer (de)serialiser at +0x28 length 0x100.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // CompletedFburnChallenges (256B bit set)

namespace BrnNetwork
{
    // The completion bit set type the message carries (FastBitArray<2000>, 256 bytes),
    // reused BY NAME from its committed GameState home.
    typedef BrnGameState::GameStateModuleIO::CompletedFburnChallenges
        FburnChallengeStatusPayload;

    struct FburnChallengeStatusMessage : CgsNetwork::ReliableMessage
    {
        FburnChallengeStatusPayload mCompletedChallenges;   // +0x28 (256 bytes)

        void                          Construct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     const FburnChallengeStatusPayload* lpCompletedChallenges);
        bool                          Retrieve(FburnChallengeStatusPayload* lpCompletedChallenges);
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
