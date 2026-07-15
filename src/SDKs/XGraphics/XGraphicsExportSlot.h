#pragma once

// ===========================================================================
// XGRAPHICS::ExportSlot -- an export-slot virtual register in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). Like
// XGRAPHICS::AddrIndexedSet it is a value-kind derived DIRECTLY from
// XGRAPHICS::VRegInfo: its ctor forwards straight to VRegInfo::VRegInfo (the
// caller's usage code / type / owning CFG), stamps its own ExportSlot vtable,
// then records the export slot into the base usage slot. The usage code selects
// one of the five microcode export banks (0 <= usage <= 4), and the recorded
// value is that code biased by +33 (0x21) -- the ctor asserts the range and
// stores `usage + 33` into the base usage slot.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::ExportSlot::ExportSlot @ 0x82C2A1C8  (ctor)
//     XGRAPHICS::ExportSlot::NewItem    @ 0x82C2CC68  (arena factory)
// The compiler-generated `scalar deleting destructor' thunk (0x82C2A860) is
// dropped per project policy; the implicit virtual ~ExportSlot provides the
// same behaviour (it chains the base ~VRegInfo), and the arena free the thunk
// performs is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 an ExportSlot is
// exactly 0x30 bytes (NewItem allocates 0x34 = a 4-byte owner-arena prefix +
// the 0x30 object) -- it adds NO field past the VRegInfo base: the ctor's
// derived stores land in the base flag byte at +0x05 (set to 1) and the base
// usage slot at +0x10 (set to usage + 33). The PC x64 recon accesses every
// field by NAME (semantic parity, not byte-matching), so the widened base +
// pointers no longer preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsVReg.h" // XGRAPHICS::VRegInfo (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct ExportSlot : public VRegInfo
{
    // ExportSlot adds no data member past the VRegInfo base -- the X360 object is
    // exactly the base size (0x30). The ctor's two derived stores land in base
    // fields: the flag byte at +0x05 (mbUnk05, set to 1) and the usage slot at
    // +0x10 (miUsage / GetUsage(), set to usage + 33). Nothing else in the layout
    // is touched by this kind.

    // @ 0x82C2A1C8 -- construct an export slot: run the VRegInfo base ctor
    // (aiUsage / aiType / owning CFG), stamp the ExportSlot vtable, assert the
    // usage code is in range (0 <= usage <= 4), set the base flag byte, then
    // record `aiUsage + 33` in the base usage slot. `aiUsage` is forwarded to the
    // base as its index argument (the asm passes the same register through).
    ExportSlot(s32 aiUsage, s32 aiType, CFG* apContext);

    // @ 0x82C2CC68 -- arena factory: carve a prefixed ExportSlot block from the
    // context's arena and construct one in place. Returns null on OOM (the X360
    // Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static ExportSlot* NewItem(s32 aiUsage, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
