#pragma once

// ===========================================================================
// XGRAPHICS::BaseAddr -- a base-address indexing virtual register in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// It is one of the indexing value kinds (StandardIndex, AutoIndexVtx, HosCoord,
// BaseAddr) that derive from XGRAPHICS::TempValue: its ctor chains straight
// through TempValue::TempValue (which itself forwards to VRegInfo::VRegInfo and
// draws the owning CFG's next temp index) and then stamps its own BaseAddr
// vtable. Unlike TempValue it performs NO derived store of its own -- the only
// thing its ctor adds past the base chain is the vtable stamp, so it carries no
// data member past the TempValue base.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::BaseAddr::BaseAddr @ 0x82C2A060  (ctor)
//     XGRAPHICS::BaseAddr::NewItem  @ 0x82C2C8F8  (arena factory)
// The compiler-generated scalar-deleting-destructor thunk (0x82C2A5C8) is
// dropped per project policy; the implicit virtual ~BaseAddr provides the same
// behaviour (it chains the base ~TempValue -> ~VRegInfo), and the arena free the
// thunk performs is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 a BaseAddr is
// exactly 0x34 bytes -- the same size as its TempValue base, adding no field
// (NewItem allocates 0x38 = a 4-byte owner-arena prefix + the 0x34 object). The
// PC x64 recon accesses every field by NAME (semantic parity, not byte-matching),
// so the widened base + pointers no longer preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsTempValue.h" // XGRAPHICS::TempValue (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct BaseAddr : public TempValue
{
    // BaseAddr adds no data member past the TempValue base -- the X360 object is
    // exactly the base size (0x34). The ctor's only derived action is stamping
    // the BaseAddr vtable; it performs no store of its own (it draws no counter
    // and touches no member beyond what the TempValue base ctor already wrote).

    // @ 0x82C2A060 -- construct a base-address indexing value: run the TempValue
    // base ctor (index / type / owning CFG, which draws the CFG's next temp
    // index) and stamp the BaseAddr vtable.
    BaseAddr(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C2C8F8 -- arena factory: carve a prefixed BaseAddr block from the
    // context's arena and construct one in place. Returns null on OOM (the X360
    // Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static BaseAddr* NewItem(s32 aiIndex, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
