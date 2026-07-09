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

    // Reset the pool to its all-free state. X360 (SceneManagerEntity<10000,s32>::Clear
    // @0x828B8E30 / VolumeInstance<5048,s32>::Clear, both called by EntityManager::Prepare
    // @0x828C5FC8): memset the occupancy BitArray to 0, refill the free queue DESCENDING
    // (maiObjectFreeQueue[i] = N-1-i, i.e. N-1 down to 0 stored front-to-back so AllocateObject
    // hands out slot 0 first), then miNumObjectsFree = N. INLINE because Clear() is called from
    // multiple separate instantiation TUs (EntityManager::Prepare instantiates both
    // <SceneManagerEntity,10000> and <VolumeInstance,5048>); an out-of-line body in one .cpp
    // would not be visible to the other TU.
    void Clear()
    {
        mObjectsAllocated.UnSetAll();
        for (s32 liSlot = N - 1; liSlot >= 0; --liSlot)
        {
            maiObjectFreeQueue[N - 1 - liSlot] = static_cast<TIndex>(liSlot);
        }
        miNumObjectsFree = N;
    }

    // Lowest ALLOCATED slot index, or -1 when the pool is empty. The X360 body
    // (0x828C3538) inlines the occupancy-BitArray lowest-set-bit scan; forwarding to
    // BitArray<N>::GetFirstNonZeroBit is value-identical. DWARF CgsObjectPool.h:298.
    s32 GetFirstObjectIndex() const
    {
        return mObjectsAllocated.GetFirstNonZeroBit();
    }

    // Lowest ALLOCATED slot index strictly greater than liCurrentIndex, or -1 when none
    // remains. The X360 body (0x828C35A0) inlines the same scan starting after
    // liCurrentIndex; forwarding to BitArray<N>::GetNextNonZeroBit is value-identical.
    // DWARF CgsObjectPool.h:306.
    s32 GetNextObjectIndex(s32 liCurrentIndex) const
    {
        return mObjectsAllocated.GetNextNonZeroBit(liCurrentIndex);
    }

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

    // Number of currently-free slots (the pool's own running free counter). The X360 reads
    // this word directly before allocating (e.g. BehaviourManager::AllocateBehaviour's
    // out-of-slots pre-check) -- exposed by name so callers never poke the counter by offset.
    s32 GetNumFreeObjects() const { return miNumObjectsFree; }

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
