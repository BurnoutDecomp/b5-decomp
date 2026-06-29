#pragma once

// ===================================================================================
// BrnNetwork::ActiveFburnChallengeMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnActiveFburnChallengeMessage.h
//
// SHAPE from DecFIGS DWARF (BrnActiveFburnChallengeMessage.h:46) gated against the X360
// binary. A reliable broadcast announcing which players are currently taking part in a
// given freeburn challenge: a fixed-size table of network-player ids, the challenge id,
// and the count of players in it, laid out after the 0x28-byte ReliableMessage base.
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x8257CBD8 / PackOrUnpack @ 0x8257CC80
// / Retrieve @ 0x8257FCC0):
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28 -- the +0x20/+0x24 player ids
//           set to -1 by this ctor, then chains to Message::Construct)
//   +0x28  NetworkPlayerID maPlayersInChallengeIDs[7]   (28 bytes; XMemSet(+0x28,0,28))
//   +0x48  CgsID           mChallengeID                  (8 bytes; std at +0x48)
//   +0x50  s32             miNumPlayersInChallenge       (stw at +0x50)
//
// The 4-byte gap between the 0x44-end of the id table and the 0x48 CgsID is natural
// 8-byte alignment for the CgsID (u64); no member lives there.
//
// The X360 build models the vtable as Message::mpVTable (no C++ `virtual`); these are
// plain methods. Bodies live in BrnActiveFburnChallengeMessage.cpp.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // CgsID (== u64)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"           // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // Reliable broadcast of the active-freeburn-challenge roster: which challenge, how
    // many players are in it, and their network-player ids.
    struct ActiveFburnChallengeMessage : CgsNetwork::ReliableMessage
    {
        // KI_MAX_NETWORK_PLAYERS == 7 (the [0,7] wire range and the array bound the X360
        // build hard-codes -- the PrepareForSend/Retrieve asserts read `> 0` and `<= 7`).
        static const s32 KI_MAX_NETWORK_PLAYERS = 7;

        NetworkPlayerID maPlayersInChallengeIDs[KI_MAX_NETWORK_PLAYERS]; // +0x28 (7 * 4)
        CgsID           mChallengeID;                                    // +0x48
        s32             miNumPlayersInChallenge;                         // +0x50

        void                          Construct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     CgsID lChallengeID,
                                                     NetworkPlayerID* lpaPlayersInChallengeIDs,
                                                     s32 liNumPlayersInChallenge);
        bool                          Retrieve(CgsID* lpChallengeID,
                                               NetworkPlayerID* lpaNetworkPlayerIDs,
                                               s32* lpiNumPlayersInChallenge);
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
