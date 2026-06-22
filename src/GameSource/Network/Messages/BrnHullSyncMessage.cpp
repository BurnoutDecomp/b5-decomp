#include "types.hpp"

#include "GameSource/Network/Messages/BrnHullSyncMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::HullSyncMessage::Construct       @ 0x8257DC38
//   BrnNetwork::HullSyncMessage::PrepareForSend  @ 0x8257DC78
//   BrnNetwork::HullSyncMessage::Retrieve        @ 0x8257DD58
//   (Release @ 0x8257DD40 lives in CgsMessageSubclasses.cpp -- its own TU.)
//
// The whole payload is the BufferedHullsToActivate array; the X360 build moves it in and
// out with a 48-byte memcpy, reproduced here as a struct copy of the named member. The
// array's Construct/Clear/GetLength assert sites (CgsArray.h) are the inlined helpers the
// pseudocode shows as the "Array used before Construct/Clear" / "GetLength() > 0" checks.

namespace BrnNetwork
{
    void HullSyncMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mBufferedHullActivates.Construct();     // live-count -> 0 (stw 0, +0x54)
    }

    void HullSyncMessage::PrepareForSend(u16 lu16FrameCount,
                                         BufferedHullsToActivate* laBufferedHullActivates)
    {
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!ReliableMessage::IsMessageValid()");
        CGS_ASSERT(laBufferedHullActivates->GetLength() > 0,
                   "laBufferedHullActivates->GetLength() > 0");

        mBufferedHullActivates = *laBufferedHullActivates;      // 48-byte memcpy in
        CgsNetwork::ReliableMessage::PrepareForSend(13, lu16FrameCount);
    }

    bool HullSyncMessage::Retrieve(BufferedHullsToActivate* laBufferedHullActivates)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *laBufferedHullActivates = mBufferedHullActivates;      // 48-byte memcpy out
        CGS_ASSERT(mBufferedHullActivates.GetLength() > 0,
                   "maBufferedHullActivates.GetLength() > 0");

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        mBufferedHullActivates.Clear();                          // live-count -> 0 (+0x54)
        CGS_ASSERT(mBufferedHullActivates.GetLength() == 0,
                   "maBufferedHullActivates.GetLength() == 0");
        return true;
    }
}
