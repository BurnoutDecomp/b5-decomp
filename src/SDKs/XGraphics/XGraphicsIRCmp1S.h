#pragma once

// ===========================================================================
// XGRAPHICS::IRCmp1S -- the "scalar compare" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst: it is the scalar-compare variant of the sibling IRCmp1D node. Like the
// other node kinds it carries the "ir_cmp1s" instruction-type tag, is carved from
// the owning CFG's arena by the NewInst factory, and is torn down by the standard
// arena-prefixed deleting-destructor idiom. This header is the canonical OWNING
// home for the class TU IRCmp1S:
//
//     XGRAPHICS::IRCmp1S::InstType                      @ 0x82C2BD18
//     XGRAPHICS::IRCmp1S::`scalar deleting destructor'  @ 0x82C2BD28 (thunk, dropped)
//     XGRAPHICS::IRCmp1S::NewInst                       @ 0x82C2C1C0 (arena factory)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRCmp1S adds no members of its own
// (construction only writes inherited IRInst fields + stamps its own vtable);
// its only distinct state is that vtable and the "ir_cmp1s" InstType tag.
//
// IMPORTANT -- unlike the IRUnary / IRBinary / IRProjection siblings, the IRCmp1S
// ctor was NOT folded inline into NewInst: NewInst @0x82C2C1C0 reaches it through
// an out-of-line `bl XGRAPHICS::IRCmp1S::IRCmp1S`, and that ctor is not one of this
// TU's three functions (it is external/unknown -- its asm is not present in this
// TU's postmortem). The ctor is therefore declared here as NewInst's construction
// target, but its internal operand/count stores cannot be grounded from this TU's
// asm; see XGraphicsIRCmp1S.cpp for the FLAGged, base-only ctor reconstruction.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRCmp1S : public IRInst
{
    // @ (out-of-line, external/unknown -- see .cpp) -- construct a scalar-compare
    // node: delegate to the IRInst(opcode) base ctor and stamp the IRCmp1S vtable.
    // NewInst's `bl XGRAPHICS::IRCmp1S::IRCmp1S(v5+1, opcode, context)` is the only
    // attestation of this ctor's signature (this, opcode, context); the ctor body
    // itself is NOT in this TU, so its operand/count stores are FLAGged un-attested
    // in the .cpp. `apContext` mirrors the sibling nodes' unused r5 base-ctor arg.
    IRCmp1S(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2BD28 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial.
    virtual ~IRCmp1S();

    // @ 0x82C2BD18 -- instruction-type tag string ("ir_cmp1s").
    const char* InstType();

    // @ 0x82C2C1C0 -- arena factory: carve a prefixed IRCmp1S block from the
    // context's arena and construct one in place. The asm reads the arena from the
    // context (+0x5AC = CFG::mpArena), Malloc's object+prefix (0x3C4 on X360 =
    // sizeof node + 4-byte prefix), stores the owning arena into the prefix slot,
    // and skips construction on the X360 Malloc -4 OOM sentinel -- all of which is
    // the ArenaAllocPrefixed idiom. Returns null on OOM.
    static IRCmp1S* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
