#include "SDKs/XGraphics/XGraphicsBaseAddr.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena)

// ===========================================================================
// XGRAPHICS::BaseAddr -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsBaseAddr.h for the attested shape. Each store / branch below is
// store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

BaseAddr::BaseAddr(s32 aiIndex, s32 aiType, CFG* apContext)
    : TempValue(aiIndex, aiType, apContext) // base ctor chain: TempValue -> VRegInfo
{
    // (The compiler sets the BaseAddr vtable on entry, matching `*a1 = vtable`.)
    // The asm has NO derived store past the vtable stamp -- everything else this
    // object needs was already written by the TempValue base ctor.
}

BaseAddr* BaseAddr::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a BaseAddr block from the context's arena (0x38 = sizeof(BaseAddr) +
    // the 4-byte owner-arena prefix), prefixed with the owning arena pointer.
    // ArenaAllocPrefixed yields null on the X360 Malloc -4 OOM sentinel; construct
    // in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(BaseAddr));
    if (!lpMem)
        return nullptr;

    return new (lpMem) BaseAddr(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
