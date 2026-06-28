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
    virtual bool GetHasClass() const;                    // @0x7DF374
    virtual void SetHasClass(int bHasClass);             // @0x7DF34C
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x7FC7E8
};
