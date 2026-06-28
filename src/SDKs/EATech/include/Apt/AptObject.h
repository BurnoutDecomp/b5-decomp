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
};
