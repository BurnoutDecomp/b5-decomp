#pragma once

// ===========================================================================
// XGRAPHICS::Arena -- the linear (bump-pointer) block allocator used by the
// XGRAPHICS shader-microcode compiler in BURNOUT_X360_ARTIST.XEX. It hands out
// 4-byte-aligned allocations from a growable backing block; when the block is
// exhausted it grows a fresh, larger block from a parent heap. This header is
// the canonical OWNING home for:
//
//     XGRAPHICS::Arena::Malloc @ 0x82C21EC0
//     XGRAPHICS::Arena::Grow   @ 0x82C26AC8
//     XGRAPHICS::Arena::Free   @ 0x82C21F88
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of these three methods. `XGRAPHICS`
// is an X360 graphics-SDK boundary, so its identifiers are preserved verbatim
// per the naming convention.
//
// LAYOUT grounded in the asm (X360 byte offsets; only the fields these three
// methods touch are attested). On X360 every field is 4-byte-wide, so pointers
// occupy one slot each; the PC x64 recon accesses everything by NAME (semantic
// parity, not byte-matching), so the widened pointers do not need to preserve
// these literal offsets.
//   +0x00 mpHeap        -- parent heap the arena grows blocks from (Grow reads
//                          a block-alloc fn @+0x5A0 and its context @+0x5A4).
//   +0x04 mpBlock       -- current backing block (an ArenaBlock header + payload).
//   +0x08 mpCurrent     -- bump pointer: next free byte in the current block.
//   +0x0C mpHighWater   -- running peak the bump pointer has ever reached
//                          (never reset by Grow; used only for reuse stats).
//   +0x10 mpEnd         -- one past the last usable byte of the current block.
//   +0x14 mpLastAlloc   -- the pointer returned by the most recent Malloc
//                          (Free rewinds the bump pointer to it, LIFO).
//   +0x18 muFreeCount   -- stats: number of LIFO Free()s serviced.
//   +0x1C muReuseCount  -- stats: number of Malloc()s that reused freed space.
//   +0x20 muFreeBytes   -- stats: total bytes returned by Free().
//   +0x24 muReuseBytes  -- stats: total bytes re-handed-out below the high water.
//   +0x28 mbTrackStats  -- when set, Malloc/Free maintain the four counters
//                          above and Free zero-fills + rewinds (LIFO free).
// The stats field NAMES/semantics are inferred from how the four counters are
// updated; the layout and behaviour are asm-exact.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

// Header written at the head of every backing block Grow() allocates. The old
// block is chained through mpPrevBlock so the whole chain can be walked/freed;
// the payload the arena hands out begins immediately after this header.
// On X360 sizeof(ArenaBlock) == 8 (the literal `8` the asm bumps by); the PC
// x64 recon uses sizeof() so the widened pointer stays correct.
struct ArenaBlock
{
    ArenaBlock* mpPrevBlock; // +0x00 previously-active block (chain link)
    u32         muBlockSize; // +0x04 total bytes in this block (header + payload)
};

// FLAG PC-platform leaf: the parent heap object is a large foreign allocator
// (the class:<global> heap) owned by another TU; only its block-alloc callback
// @+0x5A0 and the context @+0x5A4 that Arena::Grow invokes are attested, so it
// is modelled as opaque padding up to those two fields.
struct ArenaHeap
{
    u8    maPad000[0x5A0];                            // +0x000 opaque (owned elsewhere)
    void* (*mpfnAllocBlock)(void* apContext, int aiSize); // +0x5A0 block allocator
    void* mpAllocContext;                            // +0x5A4 first arg to the allocator
};

class Arena
{
public:
    // @ 0x82C21EC0 -- carve `aiSize` bytes (rounded up to 4) off the current
    // block, growing a new block first if it would not fit. Returns the alloc.
    void* Malloc(int aiSize);

    // @ 0x82C21F88 -- LIFO free: if stats tracking is on and `apPtr` was the
    // most recent Malloc, zero it and rewind the bump pointer. Returns `apPtr`.
    void* Free(void* apPtr);

    // @ 0x82C26AC8 -- allocate a fresh backing block (>= a 12248-byte payload)
    // from the parent heap, chaining the old block, and reset the bump pointer.
    void* Grow(int aiSize);

    ArenaHeap*  mpHeap;       // +0x00 parent heap the arena grows blocks from
    ArenaBlock* mpBlock;      // +0x04 current backing block
    u8*         mpCurrent;    // +0x08 bump pointer (next free byte)
    u8*         mpHighWater;  // +0x0C peak the bump pointer has ever reached
    u8*         mpEnd;        // +0x10 one past the last usable byte in the block
    u8*         mpLastAlloc;  // +0x14 pointer the most recent Malloc returned
    u32         muFreeCount;  // +0x18 stats: LIFO frees serviced
    u32         muReuseCount; // +0x1C stats: mallocs that reused freed space
    u32         muFreeBytes;  // +0x20 stats: total bytes freed
    u32         muReuseBytes; // +0x24 stats: total bytes reused below high water
    bool        mbTrackStats; // +0x28 maintain the counters + LIFO-free behaviour
};

} // namespace XGRAPHICS
