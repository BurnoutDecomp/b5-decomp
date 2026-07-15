#pragma once

// ===========================================================================
// XGRAPHICS::IRBinary -- the "two-operand op" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst: a binary instruction has exactly one output and TWO input operands
// (miNumOutputs = 1, miNumInputs = 2) and, like the unary / move / projection
// nodes, binds no operand VReg in its ctor. Its opcode is NOT fixed like
// IRProjection's 128 -- it is threaded in from the caller (the several two-operand
// value kinds each stamp their own opcode), exactly as in IRUnary / IRMov. This
// header is the canonical OWNING home for the class TU IRBinary:
//
//     XGRAPHICS::IRBinary::IRBinary                 @ 0x82C2BF80 (inlined ctor)
//     XGRAPHICS::IRBinary::NewInst                  @ 0x82C2BF80 (arena factory)
//     XGRAPHICS::IRBinary::`vector deleting dtor'   @ 0x82C2B120 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRBinary adds no members of its own
// (construction only writes inherited IRInst fields + stamps its own vtable
// off_821B2130); its only distinct state is that vtable and the single-out /
// two-in operand init. The ctor was folded inline into NewInst by the compiler;
// it is de-inlined here (per the outlining-inlined-functions guidance), mirroring
// the IRUnary / IRMov / IRProjection arena-factory idiom. `XGRAPHICS` is an X360
// graphics-SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRBinary : public IRInst
{
    // @ 0x82C2BF80 (folded into NewInst) -- construct a binary node: delegate to
    // the IRInst(opcode) base ctor, mark it single-output, and set two inputs. No
    // operand slots are touched (like IRUnary; unlike IRMov, which resets its two
    // slots). `apContext` is the caller context the compiler also leaves live in
    // the base-ctor argument register (r5); the 1-arg IRInst base ignores it,
    // exactly as in IRUnary / IRMov / IRProjection, so this node stores nothing
    // from it -- only NewInst dereferences it (for the arena).
    IRBinary(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2B120 -- destructor. The vector-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena
    // via the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped
    // per project policy; the destructor proper performs no member teardown in the
    // asm, so the class destructor is trivial.
    virtual ~IRBinary();

    // @ 0x82C2BF80 -- arena factory: carve a prefixed IRBinary block from the
    // context's arena and construct one in place. The asm reads the arena from the
    // context (+0x5AC = CFG::mpArena), Malloc's object+prefix, stores the owning
    // arena into the prefix slot, and skips construction on the X360 Malloc -4 OOM
    // sentinel -- all of which is the ArenaAllocPrefixed idiom. Returns null on OOM.
    static IRBinary* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
