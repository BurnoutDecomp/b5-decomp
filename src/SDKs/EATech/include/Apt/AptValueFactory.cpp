// ===========================================================================
// EATech Apt -- AptValueFactory bodies.
//
// Reconstructed from the X360 ARTIST.XEX (AptValueFactory::CreateArray
// @0x82AF4F30) against the leak shape.
//
// X360 0x82AF4F30 (CreateArray): AptArray::operator new(0x2C) into r3; if r3 != 0
// construct AptArray(r3, /*nCount*/r4, /*ppItems*/r5) and return it; else return
// 0. The arg shuffle (r3->r31=nCount, r4->r30=ppItems; ctor gets r4=nCount,
// r5=ppItems, r3=the freshly allocated this) is the standard codegen of
//     return new AptArray(nCount, ppItems);
// where AptArray::operator new draws the 44-byte block from the GC pool and the
// (nCount, ppItems) ctor fills it. The explicit null check is `operator new`
// returning null (the pool is exhausted / not yet wired) -> skip the ctor and
// return null. De-optimised back to that single `new` expression here.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValueFactory.h"

AptArray* AptValueFactory::CreateArray(int nCount, AptValue** ppItems)
{
    return new AptArray(nCount, ppItems);
}
