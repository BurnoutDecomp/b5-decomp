// EATech Apt -- AptRenderItemAnimation.   PS3 EXTERNAL ctor 0x814004 / Render 0x7E49B0.
//   Clone @0x82AEFD70 (reuses the sprite copy-ctor, then stamps the animation flag).
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

AptRenderItemAnimation::AptRenderItemAnimation(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItemSprite(pCharacter, nCreatedOnTick)
{
    // FLAG: console rotate-masks mFlags; 0x240000 is the animation render-type bits.
    mFlags = (mFlags & ~0x003C0000u) | 0x00240000u;
}

// Clone copy-ctor -- sprite copy then re-stamp the animation render-type bits.
AptRenderItemAnimation::AptRenderItemAnimation(const AptRenderItemAnimation* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItemSprite(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00240000u;
}

// Clone @0x82AEFD70
AptRenderItem* AptRenderItemAnimation::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemAnimation));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemAnimation(this, nCreatedOnTick, bCopyExtended);
}

void AptRenderItemAnimation::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}
