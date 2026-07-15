#pragma once

// ===========================================================================
// XGRAPHICS::IRNop -- the "no-op" node kind in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A IRInst: the
// trivial pass-through instruction that adds NO operands -- it carries zero
// outputs and zero inputs (miNumOutputs = 0, miNumInputs = 0). Unlike IRMov,
// the ctor does not touch the operand slot caches; it only stamps operand
// slot 0's write-mask/modifier bank to {1,1,1,1} (an all-select modifier word
// loaded from an X360 rodata constant). This header is the canonical OWNING
// home for the class TU IRNop:
//
//     XGRAPHICS::IRNop::IRNop                    @ 0x82C2C670 (ctor)
//     XGRAPHICS::IRNop::NewInst                  @ 0x82C2C710 (arena factory)
//     XGRAPHICS::IRNop::`scalar deleting dtor'   @ 0x82C2C6C0 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (mirroring the committed IR-instruction
// siblings IRMov/IRAlu, both homed against IRInst). IRNop adds no members of its
// own (the ctor only writes inherited IRInst fields + stamps its own vtable); its
// only distinct state is the vtable and the no-op operand init. `XGRAPHICS` is an
// X360 graphics-SDK boundary, so its identifiers are preserved verbatim per the
// naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRNop : public IRInst
{
    // @ 0x82C2C670 -- construct a no-op node: delegate to the IRInst(opcode) base
    // ctor, mark it zero-output / zero-input, and stamp operand slot 0's modifier
    // bank to {1,1,1,1}. `apContext` is the caller context the compiler also leaves
    // live in the base-ctor argument register; the 1-arg IRInst base ignores it
    // (IRMov/IRLoadTemp call the same base ctor without passing context), so this
    // node stores nothing from it -- only the arena factory below dereferences it.
    IRNop(s32 aiOpcode, CFG* apContext);

    // @ 0x82C2C6C0 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena
    // via the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped
    // per project policy; the destructor proper performs no member teardown in the
    // asm, so the class destructor is trivial.
    virtual ~IRNop();

    // @ 0x82C2C710 -- arena factory: carve a prefixed IRNop block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed).
    static IRNop* NewInst(s32 aiOpcode, CFG* apContext);
};

} // namespace XGRAPHICS
