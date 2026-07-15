#pragma once

// ===========================================================================
// XGRAPHICS::ExportAddress -- an export-address indexing virtual register in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// Like XGRAPHICS::AddrIndexedSet it is a value-kind derived DIRECTLY from
// XGRAPHICS::VRegInfo: its ctor forwards straight to VRegInfo::VRegInfo (index /
// type / owning CFG) and then stamps its own ExportAddress vtable. Its two derived
// stores mark the register as an export-address slot -- a flag byte (+0x05) set to
// 1 and the base usage slot (+0x10) fixed to 32 -- and it asserts (in the X360
// debug build) that the caller passed a zero first argument.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::ExportAddress::ExportAddress @ 0x82C2A140  (ctor)
//     XGRAPHICS::ExportAddress::NewItem       @ 0x82C2CC10  (arena factory)
// The compiler-generated `scalar deleting destructor' thunk (0x82C2A808) is
// dropped per project policy; the implicit virtual ~ExportAddress provides the
// same behaviour (it chains the base ~VRegInfo), and the arena free the thunk
// performs is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 an ExportAddress is
// exactly 0x30 bytes (NewItem allocates 0x34 = a 4-byte owner-arena prefix + the
// 0x30 object) -- it adds NO field past the VRegInfo base; the ctor's two derived
// stores land in the base flag byte at +0x05 and the base usage slot at +0x10.
// The PC x64 recon accesses every field by NAME (semantic parity, not byte-
// matching), so the widened base + pointers no longer preserve these literal
// displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsVReg.h" // XGRAPHICS::VRegInfo (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct ExportAddress : public VRegInfo
{
    // ExportAddress adds no data member past the VRegInfo base -- the X360 object
    // is exactly the base size (0x30). The ctor's two derived stores write the
    // base flag byte (+0x05 = 1) and the base usage slot (+0x10 = 32); nothing
    // else in the layout is touched by this kind.

    // @ 0x82C2A140 -- construct an export-address value: run the VRegInfo base ctor
    // (index / type / owning CFG), stamp the ExportAddress vtable, mark the flag
    // byte, and fix the usage slot to 32.
    ExportAddress(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C2CC10 -- arena factory: carve a prefixed ExportAddress block from the
    // context's arena and construct one in place. Returns null on OOM (the X360
    // Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static ExportAddress* NewItem(s32 aiIndex, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
