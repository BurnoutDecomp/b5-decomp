#pragma once

// ===========================================================================
// XGRAPHICS::IRLoadTemp -- the "load temp" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst (opcode 118): an instruction that loads a single temporary/virtual
// register value, so it has exactly one output operand (miNumOutputs = 1),
// no inputs (miNumInputs = 0), and binds its operand-0 slot to the VReg it
// loads. This header is the canonical OWNING home for the class TU IRLoadTemp:
//
//     XGRAPHICS::IRLoadTemp::IRLoadTemp                    @ 0x82C2B478 (ctor)
//     XGRAPHICS::IRLoadTemp::InstType                      @ 0x82C2B418
//     XGRAPHICS::IRLoadTemp::`scalar deleting destructor'  @ 0x82C2B428
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRLoadTemp adds no members of its own
// (the ctor only writes inherited IRInst fields + stamps the operand VReg); its
// only distinct behaviour is the vtable and the "load temp" InstType tag.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)
#include "SDKs/XGraphics/XGraphicsVReg.h"   // XGRAPHICS::VReg (operand)

namespace XGRAPHICS
{

struct IRLoadTemp : public IRInst
{
    // @ 0x82C2B478 -- construct a load-temp node over `apTemp`: delegate to the
    // IRInst(118) base ctor, mark it single-output/no-input, and bind operand
    // slot 0 to `apTemp` (caching its type/index and flagging the vreg).
    explicit IRLoadTemp(VReg* apTemp);

    // @ 0x82C2B428 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the vtable then routes the storage back
    // to XGRAPHICS::Arena via Arena::Free(*(this-1))) and is dropped per project
    // policy; the destructor proper performs no member teardown in the asm.
    virtual ~IRLoadTemp();

    // @ 0x82C2B418 -- instruction-type tag string ("load temp").
    const char* InstType();
};

} // namespace XGRAPHICS
