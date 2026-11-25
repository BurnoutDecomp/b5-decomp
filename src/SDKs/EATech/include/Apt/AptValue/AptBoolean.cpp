// ===========================================================================
// EATech Apt -- AptBoolean out-of-line bodies (X360 AptBoolean::Create @0x82AD5390
// + the reference shape). Two shared singletons; Create returns the matching one.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include <new>   // placement new

AptBoolean* AptBoolean::spBooleanFalse = 0;
AptBoolean* AptBoolean::spBooleanTrue  = 0;

bool AptBoolean::GetBool() const
{
    return mbValue;
}

// Create the two shared singletons -- called by AptValueInitialize (AptInit.cpp
// @0x82B02800) after AptAllocatorInitialize wires gpNonGCPoolManager; the
// null-pool guard keeps a pre-boot call inert (the singletons come up with the
// runtime).
void AptBoolean::Initialize()
{
    if (gpNonGCPoolManager == 0)
        return;
    if (spBooleanFalse == 0)
    {
        void* lpMem = gpNonGCPoolManager->Allocate(sizeof(AptBoolean));
        if (lpMem != 0)
            spBooleanFalse = ::new (lpMem) AptBoolean(false);
    }
    if (spBooleanTrue == 0)
    {
        void* lpMem = gpNonGCPoolManager->Allocate(sizeof(AptBoolean));
        if (lpMem != 0)
            spBooleanTrue = ::new (lpMem) AptBoolean(true);
    }
}

void AptBoolean::Shutdown()
{
    if (spBooleanTrue != 0)
    {
        spBooleanTrue->~AptBoolean();
        gpNonGCPoolManager->Deallocate(spBooleanTrue, sizeof(AptBoolean));
        spBooleanTrue = 0;
    }
    if (spBooleanFalse != 0)
    {
        spBooleanFalse->~AptBoolean();
        gpNonGCPoolManager->Deallocate(spBooleanFalse, sizeof(AptBoolean));
        spBooleanFalse = 0;
    }
}
