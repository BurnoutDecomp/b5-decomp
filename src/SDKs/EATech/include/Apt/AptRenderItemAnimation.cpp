// EATech Apt -- AptRenderItemAnimation.   PS3 EXTERNAL ctor 0x814004 / Render 0x7E49B0.
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"

AptRenderItemAnimation::AptRenderItemAnimation(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItemSprite(pCharacter, nCreatedOnTick)
{
    // FLAG: console rotate-masks mFlags; 0x240000 is the animation render-type bits.
    mFlags = (mFlags & ~0x003C0000u) | 0x00240000u;
}

void AptRenderItemAnimation::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}
