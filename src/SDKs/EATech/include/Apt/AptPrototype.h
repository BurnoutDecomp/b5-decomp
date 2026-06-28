#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptPrototype.
//
// The ActionScript class-prototype object: a property-bearing AS value that
// also carries a single "super constructor" reference -- the function the AS
// `extends` / class machinery chains to. It is the node the prototype chain is
// built from (AptCIH::AssociateInstToClass, the interpreter's _createObject /
// _FunctionAptActionExtends / _FunctionAptActionImplementsOp construct one), and
// the magic member name it exposes is "__constructor__".
//
// BASE: the ctor forwards directly to AptValueWithHash::AptValueWithHash(eType=
// AptVFT_Prototype (20), nHashCapacity=8) -- NOT to AptObject::AptObject -- so
// AptPrototype derives from AptValueWithHash (a property-bearing AptValueGC),
// adding one trailing member. (It is therefore a sibling of AptNativeFunction in
// shape: AptValueWithHash base + a single +0x1C field, not an AptObject -- it has
// no mClassFlags word; its +0x1C is the super-constructor pointer.)
//
// SHAPE: no Feb-2007 / DecFIGS header is in scope for this class, so the LAYOUT
// is recovered from the X360 ARTIST.XEX:
//     AptPrototype::AptPrototype                  @ 0x82AEFEF0
//     AptPrototype::~AptPrototype                 @ 0x82AEFFA0
//     AptPrototype::operator new                  @ 0x82AE5FE8
//     AptPrototype::`vector deleting destructor'  @ 0x82AEFFF8  (compiler thunk -- dropped)
//     AptPrototype::DestroyGCPointers             @ 0x82AF0E80
//     AptPrototype::RegisterReferences            @ 0x82AE25F0
//     AptPrototype::SetSuperConstructor           @ 0x82AD6418
//     AptPrototype::objectMemberLookup            @ 0x82AD63C8
//     AptPrototype::objectMemberSet               @ 0x82ADC8C8
//
// LAYOUT (sizeof = 32 / 0x20, pinned by `operator delete(this, 32)` in the
// deleting-destructor thunk + the `li r4, 0x20` there):
//   AptValueWithHash base ............ 28 bytes (vtable + bitfield + AptNativeHash)
//   mpSuperConstructor  AptValue* .... +0x1C   (the chained super-constructor;
//                                               a ref-counted GC value)
//
// vtable object-type index = AptVFT_Prototype (20), confirmed by the ctor's
// `li r4, 0x14` (20) argument to the base.
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptPrototype, the AptVFT_*
// index, objectMemberLookup/objectMemberSet, SetSuperConstructor) are an
// external/middleware API and kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"   // AptValueWithHash base + AptVFT_Prototype
                                                        // (transitively: AptValue, AptNativeString)

class AptPrototype : public AptValueWithHash
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE5FE8) -------------
    // AptPrototype is a garbage-collected value, so its block comes from the GC
    // value pool (gpGCPoolManager) and operator new flips the AptValueGC_MemItem
    // "allocated" flag -- exactly like the rest of the GC value family
    // (AptNativeFunction / AptArray). Defined out-of-line in AptPrototype.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // @ 0x82AEFEF0 -- construct an empty prototype (AptVFT_Prototype, hash
    // capacity 8); the super-constructor starts null and delayed deletion is
    // disabled.
    AptPrototype();

    // ---- the super-constructor (the "__constructor__" member) -------------
    // @ 0x82AD6418 -- (re)set the chained super-constructor, AddRef'ing the new
    // value and Release'ing the old (ref-counted, via the AptValue vtable:
    // AddRef = vtbl[0], Release = vtbl[1]).
    void SetSuperConstructor(AptValue* pSuperConstructor);

    AptValue* GetSuperConstructor() const { return mpSuperConstructor; }

    // ---- AptValue object-model virtual overrides -------------------------
    // The prototype exposes exactly one native member, "__constructor__":
    // objectMemberLookup returns the super-constructor for that name (else null),
    // and objectMemberSet routes that name to SetSuperConstructor (else reports
    // "not handled"). All other members go through the property hash.
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;   // @0x82AD63C8
    virtual bool      objectMemberSet(AptValue* const pThis,
                                      const AptNativeString* const pName,
                                      AptValue* const pValue);                        // @0x82ADC8C8

    // ---- GC virtual overrides --------------------------------------------
    // RegisterReferences marks the property hash + the super-constructor;
    // DestroyGCPointers releases the super-constructor then tears the hash down.
    virtual void RegisterReferences();   // @0x82AE25F0
    virtual void DestroyGCPointers();    // @0x82AF0E80

protected:
    // @ 0x82AEFFA0 -- resets the AptPrototype vtable then chains to the base
    // teardown. AptPrototype owns no destructor work of its own: the embedded
    // AptNativeHash (in the AptValueWithHash base) destroys itself, and the
    // super-constructor reference is released by DestroyGCPointers (the GC path),
    // not the plain destructor -- so the X360 body is the codegen of an empty
    // user destructor. The `vector deleting destructor' thunk @0x82AEFFF8 is
    // compiler-generated and intentionally not hand-written.
    virtual ~AptPrototype();

private:
    // +0x1C -- the chained super-constructor (a ref-counted GC AptValue). The
    // ctor starts it null; SetSuperConstructor maintains the refcount;
    // DestroyGCPointers releases + clears it.
    AptValue* mpSuperConstructor;   // +0x1C

    // Reserve room for this many native members in the base property hash. The
    // X360 ctor passes `8` (li r5, 8) to AptValueWithHash.
    static const int32_t KI_HASH_CAPACITY = 8;
};
