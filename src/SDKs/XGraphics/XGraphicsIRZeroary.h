#pragma once

// ===========================================================================
// XGRAPHICS::IRZeroary -- the "zero-operand" node kind in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It
// IS-A IRInst: a zeroary instruction has exactly one output and no inputs
// (miNumOutputs = 1, miNumInputs = 0) -- the microcode-IR node kind for ops
// that produce a value taking no source operands. The opcode is NOT fixed like
// IRLoadTemp's 118 -- it is threaded in from the arena-factory caller. This
// header is the canonical OWNING home for the class TU IRZeroary:
//
//     XGRAPHICS::IRZeroary::IRZeroary            (ctor, inlined into NewInst)
//     XGRAPHICS::IRZeroary::NewInst              @ 0x82C2C060 (arena factory)
//     XGRAPHICS::IRZeroary::`vector deleting dtor'@ 0x82C2B080 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRZeroary adds no members of its own
// (the ctor only writes inherited IRInst fields + stamps its own vtable); its
// only distinct state is the vtable and the zeroary operand counts. `XGRAPHICS`
// is an X360 graphics-SDK boundary, so its identifiers are preserved verbatim
// per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRZeroary : public IRInst
{
    // Construct a zeroary node: delegate to the IRInst(opcode) base ctor, mark
    // it single-output / zero-input. `apContext` is the caller context the
    // compiler also leaves live in the base-ctor argument register; the 1-arg
    // IRInst base ignores it (same as IRMov -- the value node's context binding
    // is not part of the ctor), so this node stores nothing from it -- only the
    // arena factory below dereferences it. Inlined at 0x82C2C060; declared as a
    // real ctor here so the arena factory can placement-construct one.
    IRZeroary(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2B080 -- destructor. The vector-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena
    // via the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped
    // per project policy; the destructor proper performs no member teardown in
    // the asm, so the class destructor is trivial.
    virtual ~IRZeroary();

    // @ 0x82C2C060 -- arena factory: read the owning context's arena
    // (apContext->mpArena, +0x5AC), carve a prefixed IRZeroary block from it and
    // construct one in place. Returns null on the X360 Malloc -4 OOM sentinel
    // (surfaced by ArenaAllocPrefixed).
    static IRZeroary* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
