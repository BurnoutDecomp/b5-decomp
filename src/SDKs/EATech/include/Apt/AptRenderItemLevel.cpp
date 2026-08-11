// ===========================================================================
// EATech Apt -- AptRenderItemLevel.   DECOMPILED from the X360 ARTIST.XEX.
//   Clone 0x82AEC7B0 (vtable off_8214587C, render-type flag 0x3C0000).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemLevel.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// Factory ctor -- base render item + the level render-type flag.
AptRenderItemLevel::AptRenderItemLevel(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    mFlags = (mFlags & ~0x3F00u) | 0xF00u;   // level=15; x64 type field (XB1 factory `or 0F00h`)
}

// Clone copy-ctor -- base clone copy + re-stamp the level render-type flag.
AptRenderItemLevel::AptRenderItemLevel(const AptRenderItemLevel* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & ~0x3F00u) | 0xF00u;   // level=15; x64 type field (XB1 factory `or 0F00h`)
}

// Clone @0x82AEC7B0 -- pool-allocate a fresh level render item copy-initialised
// from this one (null on pool exhaustion).
AptRenderItem* AptRenderItemLevel::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemLevel));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemLevel(this, nCreatedOnTick, bCopyExtended);
}
