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
// MINIMAL SLICE -- only operator[] (the indexed accessor) is reconstructed here, from
// the X360 out-of-line instantiation
//   CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, N>::operator[](s16)
//     @ 0x82663010   (IDA-truncated symbol "BrnResource::GameDataEventSlot,")
// whose two baked asserts name this header (CgsIndexedPool.h:241 "Object not in pool",
// :250 "Object is free"). The element base / free-list / count fields are modeled at
// the exact byte offsets the asm reads. The pool's construction, Allocate/Free,
// iteration and the rest of the class are DEFERRED to their own reconstruction pass --
// GROW this header additively when they land; do NOT fork the pool elsewhere.
//
// FLAG: this is a layout-faithful minimal model of the generic, not the full
// IndexedPool surface. The byte layout (+0 element base, +4 free-index array, +0xA
// num-free, +0xC capacity) is proven by the X360 asm of operator[]; the +0x06/+0x08
// shorts are not touched by that accessor and are honest unnamed padding here.
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

private:
    // ---- layout (X360 byte offsets read by operator[]) ----
    T*   maElements;     // +0x00  live element array (the X360 `*a1` base)
    u16* mpFreeIndices;  // +0x04  free-slot index list (`*(a1+4)`, u16 entries)
    u16  muPad08;        // +0x08  (not touched by operator[]; honest padding)
    s16  msNumFree;      // +0x0A  number of free slots (`lhz 0xA`)
    s16  msCapacity;     // +0x0C  pool capacity (`lhz 0xC`)
};

} // namespace CgsContainers

#endif // GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
