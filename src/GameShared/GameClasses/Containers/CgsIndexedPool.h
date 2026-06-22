#ifndef GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
#define GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// GameShared/GameClasses/Containers/CgsIndexedPool.h
//
// CgsContainers::IndexedPool<T, N> -- a fixed-capacity object pool that hands out
// stable integer handles. Live objects live in maElements[]; the free slots are
// tracked by a list of free indices (mpFreeIndices[0 .. muNumFree)). operator[]
// resolves a handle back to its element, asserting (non-fatally) that the handle is
// in range AND that the slot is currently allocated (not on the free list).
//
// SLICE -- operator[] (the indexed accessor) plus Clear / GetObjectIndex / PushIndex,
// all reconstructed from the X360 out-of-line instantiation
//   CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, short>::*
//     operator[](s16)      @ 0x82663010  (asserts CgsIndexedPool.h:241 / :250)
//     Clear()              @ 0x826638D0
//     GetObjectIndex(T*)   @ 0x82662F50  (assert CgsIndexedPool.h:222 "Object not in pool")
//     PushIndex(s16)       @ 0x82663180  (asserts :301 "Object not in pool", :310 "Object is already free")
// (IDA-truncated symbol "BrnResource::GameDataEventSlot,short>"). The element base /
// free-list / count fields are modeled at the exact byte offsets the asm reads. The
// pool's construction, Allocate/Free proper, iteration and the rest of the class are
// DEFERRED to their own reconstruction pass -- GROW this header additively when they
// land; do NOT fork the pool elsewhere.
//
// FLAG (additive correction, X360-authoritative): the prior commit modeled +0x08 as an
// honest unnamed `muPad08` because operator[] never touches it. Clear @0x826638D0 and
// PushIndex @0x82663180 now PROVE +0x08 is a real s16 live/allocated count -- Clear sets
// it to 0, PushIndex decrements it when an index is returned to the free list. It is
// renamed msNumAllocated here. No prior TU referenced muPad08 by name (it was padding),
// so the rename is safe; the byte offset (+0x08) and the size (s16) are unchanged.
//
// FLAG: this is a layout-faithful model of the generic, not the full IndexedPool surface.
// The byte layout (+0 element base, +4 free-index array, +0x08 num-allocated, +0x0A
// num-free, +0x0C capacity) is proven by the X360 asm of these four methods; the +0x06
// short is not touched by any reconstructed method and stays honest unnamed padding.
// ============================================================================

namespace CgsContainers
{

template <typename T, s32 N>
class IndexedPool
{
public:
    // Resolve a handle to its element. X360 operator[](s16): range-guard the index
    // against the capacity ("Object not in pool"), then verify the slot is NOT on the
    // free list ("Object is free"), then return &maElements[luIndex]. Both guards are
    // non-fatal (the binary returns the element pointer regardless). The element stride
    // is sizeof(T) (== 0x30 for the GameDataEventSlot instantiation).
    T& operator[](s16 lsIndex)
    {
        CGS_ASSERT(lsIndex >= 0 && lsIndex < msCapacity, "Object not in pool\n");

        // Walk the free-index list; lsScanned counts the entries inspected before a
        // match (it stays one short of msNumFree iff lsIndex was found on the list).
        s16 lsScanned = 0;
        const s16 lsNumFree = msNumFree;
        if (lsNumFree > 0)
        {
            for (s16 lsFree = 0; lsFree < lsNumFree; ++lsFree)
            {
                if (mpFreeIndices[lsFree] == lsIndex)
                {
                    break;
                }
                lsScanned = static_cast<s16>(lsFree + 1);
            }
        }
        CGS_ASSERT(lsScanned == lsNumFree, "Object is free\n");

        return maElements[lsIndex];
    }

    const T& operator[](s16 lsIndex) const
    {
        return const_cast<IndexedPool*>(this)->operator[](lsIndex);
    }

    // Clear @ 0x826638D0: reset the free list to the identity permutation
    // (mpFreeIndices[i] = i for i in [0, msCapacity)), then mark every slot free --
    // msNumAllocated = 0, msNumFree = msCapacity. X360 loops over the capacity (`lhz
    // 0xC`) filling the s16 free-index array, then stores 0 at +0x08 and the capacity
    // at +0x0A. The fastcall "return result" is a register artifact; Clear is void.
    void Clear()
    {
        const s16 lsCapacity = msCapacity;
        for (s16 lsIndex = 0; lsIndex < lsCapacity; ++lsIndex)
        {
            mpFreeIndices[lsIndex] = static_cast<u16>(lsIndex);
        }
        msNumAllocated = 0;
        msNumFree      = msCapacity;
    }

    // GetObjectIndex @ 0x82662F50: recover a live object's handle from its address.
    // X360: index = (s16)((lpObject - maElements) / sizeof(T)); then range-guard
    // [0, msCapacity) ("Object not in pool", CgsIndexedPool.h:222, non-fatal) and
    // return the index. Pointer subtraction already scales by sizeof(T) (== the 48-byte
    // stride the asm divides by), so this is byte-faithful. The returned index is the
    // sign-extended 16-bit difference (X360 `extsh`).
    s16 GetObjectIndex(const T* lpObject) const
    {
        const s16 lsIndex = static_cast<s16>(lpObject - maElements);

        CGS_ASSERT(lsIndex >= 0 && lsIndex < msCapacity, "Object not in pool\n");

        return lsIndex;
    }

    // PushIndex @ 0x82663180: return a handle to the free list (the back half of
    // Free). X360: range-guard lsIndex < msCapacity ("Object not in pool",
    // CgsIndexedPool.h:301); scan the free list over msNumFree entries asserting the
    // index is NOT already present ("Object is already free", :310 -- lsScanned stays
    // one short of msNumFree iff a match was hit); append lsIndex at
    // mpFreeIndices[msNumFree]; decrement msNumAllocated; increment msNumFree. Both
    // asserts are non-fatal (the binary mutates the list regardless). The fastcall
    // "return result" is the receiver pointer (register artifact); modeled void.
    void PushIndex(s16 lsIndex)
    {
        CGS_ASSERT(lsIndex < msCapacity, "Object not in pool\n");

        s16       lsScanned = 0;
        const s16 lsNumFree = msNumFree;
        if (lsNumFree > 0)
        {
            for (s16 lsFree = 0; lsFree < lsNumFree; ++lsFree)
            {
                if (static_cast<s16>(mpFreeIndices[lsFree]) == lsIndex)
                {
                    break;
                }
                lsScanned = static_cast<s16>(lsFree + 1);
            }
        }
        CGS_ASSERT(lsScanned == lsNumFree, "Object is already free\n");

        mpFreeIndices[msNumFree] = static_cast<u16>(lsIndex);
        --msNumAllocated;
        ++msNumFree;
    }

private:
    // ---- layout (X360 byte offsets read by operator[]) ----
    T*   maElements;      // +0x00  live element array (the X360 `*a1` base)
    u16* mpFreeIndices;   // +0x04  free-slot index list (`*(a1+4)`, u16 entries)
    s16  msPad06;         // +0x06  (not touched by any reconstructed method; honest padding)
    s16  msNumAllocated;  // +0x08  live/allocated count (`lhz/sth 0x8`; Clear -> 0, PushIndex --)
    s16  msNumFree;       // +0x0A  number of free slots (`lhz 0xA`)
    s16  msCapacity;      // +0x0C  pool capacity (`lhz 0xC`)
};

} // namespace CgsContainers

#endif // GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
