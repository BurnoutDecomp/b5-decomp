// ===========================================================================
// EATech Apt -- AptLookup out-of-line bodies. Reconstructed STRICTLY from the
// X360 ARTIST.XEX (no Feb-2007 source / no DecFIGS DWARF exist for this class):
//     AptLookup::AptLookup  @ 0x82AE5F90
//     AptLookup::Initialize @ 0x82AE7E88
//     AptLookup::Shutdown   @ 0x82AD7838
//
// The MSVC `vector deleting destructor' @0x82AE7F90 is a compiler-generated thunk
// (per-element base-vtable revert via off_82145594, then -- when the delete flag is
// set -- free the 12*N+8-byte block back to the shared Apt pool). It is dropped,
// not written: the implicit ~AptLookup/~AptValue chain plus the pooled
// operator delete[] below express the same teardown, and Shutdown's `delete[]`
// re-emits it.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"

// The pool array + its live count (X360 off_8324E4A8 / dword_8324E4AC). Null/zero
// until Initialize builds the pool.
AptLookup* AptLookup::spLookupTable = 0;
int        AptLookup::siNumLookups  = 0;

// ctor @0x82AE5F90 (store-for-store):
//     sub_82AE3000(this, 8);           // AptValueNoGC base ctor, eType = AptVFT_Lookup
//     *(this + 8) = 0;                 // mnIndex = 0
//     *this = off_82145734;            // (the implicit AptLookup vtable store)
//     AptValue::setIsDefined(this, 1); // this value is defined
//     AptValue::setRefCount(this, 0xFFF); // pin MAX_REFCOUNT -- pool entries are
//                                         //   never freed individually
// (setIsDefined / setRefCount return r3 == this in the asm; they are the void
// bitfield writers declared in AptValue.h, chained on the same receiver.)
AptLookup::AptLookup()
    : AptValueNoGC(AptVFT_Lookup)
    , mnIndex(0)
{
    setIsDefined(1);
    setRefCount(MAX_REFCOUNT);   // pinned: a pool entry is never released on its own
}

// Initialize @0x82AE7E88 -- build the fixed AptLookup pool once.
//
// X360: siNumLookups = gAptLookupPoolSize (dword_8324E4AC = dword_82F733EC); if the
// table is not already built, allocate gAptLookupPoolSize entries from the shared
// Apt pool (the size-saving array path folded into operator new[] -- the
// 12*N+4 / +4 header arithmetic + the array count cookie), default-construct each,
// then assign each entry its array position. The `if (!spLookupTable)` guard makes
// it idempotent. (The degenerate gAptLookupPoolSize == 0 case routes through the
// global ::operator new in the X360's array-new-of-zero codegen and constructs
// nothing; `new AptLookup[0]` reproduces that -- a valid empty array, no entries.)
void AptLookup::Initialize()
{
    siNumLookups = gAptLookupPoolSize;

    if (spLookupTable == 0)
    {
        spLookupTable = new AptLookup[siNumLookups];

        for (int liIndex = 0; liIndex < siNumLookups; ++liIndex)
        {
            spLookupTable[liIndex].mnIndex = liIndex;
        }
    }
}

// Shutdown @0x82AD7838 -- tear the pool down and clear it.
//
// X360 is the codegen of `delete[] spLookupTable`: if the array cookie
// (*(spLookupTable - 1), the element count) is non-zero it dispatches the vector
// deleting destructor (vtbl +0x38, flag 3 = destruct every element + free); if the
// cookie is zero (the empty-array case) it frees the bare block via operator
// delete[]; either way it then nulls the table. Reconstructed as the idiomatic
// delete[] it lowered from.
void AptLookup::Shutdown()
{
    if (spLookupTable != 0)
    {
        delete[] spLookupTable;
        spLookupTable = 0;
    }
}
