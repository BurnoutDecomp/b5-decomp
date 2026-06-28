#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptMathObj: the AS `Math` object.
//
// AptMathObj : AptObject -- the singleton ActionScript "Math" object. Like every
// scriptable AS object it is a property-bearing AptObject (named members +
// prototype); the Math statics (abs/sin/cos/sqrt/random/PI/...) are exposed as
// native methods/properties driven by the ActionScript interpreter rather than as
// C++ members of this class, so this is the minimal owning home for the type
// itself.
//
// SHAPE/BODY from the X360 ARTIST.XEX pseudocode + assembly. The X360 ledger
// attests only the destructor for this class:
//     AptMathObj::~AptMathObj                @ 0x82AF0AD8
//     AptMathObj::`vector deleting destructor' @ 0x82AF0AE8  (a compiler thunk --
//                                                             dropped, not written)
//   - ~AptMathObj @0x82AF0AD8: stores this object's vtable (off_82145B4C, the
//     AptMathObj vtable) then tail-calls AptObject::~AptObject -- the textbook
//     derived-class destructor codegen for a class that adds no data members. The
//     reconstructed empty-bodied virtual dtor reproduces it (the compiler
//     regenerates the vtable-set + base-dtor chain).
//   - The deleting-destructor thunk's `AptObject::operator delete(this, 32)` pins
//     sizeof(AptMathObj) == 32 == sizeof(AptObject) (AptValueWithHash 28 +
//     mClassFlags 4 on PPC): AptMathObj has NO members beyond AptObject.
//
// Per the DWARF/ledger-gating rule the rest of the leak's Math surface (the native
// math methods) is left out -- the X360 build exposes them as native functions,
// not members of this class -- so this is an honest minimal owning header.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"

struct AptMathObj : public AptObject
{
    AptMathObj();           // construct the Math object (AptVFT_Math, property hash)
    virtual ~AptMathObj();  // @0x82AF0AD8
};
