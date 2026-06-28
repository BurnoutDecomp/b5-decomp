#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemAnimation: the renderable for an imported/animation
// movie character. AptRenderItemAnimation : AptRenderItemSprite -- a sprite with
// a distinct render-type flag. SHAPE + BODY from the PS3 EXTERNAL ELF
// (22AptRenderItemAnimation): ctor 0x814004, Render 0x7E49B0 (empty).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"

struct AptRenderItemAnimation : public AptRenderItemSprite
{
    AptRenderItemAnimation(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x814004
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;  // @0x7E49B0 (empty)
};
