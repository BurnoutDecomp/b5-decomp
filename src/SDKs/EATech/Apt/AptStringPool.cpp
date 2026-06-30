// ===========================================================================
// EATech Apt -- StringPool out-of-line bodies: the interned `__proto__` key
// (saConstant) + the temporary-string-pool teardown (ClearTemporaryPool).
//
// X360 ClearTemporaryPool @0x82AD8E20 (PS3 EXTERNAL _ZN10StringPool18ClearTemporary
// PoolEv @0x7E60FC). saConstant == X360 dword_8324E580 / PS3 _ZN10StringPool10sa
// ConstantE -- the interned "__proto__" property key the AS member-op fast path
// compares names against (AptNativeHash::Set/Lookup, hash 27581).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptString/StringPool.h"   // StringPool (saConstant + ClearTemporaryPool)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString (the pooled node) + gpNonGCPoolManager

#include <intrin.h>   // _InterlockedExchange (the Apt string-pool spin lock)

// ---------------------------------------------------------------------------
// saConstant -- the interned "__proto__" key.
//
// The console seeds it from a precompiled StaticStringHelperT in StringPool::
// Initialize (PS3 0x7F7A70: `operator=(StaticStringHelperT&)` over sStringPoolData);
// the only value the engine ever compares against (case-insensitively) is the
// "__proto__" key, so it is reconstructed here as that literal. EAStringC's
// const-char* ctor (InitFromBuffer) builds the same interned content.
// ---------------------------------------------------------------------------
const EAStringC StringPool::saConstant("__proto__");

// ---------------------------------------------------------------------------
// The AptString recycle free-list head (X360 off_8324E4FC / PS3 StringPool::
// spFirstFree). Its single DEFINITION lives in AptGlobals.cpp (the globals home);
// declared extern here (this TU's ClearTemporaryPool empties it, and the built EATech
// AptString.cpp's Create/Destroy pop/push it). Defining it here too was a LNK2005 dup.
// ---------------------------------------------------------------------------
extern AptString* gpStringPoolFreeList;   // off_8324E4FC (defined in AptGlobals.cpp)

// FLAG: the string-pool free-list spin lock (X360 unk_8324E8E8 / PS3
// AptMutexStringPoolFirstFree). The console brackets the free-list teardown with
// the lwarx/stwcx. interrupt-masked test-and-set idiom; modelled as a host-portable
// interlocked TAS (uncontended on the single-thread bring-up path).
namespace
{
    volatile long gStringPoolFreeListLock = 0;
    inline void StringPoolFreeListLock_Acquire()
    {
        while (_InterlockedExchange(&gStringPoolFreeListLock, 1) != 0) {}
    }
    inline void StringPoolFreeListLock_Release()
    {
        _InterlockedExchange(&gStringPoolFreeListLock, 0);
    }
}

// ---------------------------------------------------------------------------
// ClearTemporaryPool @0x82AD8E20 -- release every pooled (recycled) temporary
// string node back to the heap, emptying the free list. The GC teardown's final
// step (AptGC::CleanUnreachable / CleanAll / AptCommonShutdown).
//
// X360 walk (under the pool lock): for each node off the free-list head, save its
// mpNext (+0xC), call the node's vtable +0x28 (an empty STUB -- no-op), then call
// the head's scalar-deleting-destructor (vtable +0x38, arg 1 == `delete this`),
// advance head = saved mpNext, repeat until the list is empty. The STUB call is a
// no-op; `delete pNode` IS the scalar-deleting-destructor (~AptString() then the
// AptString operator delete -> gpNonGCPoolManager->Deallocate).
// ---------------------------------------------------------------------------
void StringPool::ClearTemporaryPool()
{
    StringPoolFreeListLock_Acquire();

    AptString* pNode = gpStringPoolFreeList;
    if (pNode)
    {
        do
        {
            AptString* const pNext = pNode->GetNext();   // v8 = result[3] (mpNext @ +0xC)
            // X360 vtable +0x28 == STUB (no-op); omitted.
            delete pNode;                                // X360 vtable +0x38(,1): scalar deleting dtor
            gpStringPoolFreeList = pNext;                // head = saved mpNext
            pNode = pNext;
        }
        while (pNode);
    }

    StringPoolFreeListLock_Release();
}
