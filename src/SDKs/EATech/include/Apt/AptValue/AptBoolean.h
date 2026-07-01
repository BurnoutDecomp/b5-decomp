#pragma once

// ===========================================================================
// EATech Apt -- AptBoolean: the boolean ActionScript value. SHAPE verbatim from
// the Feb-2007 leak (AptValue/AptBoolean.h). Unlike AptInteger/AptFloat it is NOT
// recycled: there are two shared singletons (spBooleanTrue / spBooleanFalse) with
// a pinned MAX_REFCOUNT, so Create just returns the matching one and AddRef/Release
// are no-ops. Bodies in AptBoolean.cpp (X360 AptBoolean::Create @0x82AD5390).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

int AptValueInitialize();   // AptInit.cpp @0x82B02800 -- the designated singleton bootstrap

class AptBoolean : public AptValueNoGC
{
    // The console's value-singleton bootstrap is the one external caller of the
    // protected Initialize (it builds the pinned true/false pair at Apt bring-up).
    friend int ::AptValueInitialize();

public:
    static void* operator new(size_t size)              { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size)  { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)            { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)             { AptValueNoGC::VerifyAptValueNoGC(); AptNonGCFreeSavedSize(p); }

    bool GetBool() const;
    static AptBoolean* Create(const bool bValue);

protected:
    AptBoolean()
        : AptValueNoGC(AptVFT_Boolean)
    {
        mbValue = false;
        setIsDefined(1);
        setRefCount(MAX_REFCOUNT);   // pinned: the singletons are never freed
    }

    static void Initialize();
    static void Shutdown();

    virtual void AddRef()  {}
    virtual void Release() {}

    virtual ~AptBoolean() {}

private:
    bool mbValue;

    static AptBoolean* spBooleanFalse;
    static AptBoolean* spBooleanTrue;

    AptBoolean(bool bVal)
        : AptValueNoGC(AptVFT_Boolean)
    {
        mbValue = bVal;
        setIsDefined(1);
        setRefCount(MAX_REFCOUNT);
    }
};
