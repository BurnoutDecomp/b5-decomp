// ===========================================================================
// EATech Apt -- AptObject.   DECOMPILED from the PS3 EXTERNAL ELF.
//   GetHasClass @0x7DF374 / SetHasClass @0x7DF34C / objectMemberLookup @0x7FC7E8.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"         // AllocateAptValueGC / DeallocateAptValueGC

#include <cstring>   // strcmp
#include <new>       // placement new (Create)

// GC-pool operator new/delete -- AptObject is a garbage-collected value
// (AptValueGC base), so its block comes from the GC pool. AllocateAptValueGC =
// DOGMA Allocate + AptValueGC_MemItem::SetIsAllocated (the X360 inlines that pair
// into every GC type's operator new). Guarded for null until AptInit wires the
// pool (FLAG: gpGCPoolManager is null until then).
void* AptObject::operator new(size_t size)
{
    return (gpGCPoolManager != nullptr) ? gpGCPoolManager->AllocateAptValueGC(size) : nullptr;
}

void AptObject::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != nullptr)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// The plain AS "new Object()" -- allocate a generic AptObject (value type
// AptVFT_Object) from the GC pool and construct it with the requested property-hash
// capacity, or null on pool exhaustion (the new-with-null-guard codegen the X360
// inlines at the AS object-creation sites).
AptObject* AptObject::Create(int nHashCapacity)
{
    void* pMem = AptObject::operator new(sizeof(AptObject));
    if (pMem == nullptr)
        return nullptr;
    return ::new (pMem) AptObject(AptVFT_Object, nHashCapacity);
}

bool AptObject::GetHasClass() const
{
    return ((mClassFlags >> 23) & 1) != 0;
}

void AptObject::SetHasClass(int bHasClass)
{
    // Clear bit 23, then set it iff bHasClass != 0 (the console's rotate+mask
    // idiom expressed on the whole word).
    mClassFlags = (mClassFlags & ~0x00800000u) | (bHasClass ? 0x00800000u : 0u);
}

AptValue* AptObject::objectMemberLookup(AptValue* const /*pThis*/,
                                        const AptNativeString* const pName) const
{
    // The only native member the base object exposes is "registerClass"; all
    // other members are resolved through the property hash by the caller.
    if (strcmp(pName->c_str(), "registerClass") == 0)
        return gpObjRegistrationFunc;
    return 0;
}
