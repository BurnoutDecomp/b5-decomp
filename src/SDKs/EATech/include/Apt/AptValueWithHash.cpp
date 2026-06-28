// ===========================================================================
// EATech Apt -- AptValueWithHash GC virtuals (the vtable anchor).
// DECOMPILED from the PS3 EXTERNAL ELF: RegisterReferences @0x7E7C9C delegates
// the GC mark to the embedded hash (owned by this); DestroyGCPointers @0x7F8650
// tears the hash down.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"

void AptValueWithHash::RegisterReferences()
{
    mHash.RegisterReferences(this);
}

void AptValueWithHash::DestroyGCPointers()
{
    mHash.DestroyGCPointers();
}
