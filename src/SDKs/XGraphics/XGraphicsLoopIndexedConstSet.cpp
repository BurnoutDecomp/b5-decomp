#include "SDKs/XGraphics/XGraphicsLoopIndexedConstSet.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, loop-const counter + arena)

// ===========================================================================
// XGRAPHICS::LoopIndexedConstSet -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XGraphicsLoopIndexedConstSet.h for the attested shape. Each store / branch
// below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

LoopIndexedConstSet::LoopIndexedConstSet(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the LoopIndexedConstSet vtable on entry, matching
    // `*a1 = off_821B1988`.)
    // Zero the derived member (+0x30), then draw the next sequential loop-const
    // index from the owning CFG into the base usage slot (+0x10) and advance the
    // graph's loop-indexed-const counter (post-increment, exactly as the asm
    // reloads +0x58C).
    mUnk30  = 0;                                // +0x30
    miUsage = apContext->miNextLoopIndexedConst; // +0x10
    ++apContext->miNextLoopIndexedConst;
}

LoopIndexedConstSet* LoopIndexedConstSet::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a LoopIndexedConstSet block from the context's arena, prefixed with
    // the owning arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc
    // -4 OOM sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(LoopIndexedConstSet));
    if (!lpMem)
        return nullptr;

    return new (lpMem) LoopIndexedConstSet(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
