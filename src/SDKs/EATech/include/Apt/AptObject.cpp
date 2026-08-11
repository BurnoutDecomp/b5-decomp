// ===========================================================================
// EATech Apt -- AptObject.   DECOMPILED from the PS3 EXTERNAL ELF.
//   GetHasClass @0x7DF374 / SetHasClass @0x7DF34C / objectMemberLookup @0x7FC7E8.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptArray.h"              // implemented-objects AptArray view
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"         // AllocateAptValueGC / DeallocateAptValueGC
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // the registerClass registry hash
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"  // the native-arg stack (registerClass params)
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"   // the shared true/false pair (registerClass result)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager (registry allocation)

#include <cstring>   // strcmp
#include <new>       // placement new (Create)

// The shared "undefined" singleton (X360 off_8324D814), defined in AptGlobals.cpp
// and built by AptInit's AptValueInitialize @0x82B02800. DoesImplementObject
// reads it as the past-the-end fallback when its loop walks beyond the implemented
// array's length -- it will not match a real target, so the probe simply fails.
extern AptValue* gpUndefinedValue;

// GC-pool operator new/delete -- AptObject is a garbage-collected value
// (AptValueGC base), so its block comes from the GC pool. AllocateAptValueGC =
// DOGMA Allocate + AptValueGC_MemItem::SetIsAllocated (the X360 inlines that pair
// into every GC type's operator new). The null guard covers the pre-init window
// before AptInit's AptAllocatorInitialize @0x82ADD118 wires the pool.
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
    // x64: bit 8 (mask 0x100; B4 mbHasClass) -- the old bit-23 form was the X360
    // big-endian bit-numbering reversal.
    return (mClassFlags & 0x100u) != 0;
}

