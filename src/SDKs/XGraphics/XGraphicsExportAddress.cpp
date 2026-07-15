#include "SDKs/XGraphics/XGraphicsExportAddress.h"

#include <new> // placement new

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "SDKs/XGraphics/XGraphicsArena.h"          // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsCFG.h"            // CFG (owning context, arena)

// ===========================================================================
// XGRAPHICS::ExportAddress -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsExportAddress.h for the attested shape. Each store / branch below is
// store-for-store with the X360 asm. The X360 SSMDebugPrint("Assertion failed:
// %s (%s:%u)",..) idiom maps to the project CGS_ASSERT macro (which supplies
// __FILE__/__LINE__, so the original Xenon file/line is dropped -- same
// convention as XGraphicsIRAlu.cpp / XGraphicsIRInst.cpp).
// ===========================================================================

namespace XGRAPHICS
{

ExportAddress::ExportAddress(s32 aiIndex, s32 aiType, CFG* apContext)
    : VRegInfo(aiIndex, aiType, apContext) // base ctor: this / index / type / context
{
    // (The compiler sets the ExportAddress vtable on entry, matching `*a1 = vtable`.)
    // Debug assertion inlined from vreginfo.cpp:851 -- the X360 build prints
    // "Assertion failed: _usage == 0" when the first ctor argument is non-zero.
    // The recovered Xenon assert text names the tested value `_usage`; it is the
    // first ctor argument (r4), preserved verbatim as the message.
    CGS_ASSERT(aiIndex == 0, "_usage == 0");

    // The two derived stores in the asm: mark the export-address flag byte at
    // +0x05 and fix the base usage slot at +0x10 to 32.
    mbUnk05 = 1;  // +0x05
    miUsage = 32; // +0x10
}

ExportAddress* ExportAddress::NewItem(s32 aiIndex, s32 aiType, CFG* apContext)
{
    // Carve an ExportAddress block from the context's arena (0x34 = sizeof object
    // + the 4-byte owner-arena prefix), prefixed with the owning arena pointer.
    // ArenaAllocPrefixed yields null on the X360 Malloc -4 OOM sentinel; construct
    // in place only on success.
    void* lpMem = ArenaAllocPrefixed(apContext->mpArena, sizeof(ExportAddress));
    if (!lpMem)
        return nullptr;

    return new (lpMem) ExportAddress(aiIndex, aiType, apContext);
}

} // namespace XGRAPHICS
