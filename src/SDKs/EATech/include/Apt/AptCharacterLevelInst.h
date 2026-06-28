#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterLevelInst: the character instance for a "level"
// (top-level movie / root) character in the render spine.
//
// It is a leaf of the AptCharacterInst family (AptCIH -> AptCharacterInst ->
// AptRenderItem). It carries NO extra instance data over the base: the only
// thing that distinguishes the level instance is its C++ type (its vtable) --
// every const read and every writable-item path is the inherited
// AptCharacterInst behaviour, driving the same AptRenderItem.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class. SHAPE is
// reconstructed STRICTLY from the X360 ARTIST.XEX, which exposes only the
// compiler-generated teardown thunk:
//     AptCharacterLevelInst::`vector deleting destructor'  @ 0x82AF8728
//
// LAYOUT: AptCharacterLevelInst adds NO members -- it is exactly an
// AptCharacterInst (4 dwords / 16 bytes on the console). The thunk proves the
// size: when its delete flag is set it frees the block with
// DOGMA_PoolManager::Deallocate(<non-GC pool>, this, 16), and 16 == the base
// AptCharacterInst sizeof, so the leaf contributes no data of its own.
//
// The vector deleting destructor (@0x82AF8728) is a compiler thunk ("delete
// this" codegen: chain ~AptCharacterInst(), then -- iff the delete flag (a2 & 1)
// is set -- free the 16-byte block via the inherited non-GC pool allocator). It
// is regenerated from the empty ~AptCharacterLevelInst() plus the base teardown,
// so it is not hand-written here (project policy: deleting-destructor thunks are
// dropped). The asm calls AptCharacterInst::~AptCharacterInst() directly and
// adds no member teardown, confirming the leaf has no own destructor body.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCharacterInst base

struct AptCharacterLevelInst : public AptCharacterInst
{
    // Trivial: AptCharacterLevelInst owns no extra members, so teardown is just
    // the chained ~AptCharacterInst (which the C++ destructor emits, along with
    // the pooled-block free the dropped deleting-destructor thunk wraps). Matches
    // the base AptCharacterInst non-virtual destructor contract.
    ~AptCharacterLevelInst() {}
};
