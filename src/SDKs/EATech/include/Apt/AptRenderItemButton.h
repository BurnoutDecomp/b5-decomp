#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemButton: the renderable for a button character.
// AptRenderItemButton : AptRenderItemSprite -- like a sprite (its state clips
// render through the tree), distinguished only by the render-type flag. SHAPE +
// BODY from the PS3 EXTERNAL ELF (19AptRenderItemButton): ctor 0x814B58,
// Render 0x7E49B4 (empty).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"

struct AptRenderItemButton : public AptRenderItemSprite
{
    AptRenderItemButton(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x814B58
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;  // @0x7E49B4 (empty)
};
