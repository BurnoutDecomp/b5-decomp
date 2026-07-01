#pragma once

// ===========================================================================
// EATech Apt -- AptCIHNone: the null / "none" character-instance-handle.
//
// The empty-handle singleton in the AptCIH (display-list node) family. The Apt
// runtime brings up one pinned AptCIHNone (constructed once from
// AptValueInitialize) to stand in for an absent CIH -- the handle equivalent of
// the AptNone "undefined" value. It is a full AptCIH (so ActionScript can hold
// a reference to it and the display-list / GC machinery treats it uniformly),
// built with a null character and null parent, marked defined, and pinned at
// MAX_REFCOUNT so the shared singleton is never individually freed.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (X360-only, like
// the sibling singletons AptNone / AptLookup / AptExtern). SHAPE + BODY are
// therefore reconstructed STRICTLY from the X360 ARTIST.XEX:
//     AptCIHNone::AptCIHNone                    @ 0x82B00DC8  (ctor)
//     AptCIHNone::`scalar deleting destructor'  @ 0x82B00E30  (compiler thunk -- DROPPED)
//
// LAYOUT: AptCIHNone adds NO members -- it is exactly an AptCIH (10 dwords / 40
// bytes on the console, pinned by the deleting destructor's
// AptCIH::operator delete(this, 40)). The "none-ness" is carried entirely by the
// AptCIHNone vtable (the ctor's *this = off_82145FF0 store) plus the pinned /
// defined AptValue flag word; the base AptCIH ctor runs with (pCharacter = null,
// pParent = null) so there is no character instance or parent link.
//
// BASE / vtbl index: AptCIH (AptValueGC). The ctor does NOT change the AptValue
// bitfield value-type tag -- the base AptCIH ctor sets it to
// AptVFT_CharacterInstHandle and AptCIHNone leaves it (its only write to the
// bitfield word is the low-7-bit flag set + the trailing setIsDefined/setRefCount;
// the high meValueType field is untouched). AptVFT_CIHNone (37) is the polymorphic
// vtable-table index that distinguishes the C++ type, supplied automatically by
// the vtable pointer the C++ ctor stores -- not a value-tag the bitfield carries.
//
// The deleting destructor (@0x82B00E30) is a compiler thunk (`delete this`
// codegen: restore the AptCIH vtable, run ~AptCIH, then -- iff the delete flag is
// set -- AptCIH::operator delete(this, 40)); it is regenerated from
// virtual ~AptCIHNone() + the inherited AptCIH GC operator delete, so it is not
// hand-written here (project policy: deleting-destructor thunks are dropped).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"   // AptCIH base (+ its GC operator new/delete)

int AptValueInitialize();   // AptInit.cpp @0x82B02800 -- the designated singleton bootstrap

class AptCIHNone : public AptCIH
{
    // The console's value-singleton bootstrap constructs the one pinned "EmptyCIH"
    // placeholder (dword_8324D700 = new AptCIHNone) -- the sole external ctor user.
    friend int ::AptValueInitialize();

protected:
    // @0x82B00DC8 -- the X360 ctor:
    //   AptCIH::AptCIH(this, /*pCharacter*/0, /*pParent*/0);  // base node ctor
    //   this->bitfield = (this->bitfield & ~0x7Fu) | 0x25;    // low-7 AptValue flags
    //   *this = off_82145FF0;                                 // (automatic) AptCIHNone vtable
    //   AptValue::setIsDefined(this, 1);                      // the handle IS defined
    //   AptValue::setRefCount(this, 0xFFF);                   // pin MAX_REFCOUNT (singleton)
    // The 0x25 low-7 set is: mbIsAllocated | mbIsInDeferredVector |
    // mbAllowsDelayedDeletion (clearing the rest of the low 7, including
    // mbIsDefined which setIsDefined(1) immediately re-sets). Body in AptCIHNone.cpp.
    AptCIHNone();

    // Trivial: AptCIHNone owns no extra members, so teardown is just the chained
    // ~AptCIH (which the C++ destructor emits, along with the vtable revert and
    // the pooled block free via the inherited AptCIH::operator delete).
    virtual ~AptCIHNone() {}
};
