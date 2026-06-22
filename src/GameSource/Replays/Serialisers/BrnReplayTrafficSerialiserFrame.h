#pragma once

// BrnReplays::TrafficSerialiserFrame -- the per-frame snapshot the traffic replay
// serialiser records/plays back. No DWARF or prior source recovered for this type,
// so only the fields that Clear @0x826533D8 touches are known; everything before
// them is held as an opaque leading block. All access is BY NAME.
//
// X360 reference: Clear @0x826533D8 (called by TrafficEntityModule::UpdateSerialiser).

#include "types.hpp"

namespace BrnReplays
{
    // One slot of the traffic frame's reset table. Clear @0x826533D8 zeroes each slot
    // (asm `li r11,0; std r11,0xA0..0xF0` -- a 64-bit ZERO store; both words become 0).
    // The Hex-Rays 0xFFFFFFFF00000000 literal is a misread of the std-of-r11=0.
    struct TrafficFrameSlot
    {
        s32 miHandle; // @+0x00 reset to 0 (asm std r11=0 zeroes the whole slot)
        s32 miValue;  // @+0x04 reset to 0
    };

    // Number of reset slots Clear writes: 11 eight-byte stores from +0xA0 to +0xF0.
    static const u32 KU_TRAFFIC_FRAME_SLOTS = 11;

    // Opaque per-frame traffic state. Only the region Clear touches is named; the
    // leading bytes [0x00 .. 0x90) are an undecoded block (no recovered layout). GROW
    // this home into named fields if/when a DWARF layout is recovered.
    struct TrafficSerialiserFrame
    {
        u8  maHead[0x90];                            // @0x00 undecoded leading state
        s32 miFrameCounter;                          // @0x90 reset to 0 by Clear (stw)
        s16 miActiveCount;                           // @0x94 reset to -1 by Clear (sth -- 16-bit)
        s16 mPad96;                                  // @0x96 untouched by Clear
        // @0x98 .. @0x9F : two undecoded words between the counters and the slot table.
        u8  maGap[0x08];                             // @0x98 undecoded
        TrafficFrameSlot maSlots[KU_TRAFFIC_FRAME_SLOTS]; // @0xA0 reset table (11 slots)

        // @0x826533D8 -- reset the frame's slot table and counters to the empty state
        // (declared here, defined in BrnReplayTrafficSerialiserFrame.cpp).
        TrafficSerialiserFrame* Clear();
    };
}
