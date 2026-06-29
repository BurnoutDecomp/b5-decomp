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

// @ 0x828470D0 - post-increment the prepare-stage index. Capture the current value, store
// the incremented value back through the reference, assert it has not overrun E_PREPARE_DONE,
// and return the value held before the increment (X360 returns the saved old value in r3).
EventInterpreterModule::EventInterpreterPrepareStage operator++(
    EventInterpreterModule::EventInterpreterPrepareStage& lrStage, int)
{
    const EventInterpreterModule::EventInterpreterPrepareStage leOldStage = lrStage;
    lrStage = static_cast<EventInterpreterModule::EventInterpreterPrepareStage>(leOldStage + 1);
    CGS_ASSERT(lrStage <= EventInterpreterModule::E_PREPARE_DONE,
        "leEnumIndex <= EventInterpreterModule::E_PREPARE_DONE");
    return leOldStage;
}
}
