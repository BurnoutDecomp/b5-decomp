#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnCameraRequestMessage.cpp"
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
            CgsNetwork::Message::PackOrUnpack(&mbReleaseFeed);
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
        CGS_ASSERT(lpbReleaseFeed, CGS_ASSERT_MESSAGE_AT(
            "lpbReleaseFeed",
            CGS_ARTIST_FILE, 102 /* ARTIST 0x8257E658 */));

        // ARTIST 0x8257E66C branches to a trailing `return false` (0x8257E6B8): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpbReleaseFeed = mbReleaseFeed;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0, CGS_ASSERT_MESSAGE_AT(
                "!CgsNetwork::ReliableMessage::IsMessageValid()",
                CGS_ARTIST_FILE, 112 /* ARTIST 0x8257E6A4 */));
            return true;
        }
        return false;
    }
}

#undef CGS_ARTIST_FILE
