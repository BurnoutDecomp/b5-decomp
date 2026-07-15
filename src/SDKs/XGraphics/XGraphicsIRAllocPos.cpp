#include "SDKs/XGraphics/XGraphicsIRAllocPos.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRAllocPos -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRAllocPos.h for
// the shape notes. Each store / branch below is store-for-store with the X360
// asm, mirroring the committed IR-instruction sibling XGraphicsIRNop.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2B780 -- return the instruction-type tag string "alloc_pos"
// (lis/addi aAllocPos; blr).
const char* IRAllocPos::InstType()
{
    return "alloc_pos";
}

// @ 0x82C2B790 -- the scalar deleting destructor is a compiler-generated thunk
// (it restamps the base IRInst vtable to &off_821AEB78, then, on the deleting
// flag, frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(
// *(this - 1))); per project policy deleting-destructor thunks are dropped, not
// written. The destructor proper performs no member teardown in the asm.
IRAllocPos::~IRAllocPos()
{
}

// Construction inlined into NewInst on X360 (no standalone ctor symbol). The asm
// calls IRInst::IRInst(this, 0x8E, context) -- the base ctor + implicit IRInst
// vtable stamp -- then, on entry to this derived ctor body, the IRAllocPos vtable
// (off_821B2618) is stamped, and the three flag stores below run:
//   this->muUnkA8      = 0;               // stw 0, 0xA8(r3)
//   this->miNumInputs  = 0;               // stw 0, 0x14(r3)
//   this->miNumOutputs = 1;               // stw 1, 0x10(r3)
//   *this              = &off_821B2618;   // stw off_821B2618, 0(r3)  (implicit)
IRAllocPos::IRAllocPos(CFG* apContext)
    : IRInst(142) // li r4, 0x8E -- IRAllocPos hardcodes its own opcode
{
    // apContext is live in the base-ctor argument register (r5) in the asm but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from it
    // (its arena binding is resolved in NewInst before construction).
    (void)apContext;

    muUnkA8      = 0; // +0xA8  FLAG scalar word
    miNumInputs  = 0; // +0x14
    miNumOutputs = 1; // +0x10
}

// @ 0x82C2C400 -- carve a prefixed IRAllocPos block from the context's arena and
// construct one in place. The asm fetches the arena at context+0x5AC (CFG::mpArena),
// Malloc's sizeof(node)+prefix bytes (964 on X360), writes the owning-arena prefix
// word, and returns null on the X360 Malloc -4 OOM sentinel (surfaced by
// ArenaAllocPrefixed); it constructs only on success.
IRAllocPos* IRAllocPos::NewInst(s32 aiOpcode, CFG* apContext)
{
    // The X360 factory ABI passes (opcode, context) uniformly across node kinds,
    // but IRAllocPos hardcodes its own opcode 0x8E (asm: li r4, 0x8E), so the
    // passed opcode is dead -- r3 is overwritten before it is read.
    (void)aiOpcode;

    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRAllocPos));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRAllocPos(apContext);
}

} // namespace XGRAPHICS
