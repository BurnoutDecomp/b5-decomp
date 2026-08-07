// EATech Apt -- AptRenderItemButton.   PS3 EXTERNAL ctor 0x814B58 / Render 0x7E49B4.
//   Clone @0x814C78 (reuses the sprite copy-ctor, then stamps the button flag).
#include "SDKs/EATech/include/Apt/AptRenderItemButton.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

AptRenderItemButton::AptRenderItemButton(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItemSprite(pCharacter, nCreatedOnTick)
{
    // FLAG: console rotate-masks mFlags; 0x100000 is the button render-type bits
    // (replacing the sprite's, set by the base ctor).
    mFlags = (mFlags & ~0x3F00u) | 0x400u;   // button=4; x64 type field bits 8-13
}

// Clone copy-ctor -- sprite copy then re-stamp the button render-type bits.
AptRenderItemButton::AptRenderItemButton(const AptRenderItemButton* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItemSprite(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & ~0x3F00u) | 0x400u;   // button=4; x64 type field bits 8-13
}

// Clone @0x814C78 -- pool-allocate a fresh button render item copy-initialised from this
// one (null on pool exhaustion). Mirrors AptRenderItemAnimation::Clone.
AptRenderItem* AptRenderItemButton::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemButton));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemButton(this, nCreatedOnTick, bCopyExtended);
}

void AptRenderItemButton::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}
