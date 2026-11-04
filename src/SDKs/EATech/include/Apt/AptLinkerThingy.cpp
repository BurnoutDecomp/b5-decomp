#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"

#include <new>

// ARTIST @0x82ADBF58. The member retains a copy, then the constructor's
// by-value parameter releases its own reference.
AptLinkerThingy::AptLinkerThingy(AptFilePtr file, AptValue* pValue)
    : mnRefCount(0), mpFile(file), mpValue(pValue), mbLinked(false)
{
}

AptLinkerThingy* MakeLinkerThingy(const AptFilePtr& file, AptValue* pValue)
{
    void* pMem = gpAptSharedPtrPool->Allocate(sizeof(AptLinkerThingy));
    if (pMem == 0)
        return 0;
    return ::new (pMem) AptLinkerThingy(file, pValue);
}

// ARTIST @0x82AE5128. The member's destructor clears the held file before
// releasing it. The low flag bit controls whether the pool block is freed.
void* AptLinkerThingy::ScalarDeletingDestructor(char flags)
{
    this->~AptLinkerThingy();
    if (flags & 1)
        gpAptSharedPtrPool->Deallocate(this, sizeof(AptLinkerThingy));
    return this;
}
