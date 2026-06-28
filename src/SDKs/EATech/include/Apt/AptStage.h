#pragma once

// ===========================================================================
// EATech Apt -- AptStage: the ActionScript "Stage" object.
//
// AptStage : AptObject -- the scriptable singleton the apt VM exposes to
// ActionScript as the global `Stage` (its align / scaleMode / width / height
// properties drive the Flash display root). It is a plain AptObject: a property-
// bearing AS object (named members + a prototype + the GC property hash) with no
// additional data members of its own -- it differs from a bare AptObject only by
// its vtable (the AptVFT_Stage value-type index) and the native member lookups
// the AS-globals layer installs on it.
//
// SHAPE + the destructor BODY from the X360 ARTIST.XEX:
//     AptStage::~AptStage                       @ 0x82AF2B40
//     AptStage::`vector deleting destructor'    @ 0x82AF2B50  (compiler thunk -- dropped)
//
// LAYOUT: AptObject (32 bytes) + nothing. The X360 `vector deleting destructor'
// frees with AptObject::operator delete(this, 32) and strides the array case by
// 32 bytes/element, pinning sizeof(AptStage) == sizeof(AptObject) == 32 -- i.e.
// AptStage adds no fields. The X360 ledger attests only the destructor for this
// TU; per the project rule (DWARF supplies names, the X360 ledger decides what
// exists) this is the honest minimal owning home -- the Stage-specific native
// member lookups are installed by the not-yet-reconstructed AS-globals layer, not
// overridden here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"

struct AptStage : public AptObject
{
    // FLAG: the X360 AptStage constructor is not part of this TU (it is inlined
    // into / owned by the AS-globals layer that builds the singleton), so its
    // exact property-hash reservation is not attested here. Modelled as the
    // canonical AptObject base init with the AptVFT_Stage value-type index; the
    // capacity mirrors the small-object default the sibling AS objects use.
    AptStage() : AptObject(AptVFT_Stage, 8)
    {
    }

    // @ 0x82AF2B40 -- sets the AptStage vtable pointer then chains to the base
    // AptObject destructor (the property hash is released by the AptValueWithHash
    // member teardown). The X360 body is the codegen of an empty user destructor.
    virtual ~AptStage();
};
