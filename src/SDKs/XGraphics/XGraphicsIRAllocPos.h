#pragma once

// ===========================================================================
// XGRAPHICS::IRAllocPos -- the "allocate position register" node kind in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// It IS-A IRInst: the pseudo-instruction that reserves an output position/index
// register in the compiler's IR. It carries exactly one output and zero inputs
// (miNumOutputs = 1, miNumInputs = 0); its instruction-type tag is the plain
// string "alloc_pos". This header is the canonical OWNING home for the class TU
// IRAllocPos:
//
//     XGRAPHICS::IRAllocPos::InstType                 @ 0x82C2B780
//     XGRAPHICS::IRAllocPos::`scalar deleting dtor'   @ 0x82C2B790 (thunk, dropped)
//     XGRAPHICS::IRAllocPos::NewInst                  @ 0x82C2C400 (arena factory)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (mirroring the committed IR-instruction
// siblings IRNop/IRAlu, both homed against IRInst). IRAllocPos adds no members of
// its own; its only distinct state is its own vtable (off_821B2618) and the
// one-output / zero-input node init. The construction is INLINED into NewInst on
// X360 (there is no standalone ctor symbol); it is expressed here as the
// de-optimized human ctor that NewInst placement-news. `XGRAPHICS` is an X360
// graphics-SDK boundary, so its identifiers are preserved verbatim per the naming
// convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRAllocPos : public IRInst
{
    // @ 0x82C2B780 -- instruction-type tag. Returns the plain string "alloc_pos"
    // (lis/addi of aAllocPos; blr). Non-virtual, mirroring the committed IRInst /
    // IRAlu InstType convention.
    const char* InstType();

    // @ 0x82C2B790 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial. Declared virtual so the class gets its
    // own vtable (off_821B2618), matching the *this = off_821B2618 stamp in NewInst.
    virtual ~IRAllocPos();

    // @ 0x82C2C400 -- arena factory: carve a prefixed IRAllocPos block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed). The X360 factory ABI passes
    // (opcode, context) uniformly, but IRAllocPos hardcodes its own opcode 0x8E, so
    // the opcode argument is unused (see the .cpp).
    static IRAllocPos* NewInst(s32 aiOpcode, CFG* apContext);

private:
    // Construction is inlined into NewInst on X360 (no standalone ctor symbol).
    // Expressed here as the de-optimized human ctor NewInst placement-news: it
    // delegates to the IRInst(opcode) base ctor with the hardcoded opcode 0x8E,
    // marks the node one-output / zero-input, and zero-stamps the +0xA8 flag word.
    explicit IRAllocPos(CFG* apContext);
};

} // namespace XGRAPHICS
