#include "SDKs/XGraphics/XGraphicsExportSlot.h"

#include <new> // placement new

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "SDKs/XGraphics/XGraphicsArena.h"          // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"            // CFG (owning context, arena)

// ===========================================================================
// XGRAPHICS::ExportSlot -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsExportSlot.h for the attested shape. Each store / branch / assert
// below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

ExportSlot::ExportSlot(s32 aiUsage, s32 aiType, CFG* apContext)
    : VRegInfo(aiUsage, aiType, apContext) // base ctor: this / index(=usage) / type / context
{
    // (The compiler sets the ExportSlot vtable on entry, matching `*a1 = vtable`.)
    // Assert the export usage code is one of the five microcode banks. The
    // condition string is recovered verbatim from the rodata the asm passes to
    // SSMDebugPrint ("Assertion failed: %s (%s:%u)"); it fires when aiUsage < 0
    // or aiUsage > 4 (signed compares in the asm).
    CGS_ASSERT(aiUsage >= 0 && aiUsage <= 4, "(0 <= _usage) && (_usage <= 4)");

    // Derived stores, in asm order: set the base flag byte at +0x05, then record
    // the biased usage code (usage + 0x21) in the base usage slot at +0x10,
    // overwriting the base ctor's default of -1.
    mbUnk05 = 1;              // +0x05
    miUsage = aiUsage + 33;   // +0x10  (0x21)
}

ExportSlot* ExportSlot::NewItem(s32 aiUsage, s32 aiType, CFG* apContext)
{
    // Carve an ExportSlot block from the context's arena, prefixed with the
    // owning arena pointer. ArenaAllocPrefixed yields null on the X360 Malloc -4
    // OOM sentinel; construct in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(ExportSlot));
    if (!lpMem)
        return nullptr;

    return new (lpMem) ExportSlot(aiUsage, aiType, apContext);
}

} // namespace XGRAPHICS
