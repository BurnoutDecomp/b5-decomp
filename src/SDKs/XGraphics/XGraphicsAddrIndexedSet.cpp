#include "SDKs/XGraphics/XGraphicsAddrIndexedSet.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena)

// ===========================================================================
// XGRAPHICS::AddrIndexedSet -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsAddrIndexedSet.h for the attested shape. Each store / branch below is
// store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

AddrIndexedSet::AddrIndexedSet(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the AddrIndexedSet vtable on entry, matching `*a1 = vtable`.)
    // Record the caller's index in the base usage slot (+0x10), overwriting the
    // base ctor's default of -1 -- the sole derived store in the asm.
    miUsage = aiIndex; // +0x10
}

AddrIndexedSet* AddrIndexedSet::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve an AddrIndexedSet block from the context's arena, prefixed with the
    // owning arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc -4
    // OOM sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(AddrIndexedSet));
    if (!lpMem)
        return nullptr;

    return new (lpMem) AddrIndexedSet(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
