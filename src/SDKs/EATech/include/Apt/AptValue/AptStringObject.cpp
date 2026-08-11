// ===========================================================================
// EATech Apt -- AptStringObject out-of-line bodies.   Reconstructed from the
// X360 ARTIST.XEX (the authoritative spine):
//     AptStringObject::AptStringObject     @ 0x82AF0D08
//     AptStringObject::~AptStringObject    @ 0x82AF0E20
//     AptStringObject::setString           @ 0x82AD6378
//     AptStringObject::objectMemberLookup  @ 0x82ADC878
//     AptStringObject::operator new        @ 0x82AE68A0
//     AptStringObject::operator delete     @ 0x82AF0D78
//
// The boxed "String" object: an AptObject (property hash + prototype) that also
// holds one ref-counted AptValue -- the wrapped string -- forwarding member
// lookups to it before falling back to the generic object lookup.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"

// ---------------------------------------------------------------------------
// GC-pool operator new / delete  (@0x82AE68A0 / @0x82AF0D78).
//
// The X360 reaches the GC value pool (gpGCPoolManager, an AptValueGC_PoolManager
// -- itself a DOGMA_PoolManager) and, inline, maintains the AptValueGC_MemItem
// "is allocated" flag the GC live-object walk reads:
//   new    : p = pool->Allocate(size);            then MemItem(p).SetIsAllocated(1)
//   delete : ok = pool->Deallocate(p, size);      then on success MemItem(p).SetIsAllocated(0)
// The free path is exactly AptValueGC_PoolManager::DeallocateAptValueGC, so it
// is restored as that call (de-inlined). gpGCPoolManager is wired by
// AptAllocatorInitialize (AptInit.cpp @0x82ADD118) at bring-up; the null guards
// keep a pre-boot allocation inert.
// ---------------------------------------------------------------------------
void* AptStringObject::operator new(size_t size)
{
    if (gpGCPoolManager == 0)
        return 0;

    void* lpMem = gpGCPoolManager->Allocate(size);
    if (lpMem != 0)
        static_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

void AptStringObject::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF0D08 -- a String object with property-hash capacity 8 and VFT
// index AptVFT_StringObject (0x21). The base AptObject ctor zero-inits the
// class-flags word (the byte-store + mask the X360 emits for mClassFlags(0));
// then the given value is boxed.
// ---------------------------------------------------------------------------
AptStringObject::AptStringObject(AptValue* pValue)
    : AptObject(AptVFT_StringObject, 8)
{
    // mpValue is set directly by setString (it adopts without releasing a prior
    // value), matching the X360 ctor -- no separate initialiser is emitted.
    setString(pValue);
}

// ---------------------------------------------------------------------------
// dtor @0x82AF0E20 -- release the boxed value and clear it. (The vtable reset to
// the AptStringObject vtable, and the chained ~AptObject -- which tears down the
// property hash -- are emitted automatically by the C++ destructor.)
// ---------------------------------------------------------------------------
AptStringObject::~AptStringObject()
{
    if (mpValue != 0)
        mpValue->Release();
    mpValue = 0;
}

// ---------------------------------------------------------------------------
// setString @0x82AD6378 -- AddRef the new value and adopt it. The X360 body does
// not Release a prior value (only ever called on a freshly constructed object).
// ---------------------------------------------------------------------------
void AptStringObject::setString(AptValue* pValue)
{
    pValue->AddRef();
    mpValue = pValue;
}

// ---------------------------------------------------------------------------
// objectMemberLookup @0x82ADC878 -- first ask the wrapped string value (its AS
// String methods), and only if that yields nothing fall back to the generic
// AptObject member lookup (own properties + the native "registerClass").
// ---------------------------------------------------------------------------
AptValue* AptStringObject::objectMemberLookup(AptValue* const pThis,
                                              const AptNativeString* const pName) const
{
    AptValue* lpResult = mpValue->objectMemberLookup(pThis, pName);
    if (lpResult == 0)
        lpResult = AptObject::objectMemberLookup(pThis, pName);
    return lpResult;
}
