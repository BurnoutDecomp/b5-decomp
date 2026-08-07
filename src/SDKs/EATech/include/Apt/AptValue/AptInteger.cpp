// ===========================================================================
// EATech Apt -- AptInteger out-of-line bodies. Reconstructed from the X360 ARTIST
// (AptInteger::Create @0x82AE7D48, AptInteger::Destroy @0x82AD77C0) + the leak shape.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include <intrin.h>   // _InterlockedExchange (the free-list spin lock)
#include <new>        // placement new

AptInteger* AptInteger::spFirstFree = 0;

// The free-list spin lock -- the X360 brackets every spFirstFree mutation with the
// lwarx/stwcx. interrupt-masked test-and-set idiom.
// FLAG PC-platform leaf: spin lock modelled as a host-portable interlocked TAS
// (threading primitive, not an engine method; uncontended on the single-thread
// bring-up path -- the AptString.cpp / AptStringPool.cpp treatment).
namespace
{
    volatile long gAptIntegerFreeListLock = 0;
    // FLAG PC-platform leaf: threading primitive (host interlocked TAS).
    inline void AptIntegerFreeListLock_Acquire()
    {
        while (_InterlockedExchange(&gAptIntegerFreeListLock, 1) != 0) {}
    }
    // FLAG PC-platform leaf: threading primitive (host interlocked TAS).
    inline void AptIntegerFreeListLock_Release()
    {
        _InterlockedExchange(&gAptIntegerFreeListLock, 0);
    }
}

// @ 0x82AE7D48 -- recycle a freed AptInteger from the free-list, else pool-allocate one,
// then (re)construct in place with nValue. The release-at-end re-mark + deferred-vector
// re-registration the X360 shows inline on the reuse path is the AptValue base ctor's
// GC-thread arm (sub_82AE3000, inlined) -- reproduced by the placement-new ctor, whose
// defer queue is gated on the off_8324E51C vector (see AptValue.cpp).
AptInteger* AptInteger::Create(const int nValue)
{
    AptIntegerFreeListLock_Acquire();
    AptInteger* lpValue = spFirstFree;
    if (lpValue != 0)
    {
        spFirstFree = lpValue->mpNextFree;     // pop the recycled value
        AptIntegerFreeListLock_Release();
        return ::new (lpValue) AptInteger(nValue);
    }
    AptIntegerFreeListLock_Release();

    // Free-list empty -> allocate from the non-GC pool (X360: DOGMA_PoolManager::Allocate
    // of sizeof(AptInteger); 12 on PPC). gpNonGCPoolManager is wired by
    // AptAllocatorInitialize (AptInit.cpp); the null guard keeps a pre-boot call inert.
    void* lpMem = (gpNonGCPoolManager != 0) ? gpNonGCPoolManager->Allocate(sizeof(AptInteger)) : 0;
    if (lpMem == 0)
        return 0;
    return ::new (lpMem) AptInteger(nValue);
}

// @ 0x82AD77C0 -- recycle: push onto the free-list (not freed; ClearPool does the real
// release), under the free-list lock.
void AptInteger::Destroy()
{
    AptIntegerFreeListLock_Acquire();
    mpNextFree  = spFirstFree;
    spFirstFree = this;
    AptIntegerFreeListLock_Release();
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
