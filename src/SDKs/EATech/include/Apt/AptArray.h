#pragma once

// ===========================================================================
// EATech Apt -- AptArray: the ActionScript Array object.
//
// AptArray : AptObject -- a scriptable object (so it has named members + a
// prototype) plus a dynamic, ref-counted vector of AptValue* elements. The AS
// array methods (push/pop/shift/unshift/slice/splice/concat/sort/sortOn/reverse/
// join) are native methods driven by the ActionScript interpreter; this header
// reconstructs the STORAGE core (the vector + element access + GC), the AS
// methods are a follow-on with the VM.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (8AptArray). LAYOUT: AptObject
// (32 bytes) + mpArray (AptValue**) + mnCapacity + mnLength = 44 bytes
// (the GC-pool block size the dtor frees).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptObject.h"

struct AptArray : public AptObject
{
    // GC-pool allocation @0x82AE6088 -- AptArray is a garbage-collected value
    // (AptValueGC base), so its block comes from the GC pool rather than the
    // non-GC leaves' gpNonGCPoolManager route. Bodies in AptArray.cpp so the
    // header need not pull in the pool-manager type. Reached as
    // AptArray::operator new(44) from AptValueFactory::CreateArray.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    AptValue** mpArray;     // [8]  element storage (ref-counted AptValue*s)
    int32_t    mnCapacity;  // [9]  allocated slots
    int32_t    mnLength;    // [10] logical length

    AptArray();                                  // @0x82D978
    AptArray(int nCount, AptValue** ppItems);    // @0x82D728
    virtual ~AptArray();                         // @0x82EA7C

    // GC virtual overrides (mark/teardown the elements + the object hash).
    virtual void RegisterReferences();   // @0x7E7CB0
    virtual void DestroyGCPointers();    // @0x7F8660

    int32_t   length() const { return mnLength; }            // @0x7DF380
    AptValue* GetAt(int32_t nIndex) const;                   // @0x7E094C
    AptValue* get(int32_t nIndex) const;                     // @0x7E14F4 (bounds-checked -> undefined)
    void      SetAt(int32_t nIndex, AptValue* pValue);       // @0x7E11B0 (AddRef/Release)
    void      set(int32_t nIndex, AptValue* pValue);         // @0x7F0328 (grow + set, extends length)

    // ---- ActionScript Array native methods (sMethod_*) + sort comparator --
    // bodies in AptArray.cpp. AptExtFunctionPtr natives: f(thisArray[, argCount]),
    // AS args off the global native-arg stack. (sort/sortOn/join are a follow-on.)
    static AptValue* sMethod_push(AptArray* pThis, int nArgCount);
    static AptValue* sMethod_pop(AptArray* pThis);
    static AptValue* sMethod_reverse(AptArray* pThis);
    static AptValue* sMethod_unshift(AptArray* pThis, int nArgCount);
    static AptValue* sMethod_slice(AptArray* pThis, int nArgCount);
    static AptValue* sMethod_splice(AptArray* pThis, int nArgCount);
    static AptValue* sMethod_concat(AptArray* pThis, int nArgCount);
    static int       defaultSortCompareFunc(AptValue* const* ppA, AptValue* const* ppB);

private:
    void _reserve(int32_t nCount);   // @0x7F01D0 (grow to pow2, min 8)
};

// FLAG (homed by the AS-globals TU): the shared "undefined" value get() returns
// for out-of-range indices. Null until the AS globals are built.
extern AptValue* gpUndefinedValue;
