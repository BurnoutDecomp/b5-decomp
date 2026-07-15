#include "SDKs/XGraphics/XGraphicsLoopRegister.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, arena)

// ===========================================================================
// XGRAPHICS::LoopRegister -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsLoopRegister.h for the attested shape. Each store / branch below is
// store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

LoopRegister::LoopRegister(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the LoopRegister vtable on entry, matching `*a1 = vtable`.)
    // Record the caller's index in the base usage slot (+0x10), overwriting the
    // base ctor's default of -1 -- the sole derived store in the asm
    // (`stw r30, 0x10(r31)`).
    miUsage = aiIndex; // +0x10
}

LoopRegister* LoopRegister::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a LoopRegister block from the context's arena (0x34 = sizeof(LoopRegister)
    // + the 4-byte owner-arena prefix), prefixed with the owning arena pointer.
    // ArenaAllocPrefixed yields null on the X360 Malloc -4 OOM sentinel; construct
    // in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(LoopRegister));
    if (!lpMem)
        return nullptr;

    return new (lpMem) LoopRegister(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
