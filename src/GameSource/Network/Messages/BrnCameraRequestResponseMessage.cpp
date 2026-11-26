#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnCameraRequestResponseMessage.cpp"
#include "GameSource/Network/Messages/BrnCameraRequestResponseMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CameraRequestResponseMessage::Construct       @ 0x8257AE88
//   BrnNetwork::CameraRequestResponseMessage::PackOrUnpack    @ 0x8257AEF0
//   BrnNetwork::CameraRequestResponseMessage::PrepareForSend  @ 0x8257AEC8
//   BrnNetwork::CameraRequestResponseMessage::Retrieve        @ 0x8257E6C8
//
// Reliable accept/decline reply to a camera feed request. The EResponse enum is
// (de)serialised through the shared 32-bit int primitive via a scratch int (the
// quantiser works on s32), exactly as the X360 build does.

namespace BrnNetwork
{
    void CameraRequestResponseMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        meCameraRequestResponse = E_RESPONSE_DECLINE;
    }

    CgsNetwork::PackOrUnpackResult CameraRequestResponseMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();

        s32 liResponse = meCameraRequestResponse;
        const CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::Message::PackOrUnpack(&liResponse, 0, E_RESPONSE_COUNT) | lxBase;
        meCameraRequestResponse = static_cast<EResponse>(liResponse);
        return lxResult;
    }

    void CameraRequestResponseMessage::PrepareForSend(u16 lu16FrameCount, EResponse leResponse)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            meCameraRequestResponse = leResponse;
            CgsNetwork::ReliableMessage::PrepareForSend(22, lu16FrameCount);
        }
    }

    bool CameraRequestResponseMessage::Retrieve(EResponse* lpeCameraRequestResponse)
    {
        CGS_ASSERT_AT(lpeCameraRequestResponse,
            "lpeCameraRequestResponse",
            CGS_ARTIST_FILE, 103 /* ARTIST 0x8257E700 */);

        // ARTIST 0x8257E714 branches to a trailing `return false` (0x8257E760): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpeCameraRequestResponse = meCameraRequestResponse;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

            CGS_ASSERT_AT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                "!CgsNetwork::ReliableMessage::IsMessageValid()",
                CGS_ARTIST_FILE, 113 /* ARTIST 0x8257E74C */);
            return true;
        }
        return false;
    }

    // Zero the response word, then size through the reliable base (the console body is one
    // folded copy shared with CheckpointTriggeredMessage::GetPackedMessageSize).
    s32 CameraRequestResponseMessage::GetPackedMessageSize()
    {
        meCameraRequestResponse = E_RESPONSE_DECLINE;
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }
}

#undef CGS_ARTIST_FILE
