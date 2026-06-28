// ===========================================================================
// EATech Apt -- AptInteger out-of-line bodies. Reconstructed from the X360 ARTIST
// (AptInteger::Create @0x82AE7D48, AptInteger::Destroy @0x82AD77C0) + the leak shape.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include <new>   // placement new

AptInteger* AptInteger::spFirstFree = 0;

// @ 0x82AE7D48 -- recycle a freed AptInteger from the free-list, else pool-allocate one,
// then (re)construct in place with nValue.
//
// FLAG (faithful note): the X360 guards spFirstFree with an lwarx/stwcx atomic spinlock
// (multithreaded) and, when reusing a freed value, re-marks it release-at-end and re-registers
// it in the per-frame deferred-release vector (the Apt value frame garbage collector). The PC
// boot path is single-threaded and that frame-GC vector is part of the not-yet-reconstructed
// Apt runtime startup (AptInit), so this reconstructs the core, observable Create semantics --
// pop the free-list, else allocate from the non-GC pool, then placement-construct.
AptInteger* AptInteger::Create(const int nValue)
{
    AptInteger* lpValue = spFirstFree;
    if (lpValue != 0)
    {
        spFirstFree = lpValue->mpNextFree;     // pop the recycled value
        return ::new (lpValue) AptInteger(nValue);
    }

    // Free-list empty -> allocate from the non-GC pool (X360: DOGMA_PoolManager::Allocate of
    // sizeof(AptInteger); 12 on PPC). Guarded for null until AptInit wires the pool.
    void* lpMem = (gpNonGCPoolManager != 0) ? gpNonGCPoolManager->Allocate(sizeof(AptInteger)) : 0;
    if (lpMem == 0)
        return 0;
    return ::new (lpMem) AptInteger(nValue);
}

// @ 0x82AD77C0 -- recycle: push onto the free-list (not freed; ClearPool does the real release).
// FLAG: the X360 lwarx/stwcx atomic spinlock around spFirstFree is omitted (single-threaded boot).
void AptInteger::Destroy()
{
    mpNextFree  = spFirstFree;
    spFirstFree = this;
}

// Release every recycled value back to the pool (called at Apt shutdown / pool clear).
void AptInteger::ClearPool()
{
    while (spFirstFree != 0)
    {
        AptInteger* lpNext = spFirstFree->mpNextFree;
        if (gpNonGCPoolManager != 0)
            gpNonGCPoolManager->Deallocate(spFirstFree, sizeof(AptInteger));
        spFirstFree = lpNext;
    }
}
