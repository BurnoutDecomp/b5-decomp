#include "types.hpp"

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
            CgsNetwork::PackOrUnpackInt(this, &liResponse, 0, E_RESPONSE_COUNT) | lxBase;
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
        CGS_ASSERT(lpeCameraRequestResponse, "lpeCameraRequestResponse");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpeCameraRequestResponse = meCameraRequestResponse;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0, "!CgsNetwork::ReliableMessage::IsMessageValid()");

        return true;
    }
}
