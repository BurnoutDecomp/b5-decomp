#pragma once

// ===========================================================================
// XGRAPHICS::IRAlu -- the "ALU op" node kind in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A IRInst: an ALU
// node is the general vector/scalar arithmetic instruction that the microcode
// assembler encodes into a GPU ALU instruction word (the AssembleSrcRegConst /
// AssembleDest / Assemble encoder trio). Its opcode is NOT fixed -- it is threaded
// through IRInst::miOpcode and used to index the per-opcode attribute table
// (gaIROpcodeInfo) and the external ALU-opcode map. This header is the canonical
// OWNING home for the class TU IRAlu:
//
//     XGRAPHICS::IRAlu::InstType               @ 0x82C28698
//     XGRAPHICS::IRAlu::SetMask                @ 0x82C286C0
//     XGRAPHICS::IRAlu::SetSwizzle             @ 0x82C28708
//     XGRAPHICS::IRAlu::MarkInstructionAsExport@ 0x82C2B000
//     XGRAPHICS::IRAlu::Assemble               @ 0x82C27AC0  [BLOCKED]
//     XGRAPHICS::IRAlu::AssembleDest           @ 0x82C27218  [BLOCKED]
//     XGRAPHICS::IRAlu::AssembleSrcRegConst    @ 0x82C26CA8  [BLOCKED]
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRAlu adds no members of its own; all
// four reconstructed methods write inherited IRInst fields (the operand modifier
// bank, the operand slot 0 cache, the node-flags word) or reference the owning
// CFG. Three of the seven methods are BLOCKED (the microcode-assembler encoder
// trio); their per-method block reasons live in XGraphicsIRAlu.cpp and they are
// documented as commented-out declarations here for the record only, matching the
// IRExport / CFG blocked-method convention. `XGRAPHICS` is an X360 graphics-SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (homed in XGraphicsCFG.h)

struct IRAlu : public IRInst
{
    // @ 0x82C28698 -- instruction-type tag. Unlike the other IR node kinds --
    // whose InstType returns a plain tag STRING -- IRAlu::InstType returns the
    // address of a static per-class descriptor object (unk_821AF1F8, an
    // un-recovered X360 rodata blob), so the return type is a bare pointer.
    void* InstType();

    // @ 0x82C286C0 -- set the write-mask byte of operand slot `aiSlot`, component
    // `aiComponent`, to `aiValue` (the value must NOT be in [4,5]; that range
    // asserts). Returns this. Writes the inherited per-operand modifier bank.
    IRAlu* SetMask(s32 aiSlot, s32 aiComponent, s32 aiValue);

    // @ 0x82C28708 -- set the swizzle byte of operand slot `aiSlot`, component
    // `aiComponent`, to `aiValue` (same [4,5] assert). Returns this. Compiles
    // store-for-store identically to SetMask (same modifier-bank target; the two
    // differ only in the assert's source line in IR/IRinst.hpp).
    IRAlu* SetSwizzle(s32 aiSlot, s32 aiComponent, s32 aiValue);

    // @ 0x82C2B000 -- retarget this ALU node as an export: cache operand slot 0's
    // type/index (aiOperandType/aiOperand), clear the output count, set the export
    // node-flags bits (+0xE4 |= 0x42), and register the node in the owning CFG's
    // root set (tail-call CFG::AddToRootSet). Returns the root-set slot pointer.
    u32* MarkInstructionAsExport(CFG* apContext, s32 aiOperandType, s32 aiOperand);

    // ---- declared-only (see XGraphicsIRAlu.cpp for the blocked reasons) --------

    // @ 0x82C27AC0 -- encode this ALU node into a microcode ALU instruction word.
    // BLOCKED (see .cpp): records the node against its instruction slot through an
    // un-homed foreign microcode-emitter context (raw +4 / +52 offsets, the same
    // collaborator that keeps IRExport::Assemble blocked), and dispatches the
    // operand count through an un-attested vtable slot (*(vtable + 4)). Declared
    // here for the record only; not defined.
    //   int Assemble(/* emitter ctx */ void* apEmitter, /* alu word */ u32* apInst);

    // @ 0x82C27218 -- encode this node's destination operand into the ALU word.
    // BLOCKED (see .cpp): reaches the CFG through a raw +0xAB0 hop across the
    // un-homed foreign function context at IRInst::mpContext, reads an un-attested
    // CFG field (+0x854), and calls the un-homed XGRAPHICS::CFG::Number. Declared
    // here for the record only; not defined.
    //   int AssembleDest(u32* apInst);

    // @ 0x82C26CA8 -- encode a source register / constant operand into the ALU
    // word. BLOCKED (see .cpp): same raw +0xAB0 CFG hop across the un-homed foreign
    // function context, the un-homed XGRAPHICS::CFG::Number, and a tail-call to the
    // un-homed, un-named sub_82C26BE0. Declared here for the record only; not
    // defined.
    //   int AssembleSrcRegConst(u32* apInst, int aiSlot, int aiChannel);
};

} // namespace XGRAPHICS
