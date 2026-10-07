#pragma once

#include "types.hpp"
#include <cstddef>

namespace BrnReplays
{
    // DecFIGS BrnReplayFrunkHeader.h:42; ARTIST ReadCurrentFrunk reads eight
    // bytes per entry and indexes the result's arrays by this id.
    struct FrunkSerialiserEntry
    {
        s32 miId;
        s32 miSize;
    };

    // ARTIST AddFrunk 8265EEA8..B8 stores frame/flags/time/count at header
    // +8/+10/+14/+18. The frame is 64-bit; the older PS3's 16-bit field is
    // not the target shape. ReadCurrentFrunk copies the complete 32-byte header.
    struct FrunkHeader
    {
        char macMagicNumber[4];
        s64 miFrameNumber;
        u16 mxFlags;
        f32 mfFrameTime;
        s32 miNumSerialisers;
    };

    static_assert(sizeof(FrunkSerialiserEntry) == 8, "original frunk entry stride");
    static_assert(sizeof(FrunkHeader) == 32, "ARTIST frunk header size");
    static_assert(offsetof(FrunkHeader, miFrameNumber) == 8, "64-bit frame at +8");
    static_assert(offsetof(FrunkHeader, mxFlags) == 0x10, "flags at +10");
    static_assert(offsetof(FrunkHeader, mfFrameTime) == 0x14, "time at +14");
    static_assert(offsetof(FrunkHeader, miNumSerialisers) == 0x18, "count at +18");
}
