#include "SDKs/XGraphics/XGraphicsIRNop.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRNop -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRNop.h for the
// shape notes. Each store / branch below is store-for-store with the X360 asm,
// mirroring the committed IR-instruction sibling XGraphicsIRMov.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2C670 -- construct a no-op node.
//   IRInst::IRInst(this, opcode);        // base ctor + implicit vtable stamp
//   *this               = &off_821B2A30; // implicit IRNop vtable stamp, stw 0(r3)
//   this->miNumInputs   = 0;             // stw 0, 0x14(r3)
//   this->miNumOutputs  = 0;             // stw 0, 0x10(r3)
//   this->maModifier[0] = {1,1,1,1};     // stw dword_821B1F54 (=0x01010101), 0x80(r3)
IRNop::IRNop(s32 aiOpcode, CFG* apContext)
    : IRInst(aiOpcode)
{
    // apContext is live in the base-ctor argument register in the asm but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from
    // it (the no-op node has no context binding of its own).
    (void)apContext;

    miNumInputs  = 0; // +0x14
    miNumOutputs = 0; // +0x10

    // +0x80 -- operand slot 0's write-mask/modifier bank. The asm stores a single
    // 32-bit word loaded from the rodata constant dword_821B1F54 (== 0x01010101),
    // i.e. each of the four modifier bytes = 1. Written by NAMED member here.
    maModifier[0][0] = 1;
    maModifier[0][1] = 1;
    maModifier[0][2] = 1;
    maModifier[0][3] = 1;
}

// @ 0x82C2C6C0 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRNop::~IRNop()
{
}

// @ 0x82C2C710 -- carve a prefixed IRNop block from the context's arena and
// construct one in place. The asm fetches the arena at context+0x5AC (CFG::mpArena),
// Malloc's sizeof(node)+prefix bytes, writes the owning-arena prefix word, and
// returns null on the X360 Malloc -4 OOM sentinel (surfaced by ArenaAllocPrefixed);
// construct only on success.
IRNop* IRNop::NewInst(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRNop));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRNop(aiOpcode, apContext);
}

} // namespace XGRAPHICS
