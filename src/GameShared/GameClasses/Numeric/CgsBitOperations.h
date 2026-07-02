#pragma once

#include "types.hpp"

// CgsNumeric::BitOperations - small bit-twiddling helpers. DWARF home
// CgsBitOperations.h:5. GetNumberOfSetBits is header-inline on the X360 (no
// standalone symbol); the body below is the exact SWAR population count the
// inlined instances emit (e.g. the two counts in BrnDirector::ShotSelector::
// GetCrashShot @0x82239958..0x82239A00: pairwise subtract, nibble sums, the
// 0x01010101 multiply, and the >>24).
namespace CgsNumeric
{
namespace BitOperations
{
    inline s32 GetNumberOfSetBits(u32 luValue)
    {
        u32 luCount = luValue - ((luValue >> 1) & 0x55555555u);
        luCount = ((luCount >> 2) & 0x33333333u) + (luCount & 0x33333333u);
        luCount = ((luCount >> 4) + luCount) & 0x0F0F0F0Fu;
        return static_cast<s32>((luCount * 0x01010101u) >> 24);
    }
}
}
