#include "CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstdint>                                    // uintptr_t
#include <cstring>                                    // memcpy

namespace CgsModule
{
// @ 0x821F1D50 - reset to empty: count 0, start/write offset = the alignment-adjusted base so the
// first event's payload (at base + offset + 8) lands aligned.
BaseEventReceiverQueue* BaseEventReceiverQueue::Clear()
{
    miCount = 0;

    // A bare (un-Constructed) base has no buffer/alignment -- nothing to align. Guards a Clear() that
    // runs before the owning module sizes the queue (and the divide-by-zero it would cause).
    if (miAlignment <= 0 || mpBuffer == 0)
    {
        miStartOffset = 0;
        miWriteOffset = 0;
        return this;
    }

    s32 liStart = miAlignment
                - static_cast<s32>(reinterpret_cast<uintptr_t>(mpBuffer) % static_cast<u32>(miAlignment))
                - 8;
    while (liStart < 0)
        liStart += miAlignment;

    miStartOffset = liStart;
    miWriteOffset = liStart;
    return this;
}

// @ 0x82663928 - append: write [type][size] header, copy the payload, advance past the pad-to-align.
bool BaseEventReceiverQueue::AddEvent(const Event* lpEvent, s32 liType, s32 liSize)
{
    s32 liPad = 0;
    const s32 liMod = (liSize + 8) % miAlignment;
    if (liMod)
        liPad = miAlignment - liMod;

    CGS_ASSERT(miWriteOffset + liPad + liSize + 8 <= miCapacity, "Queue overflow\n");

    s32* lpHeader = reinterpret_cast<s32*>(mpBuffer + miWriteOffset);
    lpHeader[0] = liType;
    lpHeader[1] = liSize;
    memcpy(mpBuffer + miWriteOffset + 8, lpEvent, static_cast<size_t>(liSize));

    miWriteOffset += liPad + liSize + 8;
    ++miCount;
    return true;
}

// Read the first queued event at miStartOffset (the X360 ProcessReceiverQueue inlines this).
s32 BaseEventReceiverQueue::GetFirstEvent(const Event** lppEventData, s32* lpiSize) const
{
    if (miCount <= 0)
    {
        *lppEventData = 0;
        *lpiSize = 0;
        return -1;
    }
    const s32* lpHeader = reinterpret_cast<const s32*>(mpBuffer + miStartOffset);
    *lppEventData = reinterpret_cast<const Event*>(lpHeader + 2);   // payload follows the 8-byte header
    *lpiSize = lpHeader[1];
    return lpHeader[0];
}

// @ 0x82204690 - walk to the event after lpPrevData (its payload pointer). Returns -1 at the end.
s32 BaseEventReceiverQueue::GetNextEvent(const Event* lpPrevData, const Event** lppNextData, s32* lpiSize) const
{
    const s32 liPrevHeader = static_cast<s32>(reinterpret_cast<const u8*>(lpPrevData) - mpBuffer - 8);
    CGS_ASSERT(liPrevHeader >= 0 && liPrevHeader < miWriteOffset, "Prev entry is invalid\n");

    const s32* lpPrevHeader = reinterpret_cast<const s32*>(mpBuffer + liPrevHeader);
    const s32 liPrevSize = lpPrevHeader[1];

    s32 liPad = 0;
    const s32 liMod = (liPrevSize + 8) % miAlignment;
    if (liMod)
        liPad = miAlignment - liMod;

    const s32 liNext = liPrevSize + liPad + liPrevHeader + 8;
    if (liNext >= miWriteOffset)
    {
        *lppNextData = 0;
        *lpiSize = 0;
        return -1;
    }

    const s32* lpNextHeader = reinterpret_cast<const s32*>(mpBuffer + liNext);
    *lppNextData = reinterpret_cast<const Event*>(lpNextHeader + 2);
    *lpiSize = lpNextHeader[1];
    return lpNextHeader[0];
}
}
