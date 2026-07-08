#pragma once

// ===========================================================================
// XGRAPHICS::IRLoadInterp -- the "load interp" node kind in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It
// IS-A IRInst (opcode 122): an instruction that loads an interpolated value, so
// like IRLoadTemp it has exactly one output operand (miNumOutputs = 1) and no
// inputs (miNumInputs = 0). Unlike IRLoadTemp it binds no operand VReg in its
// ctor; instead it stamps the node-flags word (+0xE4 |= 0x40) that marks the
// load as interpolated. This header is the canonical OWNING home for the class
// TU IRLoadInterp:
//
//     XGRAPHICS::IRLoadInterp::IRLoadInterp                @ 0x82C2B4E0 (ctor)
//     XGRAPHICS::IRLoadInterp::InstType                    @ 0x82C2B530
//     XGRAPHICS::IRLoadInterp::`vector deleting destructor'@ 0x82C2B540 (thunk)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRLoadInterp adds no members of its
// own (the ctor only writes inherited IRInst fields + stamps its own vtable);
// its only distinct behaviour is the vtable, the load-interp flag bit, and the
// "load interp" InstType tag. `XGRAPHICS` is an X360 graphics-SDK boundary, so
// its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct IRLoadInterp : public IRInst
{
    // @ 0x82C2B4E0 -- construct a load-interp node: delegate to the IRInst(122)
    // base ctor, set the load-interp flag bit (node-flags word +0xE4 |= 0x40),
    // and mark it single-output / no-input. `apContext` is the caller context
    // the compiler also leaves live in the base-ctor argument register (r5); the
    // 1-arg IRInst base ignores it, exactly as in IRMov, so this node stores
    // nothing from it.
    explicit IRLoadInterp(CFG* apContext);

    // @ 0x82C2B540 -- destructor. The vector-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then routes the storage back to XGRAPHICS::Arena via Arena::Free(*(this-1))
    // when the deleting flag is set) and is dropped per project policy; the
    // destructor proper performs no member teardown in the asm.
    virtual ~IRLoadInterp();

    // @ 0x82C2B530 -- instruction-type tag string ("load interp").
    const char* InstType();
};

} // namespace XGRAPHICS
