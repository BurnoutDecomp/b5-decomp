#pragma once

// ===========================================================================
// XGRAPHICS::IRLoadConst -- the "load const" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst (opcode 117): an instruction that loads a single constant/immediate
// value, so like IRLoadTemp it has exactly one output operand (miNumOutputs = 1),
// no inputs (miNumInputs = 0), and binds its operand-0 slot to the constant VReg
// it loads. This header is the canonical OWNING home for the class TU
// IRLoadConst:
//
//     XGRAPHICS::IRLoadConst::IRLoadConst  @ 0x82C2B3B0 (ctor)
//     XGRAPHICS::IRLoadConst::InstType     @ 0x82C2B350
//     XGRAPHICS::IRLoadConst::Kill         @ 0x82C2AEE8 (BLOCKED -- see .cpp)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRLoadConst adds no members of its own
// (the ctor only writes inherited IRInst fields + stamps its own vtable); its
// distinct behaviour is the vtable, the "load const" InstType tag, and a Kill
// override that, for a genuine constant operand (operand-0 type == 11), removes
// the constant from the compiler's per-count constant table before unlinking.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)
#include "SDKs/XGraphics/XGraphicsVReg.h"   // XGRAPHICS::VReg (constant operand)

namespace XGRAPHICS
{

struct IRLoadConst : public IRInst
{
    // @ 0x82C2B3B0 -- construct a load-const node over `apConst`: delegate to the
    // IRInst(117) base ctor, mark it single-output/no-input, and bind operand
    // slot 0 to `apConst` (caching its type/index and flagging the vreg). Same
    // operand-binding idiom as IRLoadTemp, only the opcode + vtable differ.
    explicit IRLoadConst(VReg* apConst);

    // @ 0x82C2B350 -- instruction-type tag string ("load const").
    const char* InstType();

    // ---- declared-only (see XGraphicsIRLoadConst.cpp for the blocked reason) ----

    // @ 0x82C2AEE8 -- unlink the node, first releasing it from the compiler's
    // constant table when operand-0 is a real constant. BLOCKED: the constant-
    // removal branch reaches the VRegTable via a raw +0xAB0 hop through the
    // un-homed foreign "function context" at IRInst::mpContext, and then calls the
    // (also un-reconstructed) VRegTable::RemoveConstant. Declared here for the
    // record only; not defined.
    DListNode* Kill();
};

} // namespace XGRAPHICS
