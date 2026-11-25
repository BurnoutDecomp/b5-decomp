// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptExtObject out-of-line bodies.
//
// The base for native "extension" objects the apt VM exposes to ActionScript: a
// garbage-collected AptValue (value type AptVFT_Extension == 29) that OWNS a
// pool-allocated AptNativeHash of native members (held by pointer at mpNativeHash)
// and forwards ActionScript variable get/set onto a bound apt context value via the
// shared interpreter (gAptActionInterpreter).
//
// DECOMPILED from the X360 ARTIST.XEX (the authoritative spine):
//     AptExtObject::AptExtObject         @ 0x82AE8338
//     AptExtObject::~AptExtObject        @ 0x82AF2F10
//     AptExtObject::operator new         @ 0x82AE82D8
//     AptExtObject::operator delete      @ 0x824ED540
//     AptExtObject::CreateNewAptObject   @ 0x82AF07E0
//     AptExtObject::CreateNewAptFunction @ 0x82AF08D0
//     AptExtObject::SetFunction          @ 0x82AF7288
//     AptExtObject::GetParam             @ 0x82ADC270
//     AptExtObject::GetVariable          @ 0x82B06C80
//     AptExtObject::SetVariable          @ 0x82B06CE0
//     AptExtObject::objectMemberSet      @ 0x82AF9720
//     AptExtObject::RegisterReferences   @ 0x82ADCFD8
//     AptExtObject::DestroyGCPointers    @ 0x82AEDDB0
//
// LAYOUT (x64-native): AptValueGC base (vtable + 32-bit bitfield, 8 bytes) +
//   mpNativeHash  [c:+0x08]  the owned native-member hash (separately pool-allocated)
//   mnObjectSize  [c:+0x0C]  the object's allocated size (stamped by operator new)
// The console's a1[2] == mpNativeHash (+8) and *(v+12) == mnObjectSize (+0xC).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// XDK size optimization matches ARTIST at 0x82AE82D8 and 0x82B06C80.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptExtObject.h"
#include "SDKs/EATech/include/Apt/AptGlobal.h"

#include "SDKs/EATech/include/Apt/AptNativeHash.h"          // the owned member hash
#include "SDKs/EATech/include/Apt/AptObject.h"              // AptObject::Create (CreateNewAptObject)
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"      // AptNativeFunction (CreateNewAptFunction)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"   // getVariable / setVariable
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC key (SetFunction)
#include "SDKs/EATech/include/Apt/AptDefine.h"              // gpNonGCPoolManager (hash pool)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager::Allocate/Deallocate
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"          // gpGCPoolManager, AptValueGC_MemItem,
                                                            // gAptValueGCSizeOffset, DeallocateAptValueGC
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"            // AptValueGC_MemItem::SetIsAllocated

#include <new>   // placement new (the pool-allocated AptNativeHash)

// ---------------------------------------------------------------------------
// The shared Apt action interpreter instance (X360 &dword_8324E760). It is the
// `this` the X360 GetVariable/SetVariable pass to getVariable/setVariable, and the
// object whose mOperandStack.mnTop (dword_8324E760, +0) / mOperandStack.mppItems (off_8324E768, +8) GetParam
// reads. Defined in AptGlobals.cpp (&dword_8324E760); declared extern here.
// ---------------------------------------------------------------------------
extern AptActionInterpreter gAptActionInterpreter;          // &dword_8324E760

// ---------------------------------------------------------------------------
// operator new @0x82AE82D8 -- allocate the extension block from the garbage-
// collected value pool (off_8324D834 == gpGCPoolManager), mark its AptValueGC_MemItem
// header allocated (byte_8324D804 == gAptValueGCSizeOffset selects the size word),
// then stamp the requested size into the object's mnObjectSize (+0xC). No null guard
// in the asm: only reached after the Apt runtime has wired the GC pool.
//
// The cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block (the
// GC walk's "allocated" flag), not a member poke into a live C++ object -- the same
// external-allocator pattern as the rest of the GC value family (AptNativeFunction /
// AptArray operator new).
// ---------------------------------------------------------------------------
void* AptExtObject::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    // [c:+0x0C] stamp the allocated size into mnObjectSize on the raw block (the X360
    // `stw r30, 0xC(r31)` -- written before the C++ object is constructed).
    reinterpret_cast<AptExtObject*>(lpMem)->mnObjectSize = static_cast<uint32_t>(size);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @0x824ED540 -- return the block to the GC pool via the named
// AptValueGC_PoolManager helper (DeallocateAptValueGC = DOGMA free + clear the
// AptValueGC_MemItem allocated flag). off_8324D834 == gpGCPoolManager.
// ---------------------------------------------------------------------------
void AptExtObject::operator delete(void* p, size_t size)
{
    gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AE8338
//
// Construct the extension's hash in the Apt non-GC pool.
AptExtObject::AptExtObject(const int32_t iNumMembers)
    : AptValueGC(AptVFT_Extension)
{
    mpNativeHash = new AptNativeHash(iNumMembers);

    // X360 @0x82AE839C: AptValue::SetAllowDelayedDeletion(this, 0).
    SetAllowDelayedDeletion(false);
}

// ARTIST 0x82AF2F10. Release the hash through its pool-owning destructor.
AptExtObject::~AptExtObject()
{
    delete mpNativeHash;
}

// ---------------------------------------------------------------------------
// ARTIST 0x82AF07E0. Construct the native AS object in the GC pool with
// its default eight-bucket property hash.
AptObject* AptExtObject::CreateNewAptObject()
{
    return new AptObject(AptVFT_Object, 8);
}

// ---------------------------------------------------------------------------
// CreateNewAptFunction @0x82AF08D0 -- a fresh AS native-function value wrapping
// pAptExtFnc. X360: AptNativeFunction::operator new(36); if non-null, construct it
// (AptNativeFunction(pAptExtFnc)); else null.
// ---------------------------------------------------------------------------
AptNativeFunction* AptExtObject::CreateNewAptFunction(AptExtFunctionPtr pAptExtFnc)
{
    void* lpMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));   // [c:0x24 == 36]
    if (lpMem)
        return ::new (lpMem) AptNativeFunction(pAptExtFnc);
    return 0;
}

