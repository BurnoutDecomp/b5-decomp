#pragma once

// ===================================================================================
// BrnNetwork::CameraRequestResponseMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCameraRequestResponseMessage.h
//
// SHAPE from DecFIGS DWARF (BrnCameraRequestResponseMessage.h:44) gated against the
// X360 binary. Construct @ 0x8257AE88 inits the inherited player ids at +0x20/+0x24
// (a1[8]/a1[9] = -1) then stores its enum at +0x28 (a1[10]):
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  EResponse meCameraRequestResponse
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // The reliable reply to a CameraRequestMessage: accept or decline a feed request.
    struct CameraRequestResponseMessage : CgsNetwork::ReliableMessage
    {
        // BrnCameraRequestResponseMessage.h:48
        enum EResponse
        {
            E_RESPONSE_DECLINE = 0,
            E_RESPONSE_ACCEPT  = 1,
            E_RESPONSE_COUNT   = 2,
        };

        EResponse meCameraRequestResponse;      // +0x28

        void                           Construct();
        void                           PrepareForSend(u16 lu16FrameCount, EResponse leResponse);
        bool                           Retrieve(EResponse* lpeCameraRequestResponse);
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };
} // namespace BrnNetwork
