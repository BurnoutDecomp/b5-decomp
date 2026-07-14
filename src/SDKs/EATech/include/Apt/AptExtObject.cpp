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

#include "SDKs/EATech/include/Apt/AptExtObject.h"

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
// object whose mnStackTop (dword_8324E760, +0) / mpStack (off_8324E768, +8) GetParam
// reads. FLAG: global owned by the Apt boot TU; declared extern here.
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
// X360 sequence: AptValueGC base ctor (sub_82AE3000(this, 29) -- value type
// AptVFT_Extension); install the AptExtObject vtable (off_82145838); allocate a
// 20-byte AptNativeHash from the non-GC pool (off_8324D808 == gpNonGCPoolManager) and
// placement-construct it with iNumMembers (null on pool exhaustion); store it at
// mpNativeHash (+8); then SetAllowDelayedDeletion(false).
//
// The vtable install is the compiler's automatic codegen here; the explicit work is
// the hash allocation + the delayed-deletion flag.
// ---------------------------------------------------------------------------
AptExtObject::AptExtObject(const int32_t iNumMembers)
    : AptValueGC(AptVFT_Extension)
{
    // Allocate the native-member hash from the non-GC Apt pool (size 20 / [c:0x14])
    // and placement-construct it, or leave it null if the pool is exhausted.
    void* lpHashMem = gpNonGCPoolManager->Allocate(sizeof(AptNativeHash));   // [c:0x14]
    mpNativeHash = lpHashMem ? ::new (lpHashMem) AptNativeHash(iNumMembers) : nullptr;

    // X360 @0x82AE839C: AptValue::SetAllowDelayedDeletion(this, 0).
    SetAllowDelayedDeletion(false);
}

// ---------------------------------------------------------------------------
// dtor @0x82AF2F10
//
// X360: read mpNativeHash (a1[2]); install the AptExtObject teardown vtable
// (off_82145838); if the hash is non-null, run its scalar-deleting destructor
// (delete mpNativeHash -- the hash is non-GC-pool-allocated, so that is the explicit
// ~AptNativeHash + Deallocate); then install the base vtable (off_82145594). The two
// vtable stores are the compiler's destructor codegen (this-class vtable on entry,
// base vtable on the way down) -- automatic here. The explicit work is deleting the
// owned hash. ~AptNativeHash itself runs DestroyGCPointers when a table was allocated
// (matching the committed AptCIH::ForceCleanNativeHash teardown order).
// ---------------------------------------------------------------------------
AptExtObject::~AptExtObject()
{
    if (mpNativeHash)   // a1[2] != 0
    {
        mpNativeHash->DestroyGCPointers();
        mpNativeHash->~AptNativeHash();
        gpNonGCPoolManager->Deallocate(mpNativeHash, sizeof(AptNativeHash));   // [c:0x14]
        mpNativeHash = nullptr;
    }
}

// ---------------------------------------------------------------------------
// CreateNewAptObject @0x82AF07E0 -- a fresh generic AS "new Object()" (value type
// AptVFT_Object), allocated from the GC pool with a property hash. The X360 inlines
// AptObject::operator new(32) + AptValueWithHash(AptVFT_Object, 8) + the AptObject
// vtable + the hasClass-bit clear; that is exactly AptObject::Create (its committed
// canonical home). Null on pool exhaustion.
// ---------------------------------------------------------------------------
AptObject* AptExtObject::CreateNewAptObject()
{
    return AptObject::Create();   // default capacity 8 (the X360 li r4, 0x13 / li r5, 8)
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
    return nullptr;
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
// mpStack[mnStackTop - iIndex - 1] (off_8324E768[dword_8324E760 - iIndex - 1]); the
// value is returned only when it exists AND is defined (the `(bitfield >> 27) & 1`
// test == big-endian bit 4 == mbIsDefined == getIsDefined()), else null.
// ---------------------------------------------------------------------------
AptValue* AptExtObject::GetParam(int iIndex)
{
    AptValue* pResult = gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop - iIndex - 1];
    if (!pResult || !pResult->getIsDefined())
        return nullptr;
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
        return nullptr;
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
    return gAptActionInterpreter.setVariable(pContext, 0, pName, pValue, 1, 1, 0) != 0;
}

// ---------------------------------------------------------------------------
// objectMemberSet @0x82AF9720 -- set the named member only when it is not already
// present (look it up first; Set only on a miss). Always returns true. The bound apt
// value (pThis, r4) is ignored -- the extension's own member hash is the target.
// ---------------------------------------------------------------------------
bool AptExtObject::objectMemberSet(AptValue* const /*pThis*/, const AptNativeString* const pName,
                                   AptValue* const pValue)
{
    if (!mpNativeHash->Lookup(*pName))
        mpNativeHash->Set(*pName, pValue);
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

// ---------------------------------------------------------------------------
// AptRegisterExtension @0x82AF7330 -- register a native extension object with the
// AS VM. X360 sequence (verified against the export pseudocode):
//   1. v2 = ext->vtbl[+0x3C](ext)      == GetName();  EAStringC name(v2)
//   2. ext->vtbl[+0x40](ext)           == Initialize()   (the extension installs its
//      native function members here -- e.g. CgsGui::AptCommunicator::Initialize
//      @0x8285F0D8 SetFunction's its 6 sMethod_* natives)
//   3. spinlock unk_8324E71C acquire (lwarx/stwcx.)
//   4. AptNativeHash::Set(off_8324E37C + 8, name, ext) -- store the extension into
//      the _global extension object's embedded native hash, keyed by its name; every
//      AS name lookup then reaches it through AptValue::findChild's global-extension
//      arm (LookupInObject(gpAptGlobalExtensionObject, name)).
//   5. spinlock release; name refcount drop (the EAStringC dtor here).
// The host boot path is single-threaded, so the interlocked latch collapses to the
// direct Set (the same one-time-guard equivalence AptValueInitialize documents).
// ---------------------------------------------------------------------------
extern AptValue* gpAptGlobalExtensionObject;   // off_8324E37C (built by AptValueInitialize)

void AptRegisterExtension(AptExtObject* pExtObject)
{
    EAStringC lName(pExtObject->GetName());   // step 1 (InitFromBuffer / dtor Decrease)
    pExtObject->Initialize();                 // step 2 (vtbl +0x40)

    AptNativeHash* pGlobalHash = gpAptGlobalExtensionObject
        ? gpAptGlobalExtensionObject->GetNativeHashVirtual()   // (off_8324E37C + 8)
        : nullptr;
    if (pGlobalHash)
        pGlobalHash->Set(lName, pExtObject);
}

// ---------------------------------------------------------------------------
// AptUnRegisterExtension @0x82AEDF28 -- remove a native extension from the AS
// VM. The X360 body obtains the virtual name, takes a temporary reference, unsets
// that name from the global-extension hash, then releases the temporary reference.
// The AddRef is load-bearing: AptNativeHash::Unset releases the registry's own
// reference, so it keeps the extension alive until the unregister operation has
// finished. The console's uncontended registry spin lock is omitted for the same
// single-threaded host reason as AptRegisterExtension above.
// ---------------------------------------------------------------------------
void AptUnRegisterExtension(AptExtObject* pExtObject)
{
    EAStringC lName(pExtObject->GetName());
    pExtObject->AddRef();

    gpAptGlobalExtensionObject->GetNativeHashVirtual()->Unset(lName);

    pExtObject->Release();
}
