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
// Prototype thunks (delegate to the hash). The native AptValueWithHash base
// occupies 28 bytes, followed by the class word at +0x1C.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"

// The native "registerClass" method value returned by objectMemberLookup (X360
// off_8324D748). Defined in AptGlobals.cpp; built + GC-rooted by
// AptValueInitialize (AptInit.cpp) -- null only before that bring-up runs.
extern AptValue* gpObjRegistrationFunc;

struct AptArray;

struct AptObject : public AptValueWithHash
{
protected:
    // ARTIST 0x82AF0048 and 0x82AF5868: the implemented count is the
    // upper byte and the class / instance flags are bits 8 and 9 in PPC order.
    struct ClassFlags
    {
        uint32_t mnImplementedCount : 8;
        uint32_t mbHasClass : 1;
        uint32_t mbIsInMainInst : 1;
    };
    ClassFlags mClassFlags;

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT(sizeof(ClassFlags) == 4, "ARTIST AptObject class state word");
        CGS_STATIC_ASSERT(offsetof(AptObject, mClassFlags) == 0x1C, "ARTIST AptObject class state offset");
        CGS_STATIC_ASSERT(sizeof(AptObject) == 0x20, "ARTIST AptObject allocation size");
    }

public:
    // ARTIST folds this step into its derived constructors. Their common
    // AptValueWithHash construction remains a separate call to 0x82AEFE98.
    __forceinline AptObject(AptVirtualFunctionTable_Indices eType, int nHashCapacity)
        : AptValueWithHash(eType, nHashCapacity)
    {
        mClassFlags.mnImplementedCount = 0;
        mClassFlags.mbHasClass = false;
        mClassFlags.mbIsInMainInst = false;
    }

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

    virtual bool GetHasClass() const;                    // @0x82AF0068
    virtual void SetHasClass(int bHasClass);             // @0x82AF0048
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x7FC7E8

    bool GetIsInMainInst() const { return mClassFlags.mbIsInMainInst != 0; }
    void SetIsInMainInst(bool bInMainInst)
    {
        mClassFlags.mbIsInMainInst = bInMainInst;
    }

    // ---- the implemented-interfaces set (AS `implements`) -------------------
    // The first byte of mClassFlags is the count of objects this one implements; the set
    // itself lives in the property hash under the "__INTERFACES__" key as an
    // AptArray of the implemented prototype/object values.

    // SetImplementedObjects @0x82AF5868 -- store pImplementedArray under the
    // "__INTERFACEs__" key and record nCount in the first byte of mClassFlags.
    void SetImplementedObjects(AptArray* pImplementedArray, int nCount);  // @0x82AF5868
    // GetImplementedObjects @0x82AE68F0 -- hand back the implemented count (via
    // *pnCountOut) and the "__INTERFACES__" AptArray (return), or null/0 when none.
    AptArray* GetImplementedObjects(int* pnCountOut) const;                // @0x82AE68F0
    // DoesImplementObject @0x82AE6960 -- true if pTarget is reachable through this
    // object's __proto__ chain OR is one of its implemented-interface entries.
    bool DoesImplementObject(AptValue* pTarget) const;                     // @0x82AE6960

    // ---- prototype thunks (delegate to the property hash) -------------------
    // AptObject homes its own out-of-line Set__Proto__ @0x82ADC150 / SetPrototype
    // @0x82ADC158 (each a tail-call into the matching AptNativeHash setter).
    void Set__Proto__(AptValue* pValue);   // @0x82ADC150
    void SetPrototype(AptValue* pValue);   // @0x82ADC158

    // The Object.registerClass NATIVE @0x82AF6A38 (gpObjRegistrationFunc wraps it).
    // registerClass(exportName, class) fills the export-name -> AS-class registry
    // (gpAptClassRegistry) the clip-placement class binding consumes. A static member
    // so it keeps the plain C-ABI the AptExtFunctionPtr pin needs (AptInit seats its
    // address); body in AptObject.cpp.
    static AptValue* RegisterClassNative(AptValue* pContext, int nNumParams);  // @0x82AF6A38
};
