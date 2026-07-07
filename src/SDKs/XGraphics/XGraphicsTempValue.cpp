#include "SDKs/XGraphics/XGraphicsTempValue.h"

#include <new> // placement new

#include "SDKs/XGraphics/XGraphicsArena.h" // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"   // CFG (owning context, temp counter + arena)

// ===========================================================================
// XGRAPHICS::TempValue -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsTempValue.h for the attested shape. Each store / branch below is
// store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

TempValue::TempValue(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the TempValue vtable on entry, matching `*a1 = vtable`.)
    // Draw the next sequential temp index from the owning CFG and record it in
    // both the base usage slot (+0x10) and this member (+0x30), then advance the
    // graph's temp counter (post-increment, exactly as the asm reloads +0x584).
    s32 liTempIndex = apContext->miNextTempIndex;
    miUsage     = liTempIndex; // +0x10
    miTempIndex = liTempIndex; // +0x30
    ++apContext->miNextTempIndex;
}

TempValue* TempValue::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve a TempValue block from the context's arena, prefixed with the owning
    // arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc -4 OOM
    // sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(TempValue));
    if (!lpMem)
        return nullptr;

    return new (lpMem) TempValue(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
