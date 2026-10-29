#pragma once
#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

namespace BrnReplays
{
    // ARTIST Clear 0x826533D8 and producer 0x8272DA80 establish this 256-byte prefix.
    // Set<u16,72> is the module's active hull set: 144 element bytes then its count.
    struct TrafficSerialiserFrame
    {
        ::Set<u16,72> mHeader;                         // +0x00, count at +0x90
        u16 muLocalPlayerHull;                        // +0x94, invalid = 0xFFFF
        u8 muNumBonusAssets;                          // +0x96
        u8 maBonusAssets[2];                          // +0x97
        u8 maPad0099[7];
        CgsContainers::FastBitArray<600> mVehicleUpdateBits; // +0xA0
        CgsContainers::FastBitArray<25> mPhysicsBits;   // +0xF0
        u8 maPad00F8[8];

        TrafficSerialiserFrame* Clear();
    };
    static_assert(sizeof(TrafficSerialiserFrame) == 256, "Traffic frame prefix");
}
