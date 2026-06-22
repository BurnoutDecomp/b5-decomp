#include "types.hpp"

#include "GameSource/Network/Messages/BrnCameraRequestMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CameraRequestMessage::Construct            @ 0x8257ADC0
//   BrnNetwork::CameraRequestMessage::GetPackedMessageSize @ 0x8257AE78
//   BrnNetwork::CameraRequestMessage::PackOrUnpack         @ 0x8257AE28
//   BrnNetwork::CameraRequestMessage::PrepareForSend       @ 0x8257AE00
//   BrnNetwork::CameraRequestMessage::Retrieve             @ 0x8257E620
//
// A reliable message: the base ReliableMessage carries the wrapped reliable id (and
// the sending/receiving player ids), and this subclass adds a single boolean payload.

namespace BrnNetwork
{
    void CameraRequestMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1) ahead of
        // the Message ctor, exactly as the X360 build folds it.
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mbReleaseFeed = false;
    }

    s32 CameraRequestMessage::GetPackedMessageSize()
    {
        mbReleaseFeed = false;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult CameraRequestMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxReleaseFeed =
            CgsNetwork::PackOrUnpackBool(this, &mbReleaseFeed);
        return CgsNetwork::ReliableMessage::PackOrUnpack() | lxReleaseFeed;
    }

    void CameraRequestMessage::PrepareForSend(u16 lu16FrameCount, bool lbReleaseFeed)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            mbReleaseFeed = lbReleaseFeed;
            CgsNetwork::ReliableMessage::PrepareForSend(21, lu16FrameCount);
        }
    }

    bool CameraRequestMessage::Retrieve(bool* lpbReleaseFeed)
    {
        CGS_ASSERT(lpbReleaseFeed, "lpbReleaseFeed");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpbReleaseFeed = mbReleaseFeed;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
}
