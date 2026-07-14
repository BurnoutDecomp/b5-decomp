// ===========================================================================
// EATech Apt -- AptRenderItemShape.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x80FDF4 / Render 0x8113C0.
//   `vector deleting destructor'  0x82AECB88   (compiler thunk; not hand-written
//     -- MSVC synthesizes it from the committed base `virtual ~AptRenderItem()`
//     + sized `operator delete`. AptRenderItemShape adds no members, so the thunk
//     just re-stamps the vtable, runs the trivial derived dtor -> base
//     ~AptRenderItem, then frees the 52-byte pool block when its flag bit is set.)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// ctor @0x80FDF4 -- base render item + the shape render-type flag.
AptRenderItemShape::AptRenderItemShape(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    // FLAG: the console additionally rotate-masks mFlags; 0x40000 is the shape
    // render-type bit (the part that matters for dispatch).
    mFlags |= 0x00040000u;
}

// Clone copy-ctor -- base copy + (re)stamp the shape render-type bits.
AptRenderItemShape::AptRenderItemShape(const AptRenderItemShape* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & 0xFF03FFFFu) | 0x00040000u;
}

// Clone @0x82AECB10 -- pool-allocate a fresh shape render item copy-initialised
// from this one (null on pool exhaustion).
AptRenderItem* AptRenderItemShape::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemShape));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemShape(this, nCreatedOnTick, bCopyExtended);
}

// Render @0x8113C0 -- push the item's transforms, draw the shape geometry, pop.
void AptRenderItemShape::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    PushMatrices(pCtx, this);
    AptCharacter_render(mpCharacter, pCtx, eOp, nTick);
    PopMatrices(pCtx, this);
}
