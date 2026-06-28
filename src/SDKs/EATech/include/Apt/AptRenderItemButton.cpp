// EATech Apt -- AptRenderItemButton.   PS3 EXTERNAL ctor 0x814B58 / Render 0x7E49B4.
#include "SDKs/EATech/include/Apt/AptRenderItemButton.h"

AptRenderItemButton::AptRenderItemButton(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItemSprite(pCharacter, nCreatedOnTick)
{
    // FLAG: console rotate-masks mFlags; 0x100000 is the button render-type bits
    // (replacing the sprite's, set by the base ctor).
    mFlags = (mFlags & ~0x003C0000u) | 0x00100000u;
}

void AptRenderItemButton::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}
