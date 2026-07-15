#pragma once

// ===========================================================================
// XGRAPHICS::IRAllocColor -- the "allocate colour register" node kind in the
// XGRAPHICS shader-microcode intermediate representation (BURNOUT_X360_ARTIST.XEX).
// It IS-A IRInst: the pseudo-instruction that reserves an output colour register
// in the compiler's IR. It carries exactly one output and zero inputs
// (miNumOutputs = 1, miNumInputs = 0); its instruction-type tag is the plain
// string "alloc_color". This header is the canonical OWNING home for the class TU
// IRAllocColor:
//
//     XGRAPHICS::IRAllocColor::InstType                 @ 0x82C2B840
//     XGRAPHICS::IRAllocColor::`scalar deleting dtor'   @ 0x82C2B850 (thunk, dropped)
//     XGRAPHICS::IRAllocColor::NewInst                  @ 0x82C2C510 (arena factory)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm (mirroring the committed IR-instruction
// siblings IRNop/IRZeroary and the direct twin IRAllocPos, all homed against
// IRInst). IRAllocColor adds no members of its own (its arena block is 0x3C4 bytes
// == the prefixed base IRInst size); its only distinct state is its own vtable
// (off_821B2708) and the one-output / zero-input node init. The construction is
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

struct IRAllocColor : public IRInst
{
    // @ 0x82C2B840 -- instruction-type tag. Returns the plain string "alloc_color"
    // (lis/addi of aAllocColor; blr). Non-virtual, mirroring the committed IRInst /
    // IRAlu / IRAllocPos InstType convention.
    const char* InstType();

    // @ 0x82C2B850 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated (it restamps the base IRInst vtable to &off_821AEB78,
    // then, when the deleting flag is set, routes the storage back to its arena via
    // the ArenaFreePrefixed idiom -- Arena::Free(*(this - 1))) and is dropped per
    // project policy; the destructor proper performs no member teardown in the asm,
    // so the class destructor is trivial. Declared virtual so the class gets its
    // own vtable (off_821B2708), matching the *this = off_821B2708 stamp in NewInst.
    virtual ~IRAllocColor();

    // @ 0x82C2C510 -- arena factory: carve a prefixed IRAllocColor block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed). The X360 factory ABI passes
    // (opcode, context) uniformly, but IRAllocColor hardcodes its own opcode 0x90,
    // so the opcode argument is unused (see the .cpp).
    static IRAllocColor* NewInst(s32 aiOpcode, CFG* apContext);

private:
    // Construction is inlined into NewInst on X360 (no standalone ctor symbol).
    // Expressed here as the de-optimized human ctor NewInst placement-news: it
    // delegates to the IRInst(opcode) base ctor with the hardcoded opcode 0x90,
    // marks the node one-output / zero-input, and zero-stamps the +0xA8 flag word.
    explicit IRAllocColor(CFG* apContext);
};

} // namespace XGRAPHICS
