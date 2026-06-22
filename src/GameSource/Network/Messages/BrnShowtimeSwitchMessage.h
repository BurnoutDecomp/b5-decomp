#pragma once

// ===================================================================================
// BrnNetwork::ShowtimeSwitchMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnShowtimeSwitchMessage.h
//
// SHAPE from DecFIGS DWARF (BrnShowtimeSwitchMessage.h:42) gated against the X360
// binary. Construct @ 0x8257C3B0 inits the inherited player ids at +0x20/+0x24 (-1)
// then stores its payload at +0x28/+0x2C:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  s32  miFinalShowtimeScore
//   +0x2C  bool mbEnteringShowtime
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // Reliable notification that a player has entered or left showtime, carrying the
    // final showtime score at the switch point.
    struct ShowtimeSwitchMessage : CgsNetwork::ReliableMessage
    {
        s32  miFinalShowtimeScore;      // +0x28
        bool mbEnteringShowtime;        // +0x2C

        void                           Construct();
        void                           PrepareForSend(u16 lu16FrameCount, s32 liFinalShowtimeScore,
                                                       bool lbEnteringShowtime);
        bool                           Retrieve(s32* lpiFinalShowtimeScore, bool* lpbEnteringShowtime);
        s32                            GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                    GetName() const;
    };
} // namespace BrnNetwork
