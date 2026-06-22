#include "GameSource/Replays/Serialisers/BrnReplayTrafficSerialiserFrame.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::TrafficSerialiserFrame).
//
//   Clear @ 0x826533D8
//
// Store-for-store vs asm @0x826533D8: `li r11,0; li r10,-1; std r11,0xA0..0xF0` writes a
// 64-bit ZERO to each of the 11 slots (both handle and value = 0; the Hex-Rays
// 0xFFFFFFFF00000000 literal is a misread of the std-of-r11=0). `sth r10,0x94` is a
// 16-bit halfword store of -1 to miActiveCount (2 bytes), and `stw r11,0x90` zeroes the
// 32-bit frame counter.
namespace BrnReplays
{
    // @ 0x826533D8
    TrafficSerialiserFrame* TrafficSerialiserFrame::Clear()
    {
        for (u32 luSlot = 0; luSlot < KU_TRAFFIC_FRAME_SLOTS; ++luSlot)
        {
            maSlots[luSlot].miHandle = 0;   // std r11(=0): the whole 8-byte slot is zeroed
            maSlots[luSlot].miValue  = 0;
        }

        miActiveCount  = -1;                // sth r10(-1),0x94 -- 16-bit halfword
        miFrameCounter = 0;                 // stw r11(0),0x90
        return this;
    }
}
