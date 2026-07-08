#include "SDKs/XGraphics/XGraphicsIRMov.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRMov -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRMov.h for the
// shape notes. Each store / branch below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

// A move touches exactly two operand slots (its output + its input).
static const int KI_MOV_OPERAND_COUNT = 2;

// @ 0x82C2B170 -- construct a move node.
//   IRInst::IRInst(this, opcode);       // base ctor + implicit vtable stamp
//   this->miNumInputs   = 1;            // stw 1, 0x14(r3)
//   *this               = &off_821B2198;// implicit IRMov vtable stamp, stw 0(r3)
//   this->miNumOutputs  = 1;            // stw 1, 0x10(r3)
//   for slot in [0,1]:                  // do { .. } while(--count) over 2 slots
//     this->maOperand[slot]     = -1;   // stw -1, 0x38 + 4*slot
//     this->maOperandType[slot] = 0;    // stw  0, 0x50 + 4*slot
IRMov::IRMov(s32 aiOpcode, CFG* apContext)
    : IRInst(aiOpcode)
{
    // apContext is live in the base-ctor argument register in the asm but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from
    // it (the value node's context binding is not part of the move ctor).
    (void)apContext;

    miNumInputs  = 1; // +0x14
    miNumOutputs = 1; // +0x10

    for (int liSlot = 0; liSlot < KI_MOV_OPERAND_COUNT; ++liSlot)
    {
        maOperand[liSlot]     = -1; // +0x38  unbound operand register index
        maOperandType[liSlot] = 0;  // +0x50  cached operand type
    }
}

// @ 0x82C2B1D0 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRMov::~IRMov()
{
}

// @ 0x82C2C620 -- carve a prefixed IRMov block from the context's arena and
// construct one in place. ArenaAllocPrefixed yields null on the X360 Malloc -4
// OOM sentinel; construct only on success.
IRMov* IRMov::MakeIRMov(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRMov));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRMov(aiOpcode, apContext);
}

// @ 0x82C2BEB8 -- identical arena factory (compiled store-for-store the same as
// MakeIRMov).
IRMov* IRMov::NewInst(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRMov));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRMov(aiOpcode, apContext);
}

} // namespace XGRAPHICS
