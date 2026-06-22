#pragma once

// ===================================================================================
// BrnNetwork::FreeburnChallengeMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnFreeburnChallengeMessage.h
//
// SHAPE from DecFIGS DWARF (BrnFreeburnChallengeMessage.h:43) gated against the X360
// binary. Construct @ 0x8257C508 inits the inherited player ids at +0x20/+0x24 (-1)
// then lays out a 64-bit id and three 32-bit fields after the 0x28-byte ReliableMessage
// base (offsets confirmed by Construct/PackOrUnpack/Retrieve):
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  CgsID                 mChallengeID          (8 bytes)
//   +0x30  EChallengeEventType   meEventType
//   +0x34  EChallengeStatus      meChallengeStatus
//   +0x38  s32                   miActionIndex
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // CgsID (== u64)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"                     // BrnNetworkModuleIO::EChallengeEventType
#include "GameSource/BurnoutConstants.h"                              // BrnGameState::EChallengeStatus

namespace BrnNetwork
{
    // Reliable broadcast of a freeburn-challenge state change: which challenge, what
    // happened to it, its resulting status, and the action index within it.
    struct FreeburnChallengeMessage : CgsNetwork::ReliableMessage
    {
        CgsID                               mChallengeID;       // +0x28
        BrnNetworkModuleIO::EChallengeEventType meEventType;    // +0x30
        BrnGameState::EChallengeStatus      meChallengeStatus;  // +0x34
        s32                                 miActionIndex;      // +0x38

        void                           Construct();
        bool                           Retrieve(CgsID* lpChallengeID,
                                                BrnNetworkModuleIO::EChallengeEventType* lpeEventType,
                                                BrnGameState::EChallengeStatus* lpeChallengeStatus,
                                                s32* lpiActionIndex);
        s32                            GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                    GetName() const;
    };
} // namespace BrnNetwork
