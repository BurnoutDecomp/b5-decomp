// ===========================================================================
// EATech Apt -- AptRenderItemMorph.   DECOMPILED from the X360 ARTIST.XEX
// (cross-checked vs PS3 EXTERNAL ELF DWARF).
//   GetRatio 0x82AD5058 / Render 0x82AFEEC0 / Clone 0x82AECA30 / dtor 0x82AECAA8.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemMorph.h"
#include "SDKs/EATech/include/Apt/AptCharacterMorph.h"     // start/end shape characters
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"   // colour-transform stack + gAptFuncs
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                              // placement new

// ctor -- base render item + the morph render-type flag.
AptRenderItemMorph::AptRenderItemMorph(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
    , mfRatio(0.0f)
{
    mFlags = (mFlags & ~0x3F00u) | 0x800u;   // morph=8; x64 type field (XB1 ctor 0x140826EF0 `or 800h`)
}

// Clone copy-ctor -- base clone copy + re-stamp the morph render-type flag.
AptRenderItemMorph::AptRenderItemMorph(const AptRenderItemMorph* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
    , mfRatio(0.0f)
{
    mFlags = (mFlags & ~0x3F00u) | 0x800u;   // morph=8; x64 type field (XB1 ctor 0x140826EF0 `or 800h`)
}

AptRenderItemMorph::~AptRenderItemMorph() {}

// Clone @0x82AECA30 -- pool-allocate a fresh morph render item copy-initialised
// from this one (null on pool exhaustion).
AptRenderItem* AptRenderItemMorph::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemMorph));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemMorph(this, nCreatedOnTick, bCopyExtended);
}

// GetRatio @0x82AD5058
f32 AptRenderItemMorph::GetRatio() const { return mfRatio; }

// Render @0x82AFEEC0 -- crossfade the morph character's start/end shapes by
// mfRatio. The alpha (scale) channel of the context's current colour transform is
// set to (1-ratio)*255 for the start shape and ratio*255 for the end shape (each
// clamped to [-255,255]), pushed via the SetColourTransform render hook, bracketed
// by a colour-transform push/pop so the fade does not leak into siblings.
void AptRenderItemMorph::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    AptCharacterMorph* pMorph = static_cast<AptCharacterMorph*>(mpCharacter);

    pCtx->pushColourTransform();
    AptCXForm& cxCurrent = pCtx->GetCurrentColourTransform();

    float fStartAlpha = -((mfRatio * 255.0f) - 255.0f);   // (1 - ratio) * 255
    if (fStartAlpha < -255.0f) fStartAlpha = -255.0f;
    if (fStartAlpha >  255.0f) fStartAlpha =  255.0f;
    cxCurrent.scale.SetValuef(AptColorHelper::Alpha, fStartAlpha);
    gAptFuncs.pfnSetColourTransform(&cxCurrent);
    if (pMorph->mpStartCharacter)
        pMorph->mpStartCharacter->render(pCtx, eOp, nTick);   // AptCharacter::render @0x810E74

    float fEndAlpha = mfRatio * 255.0f;
    if (fEndAlpha < -255.0f) fEndAlpha = -255.0f;
    if (fEndAlpha >  255.0f) fEndAlpha =  255.0f;
    cxCurrent.scale.SetValuef(AptColorHelper::Alpha, fEndAlpha);
    gAptFuncs.pfnSetColourTransform(&cxCurrent);
    if (pMorph->mpEndCharacter)
        pMorph->mpEndCharacter->render(pCtx, eOp, nTick);     // AptCharacter::render @0x810E74

    pCtx->popColourTransform();
}
