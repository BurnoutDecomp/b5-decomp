// ===========================================================================
// EATech Apt -- AptPrototype bodies.   Reconstructed from the X360 ARTIST.XEX.
//   ctor                @ 0x82AEFEF0   |  ~AptPrototype          @ 0x82AEFFA0
//   operator new        @ 0x82AE5FE8   |  SetSuperConstructor    @ 0x82AD6418
//   objectMemberLookup  @ 0x82AD63C8   |  objectMemberSet        @ 0x82ADC8C8
//   RegisterReferences  @ 0x82AE25F0   |  DestroyGCPointers      @ 0x82AF0E80
//
// The class-prototype AS object: an AptValueWithHash carrying one extra
// reference, mpSuperConstructor (the "__constructor__" member -- the function the
// AS class machinery chains to). It is a garbage-collected value, so it allocates
// from the GC value pool and participates in the GC mark/teardown.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue (AddRef/Release, sReferenceRegistrationCb)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // AptNativeString::c_str
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"        // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"           // AptValueGC_MemItem (alloc bookkeeping)

#include <cstring>   // strcmp

// ---------------------------------------------------------------------------
// operator new @ 0x82AE5FE8 / operator delete
//
// X360: `v = gpGCPoolManager->Allocate(size); v->SetIsAllocated(gAptValueGCSizeOffset,
// 1); return v;` -- the standard GC-value allocation (gpGCPoolManager is the
// off_8324D834 AptValueGC_PoolManager; its Allocate is the inherited
// DOGMA_PoolManager::Allocate, and byte_8324D804 is gAptValueGCSizeOffset). The
// delete is the mirror through the GC pool (same shape as the AptArray /
// AptNativeFunction GC siblings). Guarded for null until the Apt runtime startup
// (AptInit) wires the pool (FLAG: gpGCPoolManager is null until then).
// ---------------------------------------------------------------------------
void* AptPrototype::operator new(size_t size)
{
    if (gpGCPoolManager == 0)
        return 0;
    void* pMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(pMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return pMem;
}

void AptPrototype::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @ 0x82AEFEF0
//
// AptValueWithHash(AptVFT_Prototype, 8), then the super-constructor starts null
// and delayed deletion is disabled. (The X360 sets the AptPrototype vtable
// between the base ctor and the SetAllowDelayedDeletion call -- that vtable store
// is compiler-emitted as part of entering the most-derived ctor body and is not
// hand-written.)
// ---------------------------------------------------------------------------
AptPrototype::AptPrototype()
    : AptValueWithHash(AptVFT_Prototype, KI_HASH_CAPACITY)
{
    mpSuperConstructor = 0;
    SetAllowDelayedDeletion(false);
}

// ---------------------------------------------------------------------------
// ~AptPrototype @ 0x82AEFFA0
//
// Empty: the embedded AptNativeHash (in the AptValueWithHash base) tears itself
// down (the X360 body inlines that member destructor as the conditional
// `if (!mHash.IsEmpty()) mHash.DestroyGCPointers()`), and the vtable swaps the
// X360 shows are the compiler's most-derived-then-base vtable stores. The
// super-constructor reference is released on the GC path (DestroyGCPointers), not
// here -- matching the asm, which never touches +0x1C in the destructor.
// ---------------------------------------------------------------------------
AptPrototype::~AptPrototype()
{
}

// ---------------------------------------------------------------------------
// SetSuperConstructor @ 0x82AD6418
//
// Store the new super-constructor, AddRef it, then Release the previous one
// (AddRef-before-Release so a self-assign is safe). AddRef = AptValue vtbl[0],
// Release = AptValue vtbl[1] -- the standard ref-counted-pointer setter.
// ---------------------------------------------------------------------------
void AptPrototype::SetSuperConstructor(AptValue* pSuperConstructor)
{
    AptValue* pOld = mpSuperConstructor;
    mpSuperConstructor = pSuperConstructor;
    if (pSuperConstructor)
        pSuperConstructor->AddRef();
    if (pOld)
        pOld->Release();
}

// ---------------------------------------------------------------------------
// objectMemberLookup @ 0x82AD63C8
//
// The prototype's only native member is "__constructor__": return the super-
// constructor for that name, else null (all other members resolve through the
// property hash by the caller). The X360 inlines the name compare as a byte loop;
// it is a plain strcmp against "__constructor__".
// ---------------------------------------------------------------------------
AptValue* AptPrototype::objectMemberLookup(AptValue* const /*pThis*/,
                                           const AptNativeString* const pName) const
{
    if (strcmp(pName->c_str(), "__constructor__") == 0)
        return mpSuperConstructor;
    return 0;
}

// ---------------------------------------------------------------------------
// objectMemberSet @ 0x82ADC8C8
//
// Setting "__constructor__" routes to SetSuperConstructor and reports handled
// (true); any other name is not a native member of the prototype (false -- the
// caller then stores it in the property hash). Same inlined "__constructor__"
// compare as objectMemberLookup.
// ---------------------------------------------------------------------------
bool AptPrototype::objectMemberSet(AptValue* const /*pThis*/,
                                   const AptNativeString* const pName,
                                   AptValue* const pValue)
{
    if (strcmp(pName->c_str(), "__constructor__") != 0)
        return false;
    SetSuperConstructor(pValue);
    return true;
}

// ---------------------------------------------------------------------------
// RegisterReferences @ 0x82AE25F0
//
// Mark the property hash (owned by this), then register the super-constructor
// reference with the GC collector (one held AptValue ref: cb(owner, &slot,
// debugName, 0)). The debug name is "SuperConstructor".
//
// The X360 calls the registration callback (dword_8324E4E8 == sReferenceRegistrationCb)
// unconditionally once mpSuperConstructor is non-null -- RegisterReferences only
// runs while the GC is up, so the callback is always installed there. The extra
// `&& sReferenceRegistrationCb` guard mirrors the committed AptArray GC sibling
// and is a behavioural no-op when the collector is active (it only keeps the PC
// build, where the callback is null until AptInit, from dereferencing null).
// ---------------------------------------------------------------------------
void AptPrototype::RegisterReferences()
{
    mHash.RegisterReferences(this);
    if (mpSuperConstructor && AptValue::sReferenceRegistrationCb)
        AptValue::sReferenceRegistrationCb(this, &mpSuperConstructor, "SuperConstructor", 0);
}

// ---------------------------------------------------------------------------
// DestroyGCPointers @ 0x82AF0E80
//
// Release the super-constructor (AptValue vtbl[1] = Release) and clear the slot,
// then tear down the property hash. (Overrides AptValueWithHash::DestroyGCPointers,
// which does only the hash teardown.)
// ---------------------------------------------------------------------------
void AptPrototype::DestroyGCPointers()
{
    if (mpSuperConstructor)
        mpSuperConstructor->Release();
    mpSuperConstructor = 0;
    mHash.DestroyGCPointers();
}