void AptObject::SetHasClass(int bHasClass)
{
    // Clear the hasClass bit, then set it iff bHasClass != 0 (the console's
    // rotate+mask idiom expressed on the whole word).
    mClassFlags = (mClassFlags & ~0x100u) | (bHasClass ? 0x100u : 0u);   // x64 bit 8
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

// ~AptObject @0x82AF00D0 -- the GC teardown for a property-bearing object: if the
// hash has allocated a bucket table (mHash.mpTable != null, i.e. !IsEmpty()),
// release every GC reference it holds. The two raw vtable writes in the asm
// (off_821459D8 then off_82145594) are the during-/post-destruction vtable resets
// the compiler emits automatically for a virtual dtor; they need no source line.
AptObject::~AptObject()
{
    if (mHash.mpTable != nullptr)   // r3+4 = mHash.mpTable; !IsEmpty()
        mHash.DestroyGCPointers();
}

// SetImplementedObjects @0x82AF5868 -- record the implements-set: store the
// AptArray of implemented objects under the (verbatim, lowercase-'s') key the
// X360 build uses, then stash the count in the low byte of mClassFlags.
void AptObject::SetImplementedObjects(AptValue* pImplementedArray, char nCount)
{
    // EAStringC key("__INTERFACEs__") = InitFromBuffer; ~EAStringC =
    // DecreaseInternalRefCount (the ctor/dtor pair the asm inlines around Set).
    EAStringC key("__INTERFACEs__");
    mHash.Set(key, pImplementedArray);
    // stb r29, 0x1C(r31): the count overwrites the low byte of mClassFlags.
    mClassFlags = (mClassFlags & 0xFFFFFF00u) | (uint32_t)(uint8_t)nCount;
}

// GetImplementedObjects @0x82AE68F0 -- return the implemented count (low byte of
// mClassFlags) via *pnCountOut, and the "__INTERFACES__" AptArray as the result;
// null/0 when this object implements nothing.
AptValue* AptObject::GetImplementedObjects(int* pnCountOut) const
{
    const uint8_t nCount = (uint8_t)mClassFlags;   // lbz r11, 0x1C(r31)
    *pnCountOut = nCount;
    if (nCount == 0)
        return 0;

    EAStringC key("__INTERFACES__");
    return mHash.Lookup(key);
}

// DoesImplementObject @0x82AE6960 -- is pTarget implemented by this object? First
// walk the __proto__ chain (each link's GetNativeHashVirtual()->mp__Proto__),
// returning true on a match and stopping when a link no longer carries a hash;
// then, if this object has any implemented interfaces, scan the "__INTERFACES__"
// array for pTarget.
bool AptObject::DoesImplementObject(AptValue* pTarget) const
{
    // ---- __proto__ chain walk (r31 = i, seeded from mHash.mp__Proto__) -------
    for (AptValue* i = mHash.mp__Proto__; i != nullptr; )
    {
        if (i == pTarget)
            return true;                         // loc_82AE6A54
        if (!i->ContainsNativeHashVirtual())     // vtbl+0xC; break when no hash
            break;
        i = i->GetNativeHashVirtual()->mp__Proto__;   // vtbl+8 -> +8
    }

    // ---- implemented-interfaces scan ----------------------------------------
    const uint8_t nCount = (uint8_t)mClassFlags;   // lbz 0x1C
    if (nCount == 0)
        return false;

    EAStringC key("__INTERFACES__");
    AptArray* pInterfaces = static_cast<AptArray*>(mHash.Lookup(key));

    // The X360 re-reads the count after the Lookup; if it went to zero, bail.
    const uint8_t nCount2 = (uint8_t)mClassFlags;
    if (nCount2 == 0)
        return false;   // LABEL_14: DecreaseInternalRefCount + return 0

    for (uint32_t nIndex = 0; nIndex < nCount2; ++nIndex)
    {
        // *(v5+40) = mnLength, *(v5+32) = mpArray; past the length, the asm
        // substitutes the shared "undefined" sentinel (which never matches).
        AptValue* pEntry = (nIndex < (uint32_t)pInterfaces->mnLength)
                               ? pInterfaces->mpArray[nIndex]
                               : gpUndefinedValue;
        if (pEntry == pTarget)
            return true;   // loc_82AE6A4C
    }
    return false;
}

// Set__Proto__ @0x82ADC150 / SetPrototype @0x82ADC158 -- AptObject's out-of-line
// tail-call thunks into the matching property-hash setters.
void AptObject::Set__Proto__(AptValue* pValue)
{
    mHash.Set__Proto__(pValue);
}

void AptObject::SetPrototype(AptValue* pValue)
{
    mHash.SetPrototype(pValue);
}

// ===========================================================================
// The Object.registerClass NATIVE (X360 sub_82AF6A38; gpObjRegistrationFunc wraps
// it -- see objectMemberLookup above). registerClass(exportName, class):
//   * exactly 2 params; param 0 (stack top-1) must be a DEFINED string value/object
//     (type 1 / 33) -- the linkage/export NAME; param 1 (top-2) is the CLASS.
//   * the class registry hash (X360 dword_8324E2D4) is lazily pool-allocated
//     (20 bytes == an AptNativeHash(8) on the console) on first use.
//   * a type-3 (`null`) class UNSETS the name; anything else SETS name -> class
//     (the engine's clip-placement path looks the placed char's export name up
//     here to instantiate its AS class).
//   * returns the shared boolean true on success, false otherwise
//     (off_8324E418 / off_8324E414 == the AptBoolean singleton pair).
// ===========================================================================

// X360 dword_8324E2D4 -- the export-name -> AS-class registry Object.registerClass
// fills and the clip-placement class binding consumes. Lazily built below.
AptNativeHash* gpAptClassRegistry = nullptr;

extern AptActionInterpreter gAptActionInterpreter;   // the native-arg stack

AptValue* AptObject::RegisterClassNative(AptValue* /*pContext*/, int nNumParams)
{
    if (nNumParams == 2)
    {
        AptValue* const pName = gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop - 1];
        if (pName != nullptr && pName->getIsDefined())
        {
            const int nNameType = static_cast<int>(pName->getVtblIndex());
            if (nNameType == AptVFT_StringValue || nNameType == AptVFT_StringObject)
            {
                AptValue* const pClass =
                    gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop - 2];

                if (gpAptClassRegistry == nullptr)
                {
                    // Console: DOGMA Allocate(20) + {cap=8, table=null...} == AptNativeHash(8).
                    void* pMem = gpNonGCPoolManager
                        ? gpNonGCPoolManager->Allocate(sizeof(AptNativeHash)) : nullptr;
                    gpAptClassRegistry = pMem ? ::new (pMem) AptNativeHash(8) : nullptr;
                }

                if (gpAptClassRegistry != nullptr)
                {
                    EAStringC nameText;
                    pName->toString(&nameText);
                    if (pClass != nullptr && pClass->getVtblIndex() == AptVFT_None)
                        gpAptClassRegistry->Unset(nameText);
                    else
                        gpAptClassRegistry->Set(nameText, pClass);
                }
                return AptBoolean::Create(true);    // off_8324E418
            }
        }
    }
    return AptBoolean::Create(false);               // off_8324E414
}
