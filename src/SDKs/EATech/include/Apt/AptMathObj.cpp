// ===========================================================================
// EATech Apt -- AptMathObj: the AS `Math` object.
// Reconstructed from the X360 ARTIST.XEX pseudocode + assembly.
//   AptMathObj::~AptMathObj @0x82AF0AD8 (the `vector deleting destructor' thunk
//   @0x82AF0AE8 is a compiler thunk -- dropped, not written).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptMathObj.h"

// Construct the ActionScript Math object: an AptObject of value type AptVFT_Math
// with a small property hash (the native math methods/properties are populated by
// the AS runtime, not stored as C++ members). Mirrors the sibling AS-object ctors
// (e.g. AptArray's default capacity of 8).
AptMathObj::AptMathObj() : AptObject(AptVFT_Math, 8)
{
}

// @0x82AF0AD8 -- the X360 dtor stores this object's vtable (off_82145B4C, the
// AptMathObj vtable) then tail-calls AptObject::~AptObject. AptMathObj adds no
// data members (sizeof == 32 == sizeof(AptObject), pinned by the deleting-dtor
// thunk's `operator delete(this, 32)`), so the body is empty: the compiler
// regenerates the vtable-set + base-destructor chain.
AptMathObj::~AptMathObj()
{
}
