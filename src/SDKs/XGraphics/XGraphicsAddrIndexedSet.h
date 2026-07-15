#pragma once

// ===========================================================================
// XGRAPHICS::AddrIndexedSet -- an address-indexed-set virtual register in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// Like XGRAPHICS::TempValue it is a value-kind derived DIRECTLY from
// XGRAPHICS::VRegInfo: its ctor forwards straight to VRegInfo::VRegInfo (index /
// type / owning CFG) and then stamps its own AddrIndexedSet vtable. Unlike a
// TempValue it draws no counter -- it just records the caller's index into the
// base usage slot.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::AddrIndexedSet::AddrIndexedSet @ 0x82C2A0A0  (ctor)
//     XGRAPHICS::AddrIndexedSet::NewItem        @ 0x82C2CA00  (arena factory)
// The compiler-generated `vector deleting destructor' thunk (0x82C2A620) is
// dropped per project policy; the implicit virtual ~AddrIndexedSet provides the
// same behaviour (it chains the base ~VRegInfo), and the arena free the thunk
// performs is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 an AddrIndexedSet
// is exactly 0x30 bytes (NewItem allocates 0x34 = a 4-byte owner-arena prefix +
// the 0x30 object) -- it adds NO field past the VRegInfo base (the ctor's only
// derived store lands in the base usage slot at +0x10). The PC x64 recon accesses
// every field by NAME (semantic parity, not byte-matching), so the widened base +
// pointers no longer preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsVReg.h" // XGRAPHICS::VRegInfo (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct AddrIndexedSet : public VRegInfo
{
    // AddrIndexedSet adds no data member past the VRegInfo base -- the X360 object
    // is exactly the base size (0x30). The ctor's one derived store writes the
    // caller's index into the base usage slot (+0x10, GetUsage()); nothing else
    // in the layout is touched by this kind.

    // @ 0x82C2A0A0 -- construct an address-indexed set: run the VRegInfo base ctor
    // (index / type / owning CFG), stamp the AddrIndexedSet vtable, then record the
    // index in the base usage slot.
    AddrIndexedSet(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C2CA00 -- arena factory: carve a prefixed AddrIndexedSet block from the
    // context's arena and construct one in place. Returns null on OOM (the X360
    // Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static AddrIndexedSet* NewItem(s32 aiIndex, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
