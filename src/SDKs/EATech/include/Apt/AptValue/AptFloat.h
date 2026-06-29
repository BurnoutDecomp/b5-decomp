#pragma once

// ===========================================================================
// EATech Apt -- AptFloat: the float ActionScript value. SHAPE verbatim from the
// Feb-2007 leak (AptValue/AptFloat.h); non-GC, free-list recycled (mirror of
// AptInteger). Bodies in AptFloat.cpp (X360 AptFloat::Create @0x82AE7C08).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

class AptFloat : public AptValueNoGC
{
    friend class AptGC;   // AptGC::CleanAll calls the protected ClearPool (leak: friend)
public:
    static void* operator new(size_t size)              { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size)  { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)            { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)             { AptValueNoGC::VerifyAptValueNoGC(); AptNonGCFreeSavedSize(p); }

    float GetFloat() const { return mfValue; }

    static AptFloat* Create(const float fValue);

protected:
    static void ClearPool();

    virtual void DeleteThis()  { Destroy(); }
    virtual void ForceDelete() { Destroy(); }
    void Destroy();

    virtual ~AptFloat() {}

private:
    union
    {
        float     mfValue;
        AptFloat* mpNextFree;
    };

    AptFloat();
    explicit AptFloat(const float fValue)
        : AptValueNoGC(AptVFT_Float)
        , mfValue(fValue)
    {
    }

    static AptFloat* spFirstFree;
};
