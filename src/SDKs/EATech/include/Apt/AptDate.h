#pragma once

// ===========================================================================
// EATech Apt -- AptDate: the ActionScript Date object.
//
// AptDate : AptObject -- a scriptable AS object (it carries named members + a
// prototype through its AptObject / AptValueWithHash base). The X360 ARTIST.XEX
// exposes this class only through its destructor; the AS Date methods
// (getFullYear/getMonth/getTime/setTime/...) are native VM methods driven by the
// ActionScript interpreter and live in their own (not-yet-reconstructed) TUs, so
// they are not declared here -- this is an honest minimal owning header for what
// the X360 build attests rather than a fabricated Date surface.
//
// SHAPE from the X360 ARTIST.XEX:
//     AptDate::~AptDate                       @ 0x82AF2AA0
//     AptDate::`vector deleting destructor'   @ 0x82AF2AB0 (compiler thunk -- dropped)
//
// LAYOUT: AptDate adds NO data members of its own -- the asm proves
// sizeof(AptDate) == sizeof(AptObject) == 32: the scalar-deleting path frees with
// `AptObject::operator delete(this, 32)` and the vector-deleting path strides the
// element array by 0x20 (slwi r10, r11, 5). So AptDate == AptObject + the AptVFT_Date
// vtable identity.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"

struct AptDate : public AptObject
{
    // dtor @0x82AF2AA0 -- resets the AptDate vtable then chains to ~AptObject.
    // AptDate owns no members, so the body is empty (the AptValueWithHash member
    // destructor tears down the property hash; the vtable store + base chain are
    // compiler-emitted). Declared virtual to override the AptValue vtable slot.
    virtual ~AptDate();

protected:
    // The AptObject base ctor is protected and requires (eType, nHashCapacity);
    // AptDate is built by the VM's native "Date" constructor (a separate TU), so
    // the ctor is kept protected like the AptObject/AptValueWithHash siblings.
    // The X360 build attests no AptDate ctor of its own, so this just establishes
    // the AptVFT_Date vtable identity over an AptObject; the hash capacity is the
    // small default the other bare AS objects use (not load-bearing for the only
    // attested body, the destructor).
    explicit AptDate(int nHashCapacity = 8)
        : AptObject(AptVFT_Date, nHashCapacity)
    {
    }
};
