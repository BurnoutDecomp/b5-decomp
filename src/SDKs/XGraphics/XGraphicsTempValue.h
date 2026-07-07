#pragma once

// ===========================================================================
// XGRAPHICS::TempValue -- a temporary-value virtual register in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It is
// a value-kind derived from XGRAPHICS::VRegInfo (its ctor forwards straight to
// VRegInfo::VRegInfo and then stamps a TempValue vtable) and is itself the base
// of the indexing value kinds (StandardIndex, AutoIndexVtx, HosCoord, BaseAddr,
// which all chain through TempValue::TempValue). Each TempValue draws a fresh
// sequential temp index from a monotonic counter the owning CFG keeps at +0x584.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::TempValue::TempValue @ 0x82C29668  (ctor)
//     XGRAPHICS::TempValue::NewItem   @ 0x82C2C7F0  (arena factory)
// The compiler-generated scalar-deleting-destructor thunk (0x82C2A308) is
// dropped per project policy; the implicit virtual ~TempValue provides the same
// behaviour (it chains the base ~VRegInfo), and the arena free the thunk performs
// is the ArenaFreePrefixed idiom on the block NewItem carved.
//
// There is NO reference source and NO DWARF for this TU. The shape below is
// reconstructed purely from the ctor / NewItem / deleting-dtor X360 asm.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets. On X360 a TempValue is
// exactly 0x34 bytes (NewItem allocates 0x38 = a 4-byte owner-arena prefix + the
// 0x34 object, and the ctor's last store lands at +0x30). The PC x64 recon
// accesses every field by NAME (semantic parity, not byte-matching), so the
// widened base + pointer no longer preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

#include "SDKs/XGraphics/XGraphicsVReg.h" // XGRAPHICS::VRegInfo (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct TempValue : public VRegInfo
{
    // The one field TempValue adds past the VRegInfo base. The ctor writes the
    // freshly-drawn temp index into both this member (+0x30) and the base
    // GetUsage() slot (+0x10). FLAG: name inferred from the "TempValue" role;
    // only the two stores of the CFG temp counter are attested.
    s32 miTempIndex; // +0x30 sequential temp-value index

    // @ 0x82C29668 -- construct a temp value: run the VRegInfo base ctor, stamp
    // the TempValue vtable, then draw the next temp index from the owning CFG
    // (writing it to the base usage slot and this member) and advance it.
    TempValue(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C2C7F0 -- arena factory: carve a prefixed TempValue block from the
    // context's arena and construct one in place. Returns null on OOM (the X360
    // Malloc -4 sentinel, surfaced by ArenaAllocPrefixed).
    static TempValue* NewItem(s32 aiIndex, s32 aiType, CFG* apContext);
};

} // namespace XGRAPHICS
