#pragma once

// ===========================================================================
// XGRAPHICS::IRUnary -- the "unary op" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst: a unary instruction has exactly one output and one input operand
// (miNumOutputs = 1, miNumInputs = 1) and, like the move / projection nodes,
// binds no operand VReg in its ctor. Its opcode is NOT fixed like IRProjection's
// 128 -- it is threaded in from the caller (the several one-operand value kinds
// each stamp their own opcode), exactly as in IRMov. This header is the
// canonical OWNING home for the class TU IRUnary:
//
//     XGRAPHICS::IRUnary::IRUnary                 @ 0x82C2BE50 (inlined ctor)
//     XGRAPHICS::IRUnary::NewInst                 @ 0x82C2BE50 (arena factory)
//     XGRAPHICS::IRUnary::`scalar deleting dtor'  @ 0x82C2B0D0 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRUnary adds no members of its own
// (construction only writes inherited IRInst fields + stamps its own vtable
// off_821B20C8); its only distinct state is that vtable and the single-in/single-
// out operand init. The ctor was folded inline into NewInst by the compiler; it
// is de-inlined here (per the outlining-inlined-functions guidance), mirroring
// the IRMov / IRProjection arena-factory idiom. `XGRAPHICS` is an X360 graphics-
// SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRUnary : public IRInst
{
    // @ 0x82C2BE50 (folded into NewInst) -- construct a unary node: delegate to
    // the IRInst(opcode) base ctor and mark it single-output / single-input. No
    // operand slots are touched (unlike IRMov, which resets its two slots).
    // `apContext` is the caller context the compiler also leaves live in the
    // base-ctor argument register (r5); the 1-arg IRInst base ignores it, exactly
    // as in IRMov / IRProjection, so this node stores nothing from it -- only
    // NewInst dereferences it (for the arena).
    IRUnary(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2B0D0 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena
    // via the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped
    // per project policy; the destructor proper performs no member teardown in
    // the asm, so the class destructor is trivial.
    virtual ~IRUnary();

    // @ 0x82C2BE50 -- arena factory: carve a prefixed IRUnary block from the
    // context's arena and construct one in place. The asm reads the arena from
    // the context (+0x5AC = CFG::mpArena), Malloc's object+prefix, stores the
    // owning arena into the prefix slot, and skips construction on the X360
    // Malloc -4 OOM sentinel -- all of which is the ArenaAllocPrefixed idiom.
    // Returns null on OOM.
    static IRUnary* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
