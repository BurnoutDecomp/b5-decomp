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
    mFlags = (mFlags & ~0x3F00u) | 0x900u;   // animation=9; x64 type field (XB1 ctor 0x140826AC0 `or 900h`)
}

// Clone copy-ctor -- sprite copy then re-stamp the animation render-type bits.
AptRenderItemAnimation::AptRenderItemAnimation(const AptRenderItemAnimation* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItemSprite(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & ~0x3F00u) | 0x900u;   // animation=9; x64 type field
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

// AptRenderItemAnimation::`scalar deleting destructor' @0x82AEF3F0 -- the MSVC
// `delete` thunk. The X360 body (r3=this, r4=flags):
//     v4 = this[13];                              // mInstanceName (EAStringC)
//     *this = &off_82145898;                      // reinstall this level's vtable
//     EAStringC::DecreaseInternalRefCount(v4);    // drop mInstanceName's buffer ref
//     AptRenderItem::~AptRenderItem(this);        // base teardown
//     if (flags & 1)
//         DOGMA_PoolManager::Deallocate(off_8324D808, this, 56);   // gpNonGCPoolManager
//     return this;
// The vtable store + the mInstanceName refcount drop + the base dtor are exactly the
// implicit ~AptRenderItemAnimation() chain (Animation adds no members, so it runs
// ~AptRenderItemSprite -- which destroys mInstanceName -- then ~AptRenderItem). The
// per-object scalar-deleting destructor has no portable C++ spelling, so it is homed
// as the committed XxxScalarDeletingDtor(T*, char) free function (cf.
// IVideoRendererScalarDeletingDtor @0x827E9F00). off_8324D808 is the same pool the
// sibling Clone allocates from; the sized (56-byte) free matches sizeof(AptRenderItemAnimation).
AptRenderItemAnimation* AptRenderItemAnimationScalarDeletingDtor(AptRenderItemAnimation* pThis, char cFlags)
{
    pThis->~AptRenderItemAnimation();
    if ((cFlags & 1) != 0)
        gpNonGCPoolManager->Deallocate(pThis, sizeof(AptRenderItemAnimation));
    return pThis;
}
