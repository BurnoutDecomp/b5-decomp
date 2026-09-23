#pragma once

// ===================================================================================
// BrnNetwork::ShowtimeUpdateMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnShowtimeUpdateMessage.h
//
// SHAPE from DecFIGS DWARF (BrnShowtimeUpdateMessage.h:42) gated against the X360
// binary. Construct @ 0x8257C340 stores its single int field at +0x20, i.e. directly
// after the 0x20-byte CgsNetwork::Message base, so:
//   +0x00  (CgsNetwork::Message base)
//   +0x20  s32 miShowtimeScore
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace BrnNetwork
{
    // Carries a player's current showtime score; sent unreliably each showtime frame.
    struct ShowtimeUpdateMessage : CgsNetwork::Message
    {
        s32 miShowtimeScore;        // +0x20

        void                          Construct();
        void                          PrepareForSend(u16 lu16FrameCount, s32 liShowtimeScore);
        bool                          Retrieve(s32* lpiShowtimeScore);
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;
        s32                           GetPackedMessageSize() override;
        const char*                   GetName() const override;
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(ShowtimeUpdateMessage) == 0x24, "sizeof(ShowtimeUpdateMessage) == 0x24");
} // namespace BrnNetwork
