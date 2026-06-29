// ===========================================================================
// EATech Apt -- AptRenderItemStaticText.   DECOMPILED from the X360 ARTIST.XEX.
//   Clone 0x82AEC950 / Render 0x82AFEC18 (vtable off_821458D0, flag 0x280000).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemStaticText.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// Factory ctor -- base render item + the static-text render-type flag.
AptRenderItemStaticText::AptRenderItemStaticText(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00280000u;
}

// Clone copy-ctor -- base clone copy + re-stamp the static-text render-type flag.
AptRenderItemStaticText::AptRenderItemStaticText(const AptRenderItemStaticText* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00280000u;
}

// Clone @0x82AEC950 -- pool-allocate a fresh static-text render item copy-init'd
// from this one (null on pool exhaustion).
AptRenderItem* AptRenderItemStaticText::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemStaticText));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemStaticText(this, nCreatedOnTick, bCopyExtended);
}

// Render @0x82AFEC18 -- draw the baked static-text glyph runs.
void AptRenderItemStaticText::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    // FLAG (deferred -- Wave 5): the static-text glyph render walks the
    // AptCharacterStaticText paragraph/glyph layout -- per-paragraph colour
    // transforms and glyph runs, each glyph drawn via AptCharacter::render under a
    // per-glyph vertex matrix (with a debug-bounds path under gAptOptFlags & 4).
    // Deferred until AptCharacterStaticText's text/paragraph/glyph layout is homed;
    // draws nothing until then (no offset-poke placeholder by design).
    (void)pCtx; (void)eOp; (void)nTick;
}
