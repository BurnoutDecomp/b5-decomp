// ===========================================================================
// EATech Apt -- AptRenderItemShape.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x80FDF4 / Render 0x8113C0.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"

// ctor @0x80FDF4 -- base render item + the shape render-type flag.
AptRenderItemShape::AptRenderItemShape(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    // FLAG: the console additionally rotate-masks mFlags; 0x40000 is the shape
    // render-type bit (the part that matters for dispatch).
    mFlags |= 0x00040000u;
}

// Render @0x8113C0 -- push the item's transforms, draw the shape geometry, pop.
void AptRenderItemShape::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    PushMatrices(pCtx, this);
    AptCharacter_render(mpCharacter, pCtx, eOp, nTick);
    PopMatrices(pCtx, this);
}
