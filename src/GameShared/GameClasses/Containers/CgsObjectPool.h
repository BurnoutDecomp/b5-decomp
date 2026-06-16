#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<N>

namespace CgsContainers
{
// Fixed-capacity object pool: N inline objects, a LIFO free-index queue, and an occupancy
// BitArray. Recovered from the X360 spine (CgsObjectPool.h) with member names from DWARF.
// Template method bodies are inline (shared by every instantiation); per-instantiation ledger
// TUs are explicit-instantiation .cpps.
template <typename T, s32 N, typename TIndex>
class ObjectPool
{
public:
    static const s32 KI_CAPACITY = N;

    // Pop the next free slot index, mark it allocated, and return it (-1 when the pool is empty).
    TIndex AllocateObject()
    {
        if (miNumObjectsFree <= 0)
        {
            return static_cast<TIndex>(-1);
        }
        --miNumObjectsFree;
        const TIndex liFreeObjectIndex = maiObjectFreeQueue[miNumObjectsFree];
        CGS_ASSERT(static_cast<s32>(liFreeObjectIndex) < KI_CAPACITY, "Array index out of bounds");
        mObjectsAllocated.SetBit(static_cast<u32>(liFreeObjectIndex));
        return liFreeObjectIndex;
    }

    // True when the slot at liIndex is currently allocated.
    bool IsObjectAllocated(TIndex liIndex) const
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_CAPACITY), "Array index out of bounds");
        return mObjectsAllocated.IsBitSet(static_cast<u32>(liIndex));
    }

    void FreeObject(TIndex liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_CAPACITY), "Array index out of bounds");
        CGS_ASSERT(miNumObjectsFree < KI_CAPACITY, "miNumObjectsFree < PoolSize");
        maiObjectFreeQueue[miNumObjectsFree] = liIndex;
        ++miNumObjectsFree;
        CGS_ASSERT(mObjectsAllocated.IsBitSet(static_cast<u32>(liIndex)), "The object isn't allocated");
        mObjectsAllocated.UnSetBit(static_cast<u32>(liIndex));
    }

private:
    T                  maObjectPool[N];
    TIndex             maiObjectFreeQueue[N];
    s32                miNumObjectsFree;
    CgsContainers::BitArray<N> mObjectsAllocated;
};
}
