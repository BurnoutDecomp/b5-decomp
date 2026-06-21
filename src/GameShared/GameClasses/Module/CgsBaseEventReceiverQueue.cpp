#include "CgsBaseEventReceiverQueue.h"

namespace CgsModule
{
BaseEventReceiverQueue* BaseEventReceiverQueue::Clear()
{
    miCount = 0;

    // An un-constructed / empty queue (capacity 0) has nothing to clear; guard the modulo so a
    // Clear() before the owning module's Construct sizes the queue doesn't divide by zero. (The X360
    // always Constructs the queue first; this is a PC safety for the incremental bring-up.)
    if (miCapacity <= 0)
    {
        miWriteOffset = 0;
        miReadOffset  = 0;
        return this;
    }

    s32 liWriteOffset = miCapacity - static_cast<s32>(muBaseOffset % static_cast<u32>(miCapacity)) - 8;
    while (liWriteOffset < 0)
    {
        liWriteOffset += miCapacity;
    }

    miWriteOffset = liWriteOffset;
    miReadOffset = miWriteOffset;
    return this;
}
}
