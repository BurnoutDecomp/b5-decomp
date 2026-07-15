#pragma once

// ===========================================================================
// XGRAPHICS::IRAllocMem -- the "allocate memory register" node kind in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// It IS-A IRInst: the pseudo-instruction that reserves an output memory-backed
// register in the compiler's IR. It carries exactly one output and zero inputs
// (miNumOutputs = 1, miNumInputs = 0); its instruction-type tag is the plain
// string "alloc_mem". This header is the canonical OWNING home for the class TU
// IRAllocMem:
//
//     XGRAPHICS::IRAllocMem::InstType                 @ 0x82C2B7E0
//     XGRAPHICS::IRAllocMem::`scalar deleting dtor'   @ 0x82C2B7F0 (thunk, dropped)
//     XGRAPHICS::IRAllocMem::NewInst                  @ 0x82C2C488 (arena factory)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (mirroring the committed IR-instruction
// siblings IRAllocPos/IRNop, both homed against IRInst). IRAllocMem adds no
// members of its own; its only distinct state is its own vtable (off_821B2690)
// and the one-output / zero-input node init. The construction is INLINED into
// NewInst on X360 (there is no standalone ctor symbol); it is expressed here as
// the de-optimized human ctor that NewInst placement-news. `XGRAPHICS` is an X360
// graphics-SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRAllocMem : public IRInst
{
    // @ 0x82C2B7E0 -- instruction-type tag. Returns the plain string "alloc_mem"
    // (lis/addi of aAllocMem; blr). Non-virtual, mirroring the committed IRInst /
    // IRAllocPos InstType convention.
    const char* InstType();

    // @ 0x82C2B7F0 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial. Declared virtual so the class gets its
    // own vtable (off_821B2690), matching the *this = off_821B2690 stamp in NewInst.
    virtual ~IRAllocMem();

    // @ 0x82C2C488 -- arena factory: carve a prefixed IRAllocMem block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed). The X360 factory ABI passes
    // (opcode, context) uniformly, but IRAllocMem hardcodes its own opcode 0x8F, so
    // the opcode argument is unused (see the .cpp).
    static IRAllocMem* NewInst(s32 aiOpcode, CFG* apContext);

private:
    // Construction is inlined into NewInst on X360 (no standalone ctor symbol).
    // Expressed here as the de-optimized human ctor NewInst placement-news: it
    // delegates to the IRInst(opcode) base ctor with the hardcoded opcode 0x8F,
    // marks the node one-output / zero-input, and zero-stamps the +0xA8 flag word.
    explicit IRAllocMem(CFG* apContext);
};

} // namespace XGRAPHICS
