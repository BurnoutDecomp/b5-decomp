#pragma once

// ===========================================================================
// EATech Apt -- AptInteger: the integer ActionScript value.
// SHAPE verbatim from the Feb-2007 leak (AptValue/AptInteger.h). A non-GC value
// recycled through a free-list (spFirstFree): Create pops a freed one (or pool-
// allocates), Destroy pushes back, ClearPool releases them for real. Bodies in
// AptInteger.cpp (X360 AptInteger::Create @0x82AE7D48 / Destroy @0x82AD77C0).
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

class AptInteger : public AptValueNoGC
{
public:
    static void* operator new(size_t size)              { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size)  { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)            { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)             { AptValueNoGC::VerifyAptValueNoGC(); AptNonGCFreeSavedSize(p); }

    int GetInt() const { return mnValue; }

    static AptInteger* Create(const int nValue);

protected:
    static void ClearPool();

    virtual void DeleteThis()  { Destroy(); }
    virtual void ForceDelete() { Destroy(); }
    void Destroy();

    virtual ~AptInteger() {}

private:
    union
    {
        int         mnValue;
        AptInteger* mpNextFree;   // free-list link while recycled
    };

    AptInteger();
    explicit AptInteger(const int nValue)
        : AptValueNoGC(AptVFT_Integer)
        , mnValue(nValue)
    {
    }

    static AptInteger* spFirstFree;
};
