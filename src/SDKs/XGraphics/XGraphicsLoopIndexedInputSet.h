#pragma once

// ===========================================================================
// XGRAPHICS::LoopIndexedInputSet -- a loop-indexed input-set virtual register in
// the XGRAPHICS shader-microcode intermediate representation
// (BURNOUT_X360_ARTIST.XEX). Like XGRAPHICS::TempValue it is a value-kind derived
// DIRECTLY from XGRAPHICS::VRegInfo: its ctor forwards straight to
// VRegInfo::VRegInfo (index / type / owning CFG) and then stamps its own
// LoopIndexedInputSet vtable. Unlike a TempValue -- which writes the drawn index
// into both its derived member and the base usage slot -- a LoopIndexedInputSet
// zero-initialises its one derived member and records the drawn LOOP index only
// in the base usage slot, advancing the CFG's loop-indexed-set counter (+0x590).
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::LoopIndexedInputSet::LoopIndexedInputSet @ 0x82C2A738  (ctor)
//     XGRAPHICS::LoopIndexedInputSet::NewItem             @ 0x82C2CB08  (arena factory)
// The compiler-generated `vector deleting destructor' thunk (0x82C2A910) is
// dropped per project policy; the implicit virtual ~LoopIndexedInputSet provides
// the same behaviour (it chains the base ~VRegInfo), and the arena free the thunk
// performs is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 a LoopIndexedInputSet
// is exactly 0x34 bytes (NewItem allocates 0x38 = a 4-byte owner-arena prefix +
// the 0x34 object, and the ctor's derived store lands at +0x30). The PC x64 recon
// accesses every field by NAME (semantic parity, not byte-matching), so the
// widened base + pointers no longer preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsVReg.h" // XGRAPHICS::VRegInfo (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct LoopIndexedInputSet : public VRegInfo
{
    // The one field LoopIndexedInputSet adds past the VRegInfo base. The ctor
    // zero-initialises it (the base usage slot instead receives the drawn loop
    // index). FLAG: only the zero-init store at +0x30 is attested -- the field's
    // semantic purpose is not grounded, so it carries an unattested name.
    s32 mUnk30; // +0x30  derived state, zero-initialised  FLAG unattested purpose

    // @ 0x82C2A738 -- construct a loop-indexed input set: run the VRegInfo base
    // ctor (index / type / owning CFG), stamp the LoopIndexedInputSet vtable,
    // zero the derived member, then draw the next loop index from the owning CFG
    // into the base usage slot and advance the graph's loop-indexed counter.
    LoopIndexedInputSet(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C2CB08 -- arena factory: carve a prefixed LoopIndexedInputSet block
    // from the context's arena and construct one in place. Returns null on OOM
    // (the X360 Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static LoopIndexedInputSet* NewItem(s32 aiIndex, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
