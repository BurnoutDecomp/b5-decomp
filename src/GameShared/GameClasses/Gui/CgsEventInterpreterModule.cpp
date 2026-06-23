#include "CgsEventInterpreterModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::EventInterpreterModule::HashTableElement::HashTableElement
//
// Seeds the ten hash buckets with the empty-bucket sentinel. The compiler
// strength-reduced the fill into a pointer loop; it is re-rolled here.

namespace CgsGui
{
EventInterpreterModule::HashTableElement::HashTableElement()
{
    for (int i = 0; i < 10; ++i)
    {
        mBuckets[i] = 0xA00000000ull;
    }
}

// @ 0x8284E870 - zero the 600-bit subscription mask, then OR in the bit for each of
// liCount event indices supplied in lpaIndices. Each index is bounds-checked against
// KI_NUM_BITS (the X360 baked CgsBitArray.h:222 path/line into the failure assert).
EventInterpreterModule::sMapEntry::sMapEntry(const u32* lpaIndices, s32 liCount)
{
    for (s32 i = 0; i < KI_NUM_WORDS; ++i)
    {
        maBits[i] = 0;
    }

    for (s32 i = 0; i < liCount; ++i)
    {
        const u32 luIndex = lpaIndices[i];
        CGS_ASSERT(luIndex < static_cast<u32>(KI_NUM_BITS), "Index out of range\n");
        maBits[luIndex >> 6] |= (1ull << (luIndex & 0x3F));
    }
}
}
