#pragma once

// ===========================================================================
// XGRAPHICS::IRMov -- the "move" node kind in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A IRInst: a move
// instruction has exactly one output and one input operand (miNumOutputs = 1,
// miNumInputs = 1), and its first two operand slots start unbound (cached index
// -1, cached type 0). The opcode is NOT fixed like IRLoadTemp's 118 -- it is
// threaded in from the caller, so the several indexing value kinds that build a
// move (StandardIndex, AutoIndexVtx, HosCoord) each stamp their own opcode. This
// header is the canonical OWNING home for the class TU IRMov:
//
//     XGRAPHICS::IRMov::IRMov                    @ 0x82C2B170 (ctor)
//     XGRAPHICS::IRMov::MakeIRMov                @ 0x82C2C620 (arena factory)
//     XGRAPHICS::IRMov::NewInst                  @ 0x82C2BEB8 (arena factory)
//     XGRAPHICS::IRMov::`scalar deleting dtor'   @ 0x82C2B1D0 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRMov adds no members of its own (the
// ctor only writes inherited IRInst fields + stamps its own vtable); its only
// distinct state is the vtable and the move-specific operand init. `XGRAPHICS`
// is an X360 graphics-SDK boundary, so its identifiers are preserved verbatim
// per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRMov : public IRInst
{
    // @ 0x82C2B170 -- construct a move node: delegate to the IRInst(opcode) base
    // ctor, mark it single-output / single-input, and reset the first two operand
    // slots (cached index -1 = unbound, cached type 0). `apContext` is the caller
    // context the compiler also leaves live in the base-ctor argument register;
    // the 1-arg IRInst base ignores it (IRLoadTemp calls the same base ctor with
    // no context to pass), so this node stores nothing from it -- only the arena
    // factories below dereference it.
    IRMov(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2B1D0 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena
    // via the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped
    // per project policy; the destructor proper performs no member teardown in
    // the asm, so the class destructor is trivial.
    virtual ~IRMov();

    // @ 0x82C2C620 -- arena factory: carve a prefixed IRMov block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed).
    static IRMov* MakeIRMov(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2BEB8 -- a second arena-factory entry point, compiled store-for-store
    // identical to MakeIRMov (same allocate-prefixed-then-construct sequence).
    static IRMov* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
