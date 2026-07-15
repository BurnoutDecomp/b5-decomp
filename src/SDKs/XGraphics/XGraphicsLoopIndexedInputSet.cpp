#include "SDKs/XGraphics/XGraphicsLoopIndexedInputSet.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, loop counter + arena)

// ===========================================================================
// XGRAPHICS::LoopIndexedInputSet -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XGraphicsLoopIndexedInputSet.h for the attested shape. Each store / branch
// below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

LoopIndexedInputSet::LoopIndexedInputSet(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the LoopIndexedInputSet vtable on entry, matching
    // `*a1 = off_821B19AC`.)
    // Zero the derived member (+0x30), then draw the next sequential loop index
    // from the owning CFG into the base usage slot (+0x10) and advance the graph's
    // loop-indexed-set counter (post-increment, exactly as the asm reloads +0x590).
    mUnk30  = 0;                        // +0x30
    miUsage = apContext->miNextLoopIndex; // +0x10
    ++apContext->miNextLoopIndex;
}

LoopIndexedInputSet* LoopIndexedInputSet::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a LoopIndexedInputSet block from the context's arena, prefixed with
    // the owning arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc
    // -4 OOM sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(LoopIndexedInputSet));
    if (!lpMem)
        return nullptr;

    return new (lpMem) LoopIndexedInputSet(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
