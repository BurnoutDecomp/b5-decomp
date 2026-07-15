#include "SDKs/XGraphics/XGraphicsLoopIndexedOutputSet.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, loop-output counter + arena)

// ===========================================================================
// XGRAPHICS::LoopIndexedOutputSet -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XGraphicsLoopIndexedOutputSet.h for the attested shape. Each store / branch
// below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

LoopIndexedOutputSet::LoopIndexedOutputSet(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the LoopIndexedOutputSet vtable on entry, matching
    // `*a1 = off_821B19D0`.)
    // Zero the derived member (+0x30), then draw the next sequential loop-output
    // index from the owning CFG into the base usage slot (+0x10) and advance the
    // graph's loop-indexed-output counter (post-increment, exactly as the asm
    // reloads +0x594).
    mUnk30  = 0;                                 // +0x30
    miUsage = apContext->miNextLoopIndexedOutput; // +0x10
    ++apContext->miNextLoopIndexedOutput;
}

LoopIndexedOutputSet* LoopIndexedOutputSet::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a LoopIndexedOutputSet block from the context's arena, prefixed with
    // the owning arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc
    // -4 OOM sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(LoopIndexedOutputSet));
    if (!lpMem)
        return nullptr;

    return new (lpMem) LoopIndexedOutputSet(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
