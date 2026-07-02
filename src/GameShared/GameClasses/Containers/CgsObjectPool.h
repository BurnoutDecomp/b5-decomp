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

    // Reset the pool to its all-free state. ADDITIVE GROW: the per-instantiation
    // bodies are real X360 symbols (e.g. SceneManagerEntity<10000,s32>::Clear /
    // VolumeInstance<5048,s32>::Clear called by EntityManager::Prepare
    // @0x828C5FDC/@0x828C5FE8); declaration-only here (each instantiation's body
    // is its own ledger function).
    void Clear();

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

    // Find the first allocated slot whose object compares equal to lrTarget (via T::operator==)
    // and return its slot index, or -1 when no allocated slot matches. Only instantiated for
    // element types that define operator== (it is not instantiated unless this method is named).
    //
    // FLAG (behaviour-faithful reconstruction, not instruction-identical): the recovered body
    // walks ONLY the allocated slots by stepping through the occupancy BitArray's set bits with a
    // lowest-set-bit scan, then compares each allocated object's fields against the target. This
    // linear walk over allocated slots is value-equivalent: GetFirstNonZeroBit / GetNextNonZeroBit
    // enumerate exactly the same set bits in the same ascending order, and the field-by-field
    // compare is precisely what T::operator== performs. Empty (unallocated) slots are skipped in
    // both forms, so the returned index is identical.
    TIndex FindObject(const T& lrTarget) const
    {
        for (s32 liIndex = mObjectsAllocated.GetFirstNonZeroBit();
             liIndex >= 0;
             liIndex = mObjectsAllocated.GetNextNonZeroBit(liIndex))
        {
            if (static_cast<s32>(liIndex) >= KI_CAPACITY)
            {
                break;
            }
            if (maObjectPool[static_cast<s32>(liIndex)] == lrTarget)
            {
                return static_cast<TIndex>(liIndex);
            }
        }
        return static_cast<TIndex>(-1);
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

    // Reach the inline object at a pool slot index (callers resolve a slot via the free
    // queue / their own order array, then index the pool by that slot). The X360 body
    // (e.g. 0x8231A2E8, the ObjectPool<StoredLeapingData,7,s32>::operator[] instantiation in the
    // class:BrnGameState catch-all TU) bounds-checks the index ("Array index out of bounds",
    // CgsObjectPool.h:218) AND verifies the slot is currently allocated ("The referenced object has
    // not been allocated", CgsObjectPool.h:219) before returning the object -- both are non-gating
    // tripwire asserts. The allocated-bit check is an ADDITIVE tripwire here (it never changes the
    // happy-path result); the existing single-bounds-check users are behaviour-unaffected.
    T& operator[](TIndex liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_CAPACITY), "Array index out of bounds");
        CGS_ASSERT(mObjectsAllocated.IsBitSet(static_cast<u32>(liIndex)), "The referenced object has not been allocated");
        return maObjectPool[static_cast<s32>(liIndex)];
    }
    const T& operator[](TIndex liIndex) const
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_CAPACITY), "Array index out of bounds");
        CGS_ASSERT(mObjectsAllocated.IsBitSet(static_cast<u32>(liIndex)), "The referenced object has not been allocated");
        return maObjectPool[static_cast<s32>(liIndex)];
    }

private:
    T                  maObjectPool[N];
    TIndex             maiObjectFreeQueue[N];
    s32                miNumObjectsFree;
    CgsContainers::BitArray<N> mObjectsAllocated;
};
}
