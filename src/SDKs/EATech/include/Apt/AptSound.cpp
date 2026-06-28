// ===========================================================================
// EATech Apt -- AptSound: the AS `Sound` object.
// Reconstructed from the X360 ARTIST.XEX pseudocode + assembly.
//   AptSound::~AptSound @0x82AF1690 (the `vector deleting destructor' thunk
//   @0x82AF16A0 is a compiler thunk -- dropped, not written).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptSound.h"

// Construct the ActionScript Sound object: an AptObject of value type AptVFT_Sound
// with a small property hash (the native sound methods/properties are populated by
// the AS runtime, not stored as C++ members). Mirrors the sibling AS-object ctors
// (e.g. AptArray's / AptMathObj's default capacity of 8).
AptSound::AptSound() : AptObject(AptVFT_Sound, 8)
{
}

// @0x82AF1690 -- the X360 dtor stores this object's vtable (off_82145DDC, the
// AptSound vtable) then tail-calls AptObject::~AptObject. AptSound adds no data
// members (sizeof == 32 == sizeof(AptObject), pinned by the deleting-dtor thunk's
// `operator delete(this, 32)`), so the body is empty: the compiler regenerates the
// vtable-set + base-destructor chain.
AptSound::~AptSound()
{
}
