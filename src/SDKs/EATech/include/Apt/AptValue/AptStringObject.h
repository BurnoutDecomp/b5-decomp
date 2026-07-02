#pragma once

// ===========================================================================
// EATech Apt -- AptStringObject: the boxed ActionScript "String" object.
//
// AptStringObject : AptObject -- the scriptable wrapper ECMAScript creates for
// `new String(x)` (vtbl object-type index 0x21 == AptVFT_StringObject). Unlike
// the AS *primitive* string value (AptString, AptValueNoGC, vtbl index 1), the
// String *object* is a full property-bearing AS object (so it derives from
// AptObject: a property hash + a prototype) and additionally holds a single
// boxed AptValue -- the wrapped string value -- which it ref-counts and to which
// it forwards member lookups (charAt / indexOf / length / ...). It is therefore
// garbage-collected (AptValueGC lineage) and allocated from the GC value pool.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (the authoritative spine):
//     AptStringObject::AptStringObject     @ 0x82AF0D08
//     AptStringObject::~AptStringObject    @ 0x82AF0E20
//     AptStringObject::setString           @ 0x82AD6378
//     AptStringObject::objectMemberLookup  @ 0x82ADC878
//     AptStringObject::operator new        @ 0x82AE68A0
//     AptStringObject::operator delete     @ 0x82AF0D78
// (The compiler-generated `vector deleting destructor' @ 0x82AF55A0 is a thunk
// and is dropped per project policy.)
//
// LAYOUT: AptObject (vtable + AptValue word + embedded AptNativeHash + the
// mClassFlags word == 36 bytes / 0x24 on PPC, pinned by the deleting dtor's
// operator delete(this, 36)) + mpValue at +0x20. Reconstructed with named
// members (x64 widths for the pointer; no byte-offset hacks).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptObject.h"            // AptObject base
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"        // GC pool Allocate / DeallocateAptValueGC + gAptValueGCSizeOffset / AptValueGC_MemItem

struct AptStringObject : public AptObject
{
    // GC-pool allocation. AptStringObject is a garbage-collected value, so its
    // block comes from the GC value pool (gpGCPoolManager) -- the GC analogue of
    // the non-GC leaves' gpNonGCPoolManager route. The X360 inlines the GC
    // allocate-and-mark / free-and-unmark bookkeeping into these operators (the
    // AptValueGC_MemItem "is allocated" flag); restored here by name. Bodies in
    // AptStringObject.cpp so the header need not depend on the pool internals.
    static void* operator new(size_t size);             // @0x82AE68A0
    static void  operator delete(void* p, size_t size); // @0x82AF0D78

    explicit AptStringObject(AptValue* pValue);         // @0x82AF0D08
    virtual ~AptStringObject();                         // @0x82AF0E20

    // Box (or rebox) the wrapped string value: AddRef the new value and adopt
    // it. (Non-virtual; called from the ctor. The X360 body does not Release a
    // prior value -- it is only ever invoked on a freshly constructed object.)
    void setString(AptValue* pValue);                   // @0x82AD6378

    // Resolve a member/method name: first ask the wrapped string value (its AS
    // String methods -- charAt / indexOf / split / ...), then fall back to the
    // generic AptObject member lookup (own properties + "registerClass").
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x82ADC878

    // The boxed string value (the X360 callers read +0x20 directly -- e.g.
    // AptCIH::_gotoAndX @0x82B0D384 `lwz r3, 0x20(r3)` on the label arm).
    AptValue* GetBoxedString() const { return mpValue; }

private:
    AptValue* mpValue;   // +0x20 -- the boxed (ref-counted) string value
};
