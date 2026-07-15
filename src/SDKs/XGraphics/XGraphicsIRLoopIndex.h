#pragma once

// ===========================================================================
// XGRAPHICS::IRLoopIndex -- the "loop index" node kind in the XGRAPHICS
// shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It
// IS-A IRInst: the pseudo-instruction that yields the current loop's iteration
// index. It carries exactly one output and one input (miNumOutputs = 1,
// miNumInputs = 1); its instruction-type tag is the plain string "ir_loopindex".
// This header is the canonical OWNING home for the class TU IRLoopIndex:
//
//     XGRAPHICS::IRLoopIndex::InstType                 @ 0x82C2B6C0
//     XGRAPHICS::IRLoopIndex::`vector deleting dtor'   @ 0x82C2B6D0 (thunk, dropped)
//     XGRAPHICS::IRLoopIndex::NewInst                  @ 0x82C2C380 (arena factory)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (mirroring the committed IR-instruction
// siblings IRAllocMem/IRAllocColor/IRAllocPos, all homed against IRInst).
// IRLoopIndex adds no members of its own (its arena block is 0x3C4 bytes == the
// prefixed base IRInst size); its only distinct state is its own vtable
// (off_821B2528) and the one-output / one-input node init. The construction is
// INLINED into NewInst on X360 (there is no standalone ctor symbol); it is
// expressed here as the de-optimized human ctor that NewInst placement-news.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRLoopIndex : public IRInst
{
    // @ 0x82C2B6C0 -- instruction-type tag. Returns the plain string
    // "ir_loopindex" (lis/addi of aIrLoopindex; blr). Non-virtual, mirroring the
    // committed IRInst / IRAllocMem / IRAllocColor InstType convention.
    const char* InstType();

    // @ 0x82C2B6D0 -- destructor. The vector-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial. Declared virtual so the class gets its
    // own vtable (off_821B2528), matching the *this = off_821B2528 stamp in NewInst.
    virtual ~IRLoopIndex();

    // @ 0x82C2C380 -- arena factory: carve a prefixed IRLoopIndex block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed). The X360 factory ABI passes
    // (opcode, context) uniformly, but IRLoopIndex hardcodes its own opcode 0x7E,
    // so the opcode argument is unused (see the .cpp).
    static IRLoopIndex* NewInst(s32 aiOpcode, CFG* apContext);

private:
    // Construction is inlined into NewInst on X360 (no standalone ctor symbol).
    // Expressed here as the de-optimized human ctor NewInst placement-news: it
    // delegates to the IRInst(opcode) base ctor with the hardcoded opcode 0x7E and
    // marks the node one-output / one-input. Unlike the IRAlloc* siblings it does
    // NOT touch the +0xA8 flag word (the asm has no stw 0,0xA8).
    explicit IRLoopIndex(CFG* apContext);
};

} // namespace XGRAPHICS
