#include "types.hpp"

#include "GameSource/Network/Messages/BrnCameraStatusMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CameraStatusMessage::Construct            @ 0x8257AD20
//   BrnNetwork::CameraStatusMessage::GetPackedMessageSize @ 0x8257C378
//   BrnNetwork::CameraStatusMessage::PackOrUnpack         @ 0x8257AD58
//
// A network message carrying a single camera-status word (meCameraStatus, +0x20 on the
// console). Construct/GetPackedMessageSize delegate to the CgsNetwork::Message base after
// resetting that word. PackOrUnpack ORs the base Message::PackOrUnpack() status with the
// word serialised through the 32-bit int field primitive in [0, E_CAMERA_STATUS_COUNT].

namespace BrnNetwork
{
    void CameraStatusMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        meCameraStatus = E_CAMERA_STATUS_NONE;
    }

    s32 CameraStatusMessage::GetPackedMessageSize()
    {
        meCameraStatus = E_CAMERA_STATUS_NONE;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult CameraStatusMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::Message::PackOrUnpack();

        // The enum travels through the int primitive via a stack temporary.
        s32 liCameraStatus = static_cast<s32>(meCameraStatus);
        const CgsNetwork::PackOrUnpackResult lxStatus =
            CgsNetwork::PackOrUnpackInt(this, &liCameraStatus, 0, E_CAMERA_STATUS_COUNT);
        meCameraStatus = static_cast<ECameraStatus>(liCameraStatus);

        return lxStatus | lxBase;
    }
} // namespace BrnNetwork
