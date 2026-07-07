#pragma once

// ===========================================================================
// XGRAPHICS::InternalVector -- a minimal growable array of 32-bit words used by
// the XGRAPHICS shader-microcode compiler in BURNOUT_X360_ARTIST.XEX. It backs
// the compiler's small index/hash containers (see InternalHashTable, VRegInfo,
// CFG::AddToRootSet, IRInst::Kill). Elements are plain u32 slots; storage is
// carved from an owning XGRAPHICS::Arena, and Grow reallocates-and-copies out of
// that arena LIFO-style. This header is the canonical OWNING home for:
//
//     XGRAPHICS::InternalVector::InternalVector @ 0x82C26610  (ctor)
//     XGRAPHICS::InternalVector::Grow           @ 0x82C28940
//     XGRAPHICS::InternalVector::Insert         @ 0x82C289C8
//     XGRAPHICS::InternalVector::Remove         @ 0x82C28908
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. Every store/branch is asm-exact;
// because the reconstruction is by NAMED member (semantic parity, not byte-
// matching), the widened x64 pointers do not preserve the literal X360 offsets.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// ATTESTED members (X360 byte offsets; every field is 4-byte-wide on X360):
//   +0x00 muCapacity -- allocated element count (ctor = 2; doubles in Grow)
//   +0x04 muCount    -- number of live elements (ctor = 0)
//   +0x08 mpElements -- storage, muCapacity words, arena-allocated
//   +0x0C mpArena    -- the XGRAPHICS::Arena the storage is carved from
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsArena.h" // XGRAPHICS::Arena

namespace XGRAPHICS
{

class InternalVector
{
public:
    // @ 0x82C26610 -- start empty: capacity 2, count 0, storage carved from the
    // supplied arena (2 words == 8 bytes).
    InternalVector(Arena* apArena);

    // @ 0x82C28940 -- ensure index `auIndex` is addressable: extend the count to
    // cover it, double the capacity until it fits, then reallocate storage from
    // the arena (copying the live prefix) and free the old block. Returns a
    // pointer to element `auIndex` in the (possibly moved) storage.
    u32* Grow(u32 auIndex);

    // @ 0x82C289C8 -- open a hole at `auIndex` (growing if the array is full),
    // shift the tail up one slot, zero the new slot, and return a pointer to it.
    u32* Insert(u32 auIndex);

    // @ 0x82C28908 -- delete element `auIndex` by shifting the tail down one slot
    // and shrinking the count. No-op if `auIndex` is out of range.
    void Remove(u32 auIndex);

    u32    muCapacity; // +0x00 allocated element count
    u32    muCount;    // +0x04 live element count
    u32*   mpElements; // +0x08 storage (muCapacity words, arena-allocated)
    Arena* mpArena;    // +0x0C arena the storage is carved from
};

} // namespace XGRAPHICS
