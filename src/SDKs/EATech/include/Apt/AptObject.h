#pragma once

// ===========================================================================
// EATech Apt -- AptObject: the generic ActionScript object.
//
// AptObject : AptValueWithHash -- a property-bearing value plus a class-flags
// word (the "has class" bit + the implemented-objects bookkeeping). It is the
// base of AptArray and the other scriptable AS object types.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (9AptObject): GetHasClass @0x7DF374 /
// SetHasClass @0x7DF34C / objectMemberLookup @0x7FC7E8 / Set/Get__Proto__/
// Prototype thunks (delegate to the hash). LAYOUT: AptValueWithHash (28 bytes) +
// mClassFlags (dword [7], the hasClass bit @ bit23).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"

// FLAG (homed by the AS-globals TU): the native "registerClass" method value
// returned by objectMemberLookup. Null until the AS globals are built.
extern AptValue* gpObjRegistrationFunc;

struct AptObject : public AptValueWithHash
{
protected:
    uint32_t mClassFlags;   // [7] -- hasClass @ bit 23 (+ implemented-objects bits)

    AptObject(AptVirtualFunctionTable_Indices eType, int nHashCapacity)
        : AptValueWithHash(eType, nHashCapacity), mClassFlags(0)
    {
    }

public:
    // GC teardown @0x82AF00D0 -- release the property hash's GC pointers (the
    // compiler folds the inherited ~AptValueWithHash member-dtor here, plus the
    // during-/post-destruction vtable resets). Declared so the scalar deleting
    // destructor (and every derived `vector deleting destructor`) can chain it.
    virtual ~AptObject();
    // GC-pool allocation -- AptObject is a garbage-collected value (AptValueGC
    // base), so its block comes from the GC pool (gpGCPoolManager), exactly like
    // AptArray. Bodies in AptObject.cpp so the header need not pull in the
    // pool-manager type. Attesting site: AptScriptColour::sMethod_getTransform
    // @0x82AF5918 reaches AptObject::operator new(32) building a fresh AS Object.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // The plain AS "new Object()" -- a generic property-bearing AptObject of value
    // type AptVFT_Object. The X360 inlines exactly this (operator new(32) +
    // AptValueWithHash(AptVFT_Object, 8) + the AptObject vtable) inside the AS
    // objects that hand back a fresh Object; factored here as the canonical home.
    // Returns null if the GC pool is exhausted. Defined in AptObject.cpp.
    static AptObject* Create(int nHashCapacity = 8);

    virtual bool GetHasClass() const;                    // @0x7DF374
    virtual void SetHasClass(int bHasClass);             // @0x7DF34C
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x7FC7E8

    // ---- the implemented-interfaces set (AS `implements`) -------------------
    // mClassFlags' low byte is the count of objects this one implements; the set
    // itself lives in the property hash under the "__INTERFACES__" key as an
    // AptArray of the implemented prototype/object values.

    // SetImplementedObjects @0x82AF5868 -- store pImplementedArray under the
    // "__INTERFACES__" key and record nCount in the low byte of mClassFlags.
    void SetImplementedObjects(AptValue* pImplementedArray, char nCount);  // @0x82AF5868
    // GetImplementedObjects @0x82AE68F0 -- hand back the implemented count (via
    // *pnCountOut) and the "__INTERFACES__" AptArray (return), or null/0 when none.
    AptValue* GetImplementedObjects(int* pnCountOut) const;                // @0x82AE68F0
    // DoesImplementObject @0x82AE6960 -- true if pTarget is reachable through this
    // object's __proto__ chain OR is one of its implemented-interface entries.
    bool DoesImplementObject(AptValue* pTarget) const;                     // @0x82AE6960

    // ---- prototype thunks (delegate to the property hash) -------------------
    // AptObject homes its own out-of-line Set__Proto__ @0x82ADC150 / SetPrototype
    // @0x82ADC158 (each a tail-call into the matching AptNativeHash setter).
    void Set__Proto__(AptValue* pValue);   // @0x82ADC150
    void SetPrototype(AptValue* pValue);   // @0x82ADC158
};
