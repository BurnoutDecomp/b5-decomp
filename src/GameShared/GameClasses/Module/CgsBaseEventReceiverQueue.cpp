#include "CgsBaseEventReceiverQueue.h"

namespace CgsModule
{
BaseEventReceiverQueue* BaseEventReceiverQueue::Clear()
{
    miCount = 0;

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
