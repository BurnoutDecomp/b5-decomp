#pragma once

// ===========================================================================
// EATech Apt -- AptNone: the ActionScript "undefined" / none value.
//
// SHAPE verbatim from the Feb-2007 leak (AptValue/AptNone.h) + the X360 ARTIST
// constructor (AptNone::AptNone @0x82AE62C8). Like AptBoolean it is NOT recycled:
// the runtime brings up a single pinned singleton (constructed once from
// AptValueInitialize), so it carries a pinned MAX_REFCOUNT and -- uniquely among
// the value types -- it is marked NOT-defined (setIsDefined(0)), which is what
// makes it the AS "undefined" value.
//
// GC vs non-GC: the X360 ledger attests only AptNone::AptNone and AptNone's
// compiler-emitted vector-deleting-destructor for this class -- it owns NO
// RegisterReferences / IsGarbageCollected body. A direct AptValueGC leaf would be
// forced to define RegisterReferences (it stays pure-virtual in AptValueGC), so
// its absence proves AptNone derives from AptValueNoGC (which satisfies both GC
// virtuals inline). The ctor's pinned-refcount singleton shape matches the other
// non-GC singleton, AptBoolean.
//
// The deleting destructor (@0x82AE6318) is a compiler thunk (scalar + vector
// `delete this` codegen, restoring the base vtable then routing the block to the
// pool); it is regenerated from the virtual ~AptNone() + the pooled
// operator delete / delete[] below, so it is not hand-written here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

class AptNone : public AptValueNoGC
{
public:
    static void* operator new(size_t size)              { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size)  { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)            { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)             { AptValueNoGC::VerifyAptValueNoGC(); AptNonGCFreeSavedSize(p); }

protected:
    // @ 0x82AE62C8 -- the X360 ctor: base AptValueNoGC(AptVFT_None), then mark the
    // value undefined and pin its reference count so the shared "undefined"
    // singleton is never freed. Body in AptNone.cpp.
    AptNone();

    virtual ~AptNone() {}
};
