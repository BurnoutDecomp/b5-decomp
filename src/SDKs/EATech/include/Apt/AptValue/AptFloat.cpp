// ===========================================================================
// EATech Apt -- AptFloat out-of-line bodies (X360 AptFloat::Create @0x82AE7C08 +
// the leak shape; mirror of AptInteger -- see AptInteger.cpp for the shared
// free-list spin-lock leaf + the base-ctor deferred-release note).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include <intrin.h>   // _InterlockedExchange (the free-list spin lock)
#include <new>        // placement new

AptFloat* AptFloat::spFirstFree = 0;

// The free-list spin lock (the X360 lwarx/stwcx. idiom around spFirstFree).
// FLAG PC-platform leaf: spin lock modelled as a host-portable interlocked TAS
// (threading primitive, not an engine method; uncontended on the single-thread
// bring-up path).
namespace
{
    volatile long gAptFloatFreeListLock = 0;
    // FLAG PC-platform leaf: threading primitive (host interlocked TAS).
    inline void AptFloatFreeListLock_Acquire()
    {
        while (_InterlockedExchange(&gAptFloatFreeListLock, 1) != 0) {}
    }
    // FLAG PC-platform leaf: threading primitive (host interlocked TAS).
    inline void AptFloatFreeListLock_Release()
    {
        _InterlockedExchange(&gAptFloatFreeListLock, 0);
    }
}

// @ 0x82AE7C08 -- pop a recycled value, else pool-allocate, then construct with fValue.
// (The inline release-at-end re-mark on the reuse path is the base ctor's GC-thread
// arm -- see AptInteger.cpp; the placement-new ctor reproduces it.)
AptFloat* AptFloat::Create(const float fValue)
{
    AptFloatFreeListLock_Acquire();
    AptFloat* lpValue = spFirstFree;
    if (lpValue != 0)
    {
        spFirstFree = lpValue->mpNextFree;
        AptFloatFreeListLock_Release();
        return ::new (lpValue) AptFloat(fValue);
    }
    AptFloatFreeListLock_Release();

    void* lpMem = (gpNonGCPoolManager != 0) ? gpNonGCPoolManager->Allocate(sizeof(AptFloat)) : 0;
    if (lpMem == 0)
        return 0;
    return ::new (lpMem) AptFloat(fValue);
}

void AptFloat::Destroy()
{
    AptFloatFreeListLock_Acquire();
    mpNextFree  = spFirstFree;
    spFirstFree = this;
    AptFloatFreeListLock_Release();
}

void AptFloat::ClearPool()
{
    while (spFirstFree != 0)
    {
        AptFloat* lpNext = spFirstFree->mpNextFree;
        if (gpNonGCPoolManager != 0)
            gpNonGCPoolManager->Deallocate(spFirstFree, sizeof(AptFloat));
        spFirstFree = lpNext;
    }
}
