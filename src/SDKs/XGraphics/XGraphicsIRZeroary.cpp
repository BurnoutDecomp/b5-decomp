#include "SDKs/XGraphics/XGraphicsIRZeroary.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena source)

// ===========================================================================
// XGRAPHICS::IRZeroary -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRZeroary.h for
// the shape notes. Each store / branch below is store-for-store with the X360
// asm.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2C060 (ctor body, inlined) -- construct a zeroary node.
//   IRInst::IRInst(this, opcode, context); // base ctor + implicit vtable stamp
//   this->miNumOutputs = 1;                 // stw 1, 0x10(r3)
//   *this              = &off_821B2060;     // implicit IRZeroary vtable stamp, stw 0
//   this->miNumInputs  = 0;                 // stw 0, 0x14(r3)
IRZeroary::IRZeroary(s32 aiOpcode, CFG* apContext)
    : IRInst(aiOpcode)
{
    // apContext is live in the base-ctor argument register in the asm but the
    // 1-arg IRInst base consumes only the opcode; this node stores nothing from
    // it (the value node's context binding is not part of the zeroary ctor).
    (void)apContext;

    miNumOutputs = 1; // +0x10  a zeroary op produces exactly one value
    miNumInputs  = 0; // +0x14  and takes no source operands
}

// @ 0x82C2B080 -- the vector deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm.
IRZeroary::~IRZeroary()
{
}

// @ 0x82C2C060 -- read the owning context's arena (apContext->mpArena, +0x5AC),
// carve a prefixed IRZeroary block from it and construct one in place.
// ArenaAllocPrefixed yields null on the X360 Malloc -4 OOM sentinel; construct
// only on success. The X360 stores the arena into the block's leading slot
// before the OOM test (stw r30,0(r11) precedes the beq); ArenaAllocPrefixed
// folds that same prefix store + sentinel check.
IRZeroary* IRZeroary::NewInst(s32 aiOpcode, CFG* apContext)
{
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(IRZeroary));
    if (!lpMem)
        return nullptr;

    return new (lpMem) IRZeroary(aiOpcode, apContext);
}

} // namespace XGRAPHICS
