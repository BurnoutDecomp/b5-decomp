// ===========================================================================
// EATech Apt -- AptBoolean out-of-line bodies (X360 AptBoolean::Create @0x82AD5390
// + the leak shape). Two shared singletons; Create returns the matching one.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include <new>   // placement new

AptBoolean* AptBoolean::spBooleanFalse = 0;
AptBoolean* AptBoolean::spBooleanTrue  = 0;

bool AptBoolean::GetBool() const
{
    return mbValue;
}

// @ 0x82AD5390 -- `return a1 == 1 ? off_8324E418 : off_8324E414` -- the shared
// true / false singletons (off_8324E418 = true, off_8324E414 = false).
AptBoolean* AptBoolean::Create(const bool bValue)
{
    return bValue ? spBooleanTrue : spBooleanFalse;
}

// Create the two shared singletons. The X360 calls this from the Apt startup.
// FLAG: AptInit (the runtime startup that wires gpNonGCPoolManager) is not yet
// reconstructed, so this is guarded for a null pool; the singletons come up when
// the runtime starts.
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
