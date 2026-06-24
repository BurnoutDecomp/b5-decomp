// BrnAI::HardNoGoMap -- MapSquareOccupiedFast (@0x82764898) and
// SetMapSquare (@0x82764A80).
//
// Both are simple bit-grid accessors over the 32x8 map: one 32-bit row word per
// height index, one bit per width index. The X360 reads/writes the row word at
// 4*(height+8) (== mauMap[height]) and tests/sets bit (1<<width). Each function
// front-loads the same range/ready asserts the X360 emits (the baked d:\p4
// BrnHardNoGoMap.h file/line is replaced by CGS_ASSERT's __FILE__/__LINE__).

#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnAI
{
// @0x82764898
// asm: load mauMap[height] (lwzx @ 4*(height+8)), AND with (1<<width), then the
// subf/cntlzw/extrwi idiom returns (word & (1<<width)) != 0 as a BOOL.
bool HardNoGoMap::MapSquareOccupiedFast(u32 luWidth, u32 luHeight) const
{
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready\n");
    CGS_ASSERT(luWidth < KU_WIDTH, "Bad width index ");
    CGS_ASSERT(luHeight < KU_HEIGHT, "Bad height index ");

    const u32 luBit = 1u << luWidth;
    return (mauMap[luHeight] & luBit) == luBit;
}

// @0x82764A80
// asm: load mauMap[height] (lwzx @ 4*(height+8)), OR in (1<<width), store back
// (stwx). The ready/range asserts fire first; the bit set is unconditional.
void HardNoGoMap::SetMapSquare(u32 luWidth, u32 luHeight)
{
    CGS_ASSERT(luWidth < KU_WIDTH, "Bad Width of ");
    CGS_ASSERT(luHeight < KU_HEIGHT, "Bad Height of ");
    CGS_ASSERT(mbReady, "Hard No Go Secton not ready\n");

    mauMap[luHeight] |= (1u << luWidth);
}
}
