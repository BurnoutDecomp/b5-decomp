#pragma once

// ===========================================================================
// XGRAPHICS::IRExport -- the "export" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A
// IRInst (opcode 48): the instruction that writes a shader output to an export
// slot. Its ctor delegates to the IRInst(48) base, stamps the export vtable,
// flags the node (+0xE4 |= 0x12), marks it single-output / single-input, and --
// uniquely among the IR node ctors -- registers the freshly built node in the
// compiler's CFG root set. This header is the canonical OWNING home for the
// class TU IRExport:
//
//     XGRAPHICS::IRExport::IRExport                @ 0x82C2B280 (ctor)  [BLOCKED]
//     XGRAPHICS::IRExport::Assemble                @ 0x82C27EA0         [BLOCKED]
//     XGRAPHICS::IRExport::InstType                @ 0x82C2B2F0
//     XGRAPHICS::IRExport::`scalar deleting dtor'  @ 0x82C2B300 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRExport adds no members of its own
// (the ctor only writes inherited IRInst fields + stamps its own vtable); its
// distinct behaviour is the vtable, the "export" InstType tag, the node-flags
// bits (+0xE4 |= 0x12), the CFG-root-set registration, and the Assemble encoder.
// Two of the four methods are BLOCKED (see XGraphicsIRExport.cpp for the
// per-method reasons); they are documented as commented-out declarations here
// for the record only, matching the CFG.h blocked-method convention.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

struct IRExport : public IRInst
{
    // @ 0x82C2B280 -- construct an export node: delegate to the IRInst(48) base
    // ctor, set the node-flags bits (+0xE4 |= 0x12), mark it single-output /
    // single-input, and register the node in the owning CFG's root set.
    // BLOCKED (see .cpp): the root-set registration reaches the CFG through a raw
    // +0xAB0 hop across the un-homed foreign "function context" (the same
    // collaborator that keeps IRLoadConst::Kill and CFG::BuildUsesAndDefs
    // blocked). Declared here for the record only; not defined.
    //   explicit IRExport(/* foreign function context */ void* apContext);

    // @ 0x82C2B300 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then routes the storage back to XGRAPHICS::Arena via Arena::Free(*(this-1))
    // when the deleting flag is set) and is dropped per project policy; the
    // destructor proper performs no member teardown in the asm, so the class
    // destructor is trivial.
    virtual ~IRExport();

    // @ 0x82C2B2F0 -- instruction-type tag string ("export").
    const char* InstType();

    // @ 0x82C27EA0 -- encode this export node into a microcode ALU instruction.
    // BLOCKED (see .cpp): reaches an un-homed foreign microcode-assembler context
    // by raw offset and calls the un-homed, un-reconstructed IRAlu assembler
    // helpers. Declared here for the record only; not defined.
    //   int Assemble(/* emitter ctx */ void* apEmitter, /* alu inst */ u32* apInst);
};

} // namespace XGRAPHICS