// ---------------------------------------------------------------------------
// SetFunction @0x82AF7288 -- add a native function member named pName to this
// extension's member hash. X360: build a scoped EAStringC key around pName
// (InitFromBuffer ... DecreaseInternalRefCount), Set it on the hash with pFunction.
// The temporary EAStringC's ctor/dtor are the X360 InitFromBuffer / Decrease pair.
// ---------------------------------------------------------------------------
void AptExtObject::SetFunction(const char* pName, AptValue* pFunction)
{
    EAStringC lKey(pName);   // ctor InitFromBuffer; dtor DecreaseInternalRefCount
    mpNativeHash->Set(lKey, pFunction);
}

// ---------------------------------------------------------------------------
// GetParam @0x82ADC270 -- the apt native-method argument fetch: the iIndex-th
// ActionScript call argument off the interpreter operand stack. The console reads
// mOperandStack.mppItems[mOperandStack.mnTop - iIndex - 1] (off_8324E768[dword_8324E760 - iIndex - 1]); the
// value is returned only when it exists AND is defined (the `(bitfield >> 27) & 1`
// test == big-endian bit 4 == mbIsDefined == getIsDefined()), else null.
// ---------------------------------------------------------------------------
AptValue* AptExtObject::GetParam(int iIndex)
{
    AptValue* pResult = gAptActionInterpreter.mOperandStack.mppItems[gAptActionInterpreter.mOperandStack.mnTop - iIndex - 1];
    if (!pResult || !pResult->getIsDefined())
        return 0;
    return pResult;
}

// ---------------------------------------------------------------------------
// GetVariable @0x82B06C80 -- resolve pVariable against the bound apt context value
// through the shared interpreter (getVariable(pContext, 0, pVariable, 1, 1, 0)).
// Returned only when defined (the same `>>27 & 1` mbIsDefined test as GetParam).
// ---------------------------------------------------------------------------
AptValue* AptExtObject::GetVariable(AptValue* pContext, const AptNativeString* const pVariable)
{
    AptValue* pResult = gAptActionInterpreter.getVariable(pContext, 0, pVariable, 1, 1, 0);
    if (!pResult || !pResult->getIsDefined())
        return 0;
    return pResult;
}

// ---------------------------------------------------------------------------
// SetVariable @0x82B06CE0 -- store pValue under pName onto the bound apt context
// value through the shared interpreter (setVariable(pContext, 0, pName, pValue, 1, 1,
// 0)). The X360 tail-calls setVariable; its int result (1 if stored) is the bool.
// ---------------------------------------------------------------------------
bool AptExtObject::SetVariable(AptValue* pContext, const AptNativeString* const pName,
                               AptValue* const pValue)
{
    return gAptActionInterpreter.setVariable(pContext, 0, pName, pValue, 1, 1, 0);
}

// ---------------------------------------------------------------------------
// objectMemberSet @0x82AF9720 -- set the named member only when it is not already
// present (look it up first; Set only on a miss). Always returns true. The bound apt
// value (pThis, r4) is ignored -- the extension's own member hash is the target.
// ---------------------------------------------------------------------------
bool AptExtObject::objectMemberSet(AptValue* const /*pThis*/, const AptNativeString* const pName,
                                   AptValue* const pValue)
{
    AptNativeHash* pHash = mpNativeHash;
    if (!pHash->Lookup(*pName))
        pHash->Set(*pName, pValue);
    return true;
}

// ---------------------------------------------------------------------------
// RegisterReferences @0x82ADCFD8 -- the GC mark walk: forward to the member hash,
// owned by this extension (mpNativeHash->RegisterReferences(this)).
// ---------------------------------------------------------------------------
void AptExtObject::RegisterReferences()
{
    mpNativeHash->RegisterReferences(this);
}

// ---------------------------------------------------------------------------
// DestroyGCPointers @0x82AEDDB0 -- release every held member reference + free the
// hash's bucket array, by forwarding to the member hash.
// ---------------------------------------------------------------------------
void AptExtObject::DestroyGCPointers()
{
    mpNativeHash->DestroyGCPointers();
}

// ARTIST 0x82AF7330 and 0x82AEDF28. The extension registry uses the
// same lock as AptGlobal's native member lookup.
void AptRegisterExtension(AptExtObject* pExtObject)
{
    AptNativeString name(pExtObject->GetName());
    pExtObject->Initialize();

    AptLockNativeGlobals();
    gpAptNativeGlobals->Set(name, pExtObject);
    AptUnlockNativeGlobals();
}

void AptUnRegisterExtension(AptExtObject* pExtObject)
{
    AptNativeString name(pExtObject->GetName());
    pExtObject->AddRef();

    AptLockNativeGlobals();
    gpAptNativeGlobals->AptValueWithHash::GetNativeHashVirtual()->Unset(name);
    AptUnlockNativeGlobals();

    pExtObject->Release();
}
