#include "SDKs/XGraphics/XGraphicsIRUnary.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRUnary -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRUnary.h for
// the shape notes. Each store / branch below is store-for-store with the X360
// asm.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2BE50 (folded into NewInst) -- construct a unary node.
//   IRInst::IRInst(this, opcode /*, context in r5, ignored by 1-arg base*/);
//   this->miNumOutputs = 1;             // stw 1, 0x10(r3)
//   *this              = &off_821B20C8; // implicit IRUnary vtable stamp, stw 0(r3)
//   this->miNumInputs  = 1;             // stw 1, 0x14(r3)
IRUnary::IRUnary(s32 aiOpcode, CFG* apContext)
    : IRInst(aiOpcode)
{
    // apContext is live in the base-ctor argument register (r5) in the asm but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from it
    // (the unary node's context binding is not part of construction).
    (void)apContext;

    miNumOutputs = 1; // +0x10
    miNumInputs  = 1; // +0x14
}

// @ 0x82C2B0D0 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRUnary::~IRUnary()
{
}

// @ 0x82C2BE50 -- carve a prefixed IRUnary block from the context's arena and
// construct one in place. ArenaAllocPrefixed yields null on the X360 Malloc -4
// OOM sentinel; construct only on success. The opcode is threaded through from
// the caller (unlike IRProjection's fixed 128).
IRUnary* IRUnary::NewInst(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRUnary));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRUnary(aiOpcode, apContext);
}

} // namespace XGRAPHICS
