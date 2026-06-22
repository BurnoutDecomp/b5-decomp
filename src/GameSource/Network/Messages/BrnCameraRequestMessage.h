#pragma once

// ===================================================================================
// BrnNetwork::CameraRequestMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCameraRequestMessage.h
//
// SHAPE from DecFIGS DWARF (BrnCameraRequestMessage.h:43) gated against the X360 binary.
// Construct @ 0x8257ADC0 initialises the inherited MessageWithPlayerIDs ids at +0x20/
// +0x24 (to -1) then stores its one bool at +0x28, i.e. immediately after the 0x28-byte
// CgsNetwork::ReliableMessage base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  bool mbReleaseFeed
//
// The base (CgsNetwork::ReliableMessage) is reused BY NAME from its committed home header
// (CgsReliableMessage.h: Message -> MessageWithPlayerIDs -> ReliableMessage) -- not forked.
//
// LEDGER FUNCTIONS in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   Construct/GetPackedMessageSize/PackOrUnpack/PrepareForSend/Retrieve -> bodied in the
//     sibling BrnCameraRequestMessage.cpp.
//   GetName @ 0x827DFD80 -> returns the literal "Camera Request Message" (no member/base
//     access); bodied inline here.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // Reliable request from a spectator to start/stop receiving another player's
    // camera feed. mbReleaseFeed distinguishes a request (false) from a release (true).
    struct CameraRequestMessage : CgsNetwork::ReliableMessage
    {
        bool mbReleaseFeed;     // +0x28

        void                           Construct();
        void                           PrepareForSend(u16 lu16FrameCount, bool lbReleaseFeed);
        bool                           Retrieve(bool* lpbReleaseFeed);
        s32                            GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                    GetName() const;
    };

    // BrnNetwork::CameraRequestMessage::GetName @ 0x827DFD80.
    inline const char* CameraRequestMessage::GetName() const
    {
        return "Camera Request Message";
    }
} // namespace BrnNetwork
