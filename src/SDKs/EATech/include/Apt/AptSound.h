#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptSound: the AS `Sound` object.
//
// AptSound : AptObject -- the ActionScript "Sound" object. Like every scriptable
// AS object it is a property-bearing AptObject (named members + prototype); the
// Sound methods/properties (attachSound/start/stop/setVolume/setPan/getBytesLoaded
// /...) are exposed as native methods/properties driven by the ActionScript
// interpreter rather than as C++ members of this class, so this is the minimal
// owning home for the type itself. Its AptValue object-type index is AptVFT_Sound
// (= 13, AptValue.h).
//
// SHAPE/BODY from the X360 ARTIST.XEX pseudocode + assembly. The X360 ledger
// attests only the destructor for this class:
//     AptSound::~AptSound                @ 0x82AF1690
//     AptSound::`vector deleting destructor' @ 0x82AF16A0  (a compiler thunk --
//                                                           dropped, not written)
//   - ~AptSound @0x82AF1690: stores this object's vtable (off_82145DDC, the
//     AptSound vtable) then tail-calls AptObject::~AptObject -- the textbook
//     derived-class destructor codegen for a class that adds no data members. The
//     reconstructed empty-bodied virtual dtor reproduces it (the compiler
//     regenerates the vtable-set + base-dtor chain).
//   - The deleting-destructor thunk's `AptObject::operator delete(this, 32)` pins
//     sizeof(AptSound) == 32 == sizeof(AptObject) (AptValueWithHash 28 +
//     mClassFlags 4 on PPC): AptSound has NO members beyond AptObject.
//
// Per the DWARF/ledger-gating rule the rest of the Sound surface (the native sound
// methods) is left out -- the X360 build exposes them as native functions, not
// members of this class (and there is no PS3 DWARF for this class) -- so this is an
// honest minimal owning header.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"

struct AptSound : public AptObject
{
    AptSound();           // construct the Sound object (AptVFT_Sound, property hash)
    virtual ~AptSound();  // @0x82AF1690
};
