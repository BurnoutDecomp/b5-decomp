#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptXmlNode.
//
// The ActionScript XMLNode object value: a property-bearing AS value (the node's
// attributes / childNodes / nodeName / nodeValue live in the inherited property
// hash) plus a single class-flags word. It is one of the scriptable AS object
// value types the interpreter materialises -- AptActionInterpreter::_createObject
// constructs one (via AptXmlNode::operator new + the ctor) when AS code builds an
// XMLNode.
//
// BASE: AptObject. The X360 `scalar deleting destructor' @0x82AF5C28 chains to
// AptObject::~AptObject (not AptValueWithHash::~), and sizeof == 0x20 ==
// sizeof(AptObject), so AptXmlNode derives from AptObject and adds NO data of its
// own -- the +0x1C class-flags word is AptObject::mClassFlags (inherited). The ctor
// @0x82AF1B08 emits `AptValueWithHash(this, 24, 8)` + the inline mClassFlags
// zero/mask, which is exactly the INLINED AptObject(AptVFT_XmlNode, 8) ctor body.
//
// SHAPE: no Feb-2007 / DecFIGS header is in scope for this class, so the LAYOUT
// is recovered from the X360 ARTIST.XEX:
//     AptXmlNode::AptXmlNode                       @ 0x82AF1B08
//     AptXmlNode::operator new                     @ 0x82AE6F78
//     AptXmlNode::operator delete                  @ 0x82AF1B60
//     AptXmlNode::`scalar deleting destructor'     @ 0x82AF5C28  (compiler thunk -- dropped)
//
// LAYOUT (sizeof = 32 / 0x20, pinned by `operator delete(this, 32)` in the
// deleting-destructor thunk + the `li r4, 0x20` there):
//   AptObject base ................... 32 bytes (AptValueWithHash 28 + mClassFlags
//                                      @+0x1C). AptXmlNode adds no fields of its own.
//
// vtable object-type index = AptVFT_XmlNode (24), confirmed by the ctor's
// `li r4, 0x18` (24) argument to the base.
//
// The X360 ledger attests only these four functions for this class, so this is an
// honest minimal owning header: AptXmlNode adds only the GC pool new/delete and
// inherits everything else (the class-flags word + the AS object-model / GC
// interface) from AptObject. The property-bearing virtuals (GetNativeHashVirtual /
// RegisterReferences / DestroyGCPointers / ...) are the base's and are not
// re-declared here (no AptXmlNode override exists in the X360 ledger).
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptXmlNode, the AptVFT_*
// index, mClassFlags) are an external/middleware API and kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptObject.h"   // AptObject base (AptValueWithHash + mClassFlags) + AptVFT_XmlNode
                                                // (transitively: AptValueWithHash, AptValue, AptNativeString)

class AptXmlNode : public AptObject
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE6F78) -------------
    // AptXmlNode is a garbage-collected value (AptValueGC base via
    // AptValueWithHash), so its block comes from the GC value pool
    // (gpGCPoolManager) and operator new flips the AptValueGC_MemItem
    // "allocated" flag -- exactly like the rest of the GC value family
    // (AptPrototype / AptArray / AptGlobalExtensionObject). Defined out-of-line
    // in AptXmlNode.cpp so the header need not pull in the pool-manager type.
    static void* operator new(size_t size);             // @0x82AE6F78
    static void  operator delete(void* p, size_t size); // @0x82AF1B60

    // @0x82AF1B08 -- construct an empty XMLNode (AptVFT_XmlNode, hash capacity 8);
    // the class-flags word starts cleared.
    AptXmlNode();

protected:
    // Derived-class base ctor (e.g. AptXml passes AptVFT_Xml, 0). The X360 inlines
    // the AptObject(eType, nHashCapacity) construction into each derived ctor; this
    // de-inlined form lets AptXml build with its own vtable index / hash capacity
    // while still chaining through the AptXmlNode -> AptObject base teardown.
    AptXmlNode(AptVirtualFunctionTable_Indices eType, int nHashCapacity);

    // @0x82AF5C28 (the `scalar deleting destructor' thunk) sets this object's
    // vtable then chains to the base teardown (and, with the delete flag,
    // operator delete(this, 32)); i.e. AptXmlNode adds nothing over the base
    // destructor (the embedded AptNativeHash in the AptValueWithHash base destroys
    // itself). The thunk is a compiler artifact and is dropped; this is the real
    // (empty) destructor body.
    virtual ~AptXmlNode();

private:
    // mClassFlags (+0x1C, the hasClass/implemented-objects word) is INHERITED from
    // AptObject -- the X360 `scalar deleting destructor' @0x82AF5C28 chains to
    // AptObject::~AptObject (not AptValueWithHash::~), and sizeof == 0x20 ==
    // sizeof(AptObject), proving AptXmlNode derives from AptObject and adds no data.

    // Reserve room for this many native members in the base property hash. The
    // X360 ctor passes `8` (li r5, 8) to the AptObject base.
    static const int32_t KI_HASH_CAPACITY = 8;
};
