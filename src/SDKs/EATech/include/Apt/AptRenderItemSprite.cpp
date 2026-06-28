// ===========================================================================
// EATech Apt -- AptRenderItemSprite.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x810B6C / Render 0x7E49AC / PushRenderData 0x7F249C /
//   PopRenderData 0x7ECAD0 / PushRenderDataAbsolute 0x7F2958.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"

// ctor @0x810B6C -- base render item + an empty instance name + the sprite
// render-type flag.
AptRenderItemSprite::AptRenderItemSprite(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
    // mInstanceName default-constructs to the shared empty string.
{
    // FLAG: the console also rotate-masks mFlags; 0x140000 is the sprite
    // render-type bits.
    mFlags |= 0x00140000u;
}

// Render @0x7E49AC -- empty: a movie-clip draws nothing itself; its display-list
// children render through the render tree.
void AptRenderItemSprite::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}

// PushRenderData @0x7F249C -- bracket the children's render with the sprite's
// transforms. (FLAG: the console first notifies a render-data-tracking hook
// keyed on the instance name -- dword_1059C6FC; null/no-op here.)
void AptRenderItemSprite::PushRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation, int) const
{
    PushMatrices(pCtx, this);
}

// PopRenderData @0x7ECAD0
void AptRenderItemSprite::PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation, int) const
{
    PopMatrices(pCtx, this);
    // FLAG: the console then notifies dword_1059C700 with the instance name.
}

// PushRenderDataAbsolute @0x7F2958
void AptRenderItemSprite::PushRenderDataAbsolute(AptRenderingContext* pCtx) const
{
    // FLAG: the console first notifies the render-data hook (dword_1059C6FC).
    PushMatricesAbsolute(pCtx, this);
}
