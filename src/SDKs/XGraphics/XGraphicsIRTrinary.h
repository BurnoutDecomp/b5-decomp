#pragma once

// ===========================================================================
// XGRAPHICS::IRTrinary -- the "three-operand op" node kind in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst: a trinary instruction has exactly one output and THREE input operands
// (miNumOutputs = 1, miNumInputs = 3) and, like the binary / unary / move nodes,
// binds no operand VReg in its ctor. Its opcode is NOT fixed like IRProjection's
// 128 -- it is threaded in from the caller (the several three-operand value kinds
// each stamp their own opcode), exactly as in IRBinary / IRUnary / IRMov. This
// header is the canonical OWNING home for the class TU IRTrinary:
//
//     XGRAPHICS::IRTrinary::NewInst                  @ 0x82C2BFF0 (arena factory,
//                                                      inlined ctor)
//     XGRAPHICS::IRTrinary::`scalar deleting dtor'   @ 0x82C28768 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (the direct twin of the committed
// IRBinary, differing only in the input count and the class vtable). IRTrinary
// adds no members of its own (construction only writes inherited IRInst fields +
// stamps its own vtable off_821AF200); its only distinct state is that vtable and
// the single-out / three-in operand init. The ctor was folded inline into NewInst
// by the compiler; it is de-inlined here (per the outlining-inlined-functions
// guidance), mirroring the committed IRBinary arena-factory idiom. `XGRAPHICS` is
// an X360 graphics-SDK boundary, so its identifiers are preserved verbatim per the
// naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRTrinary : public IRInst
{
    // @ 0x82C2BFF0 (folded into NewInst) -- construct a trinary node: delegate to
    // the IRInst(opcode) base ctor, mark it single-output, and set three inputs.
    // No operand slots are touched (like IRBinary / IRUnary). `apContext` is the
    // caller context the compiler also leaves live in the base-ctor argument
    // register (r5); the 1-arg IRInst base ignores it, exactly as in IRBinary, so
    // this node stores nothing from it -- only NewInst dereferences it (for the
    // arena).
    IRTrinary(s32 aiOpcode, CFG* apContext);

    // @ 0x82C28768 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial. Declared virtual so the class gets its
    // own vtable (off_821AF200), matching the *this = off_821AF200 stamp in NewInst.
    virtual ~IRTrinary();

    // @ 0x82C2BFF0 -- arena factory: carve a prefixed IRTrinary block from the
    // context's arena and construct one in place. The asm reads the arena from the
    // context (+0x5AC = CFG::mpArena), Malloc's object+prefix (0x3C4 on X360),
    // stores the owning arena into the prefix slot, and skips construction on the
    // X360 Malloc -4 OOM sentinel -- all of which is the ArenaAllocPrefixed idiom.
    // The opcode is threaded through from the caller (unlike IRProjection's fixed
    // 128). Returns null on OOM.
    static IRTrinary* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
