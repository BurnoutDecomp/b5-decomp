// ===========================================================================
// EATech Apt -- AptRenderItemSprite.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x810B6C / Render 0x7E49AC / PushRenderData 0x7F249C /
//   PopRenderData 0x7ECAD0 / PushRenderDataAbsolute 0x7F2958.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// ctor @0x810B6C -- base render item + an empty instance name + the sprite
// render-type flag.
AptRenderItemSprite::AptRenderItemSprite(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
    // mInstanceName default-constructs to the shared empty string.
{
    // FLAG: the console also rotate-masks mFlags; 0x140000 is the sprite
    // render-type bits.
    mFlags |= 0x500u;   // sprite=5; x64 type field (XB1 factory 0x14083C0D0 `or 500h`)
}

// Clone copy-ctor @0x82AEC040 -- base copy + the instance name + (re)stamp the
// sprite render-type bits. (X360 Sprite::Clone delegates entirely to this;
// Animation/Button Clone reuse it then re-stamp their own flag/vtable.)
AptRenderItemSprite::AptRenderItemSprite(const AptRenderItemSprite* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
    , mInstanceName(pSource->mInstanceName)
{
    mFlags = (mFlags & ~0x3F00u) | 0x500u;   // sprite=5; x64 type field
}

// Clone @0x82AEC890 -- pool-allocate a fresh sprite copy-initialised from this one.
AptRenderItem* AptRenderItemSprite::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemSprite));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemSprite(this, nCreatedOnTick, bCopyExtended);
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

// dtor @0x82AEC8E0 (X360 vector deleting destructor) -- mInstanceName destructs
// automatically (the compiler emits its refcount drop) and the base ~AptRenderItem
// runs; the (a2 & 1) sized pool delete of the 0x38-byte block is the compiler's
// deleting-destructor wrapper.
AptRenderItemSprite::~AptRenderItemSprite()
{
}

// GetRenderPropertiesString @0x82AD5030 -- the sprite's render-properties string is
// its instance name (console returns &this->mInstanceName, item+0x34).
EAStringC* AptRenderItemSprite::GetRenderPropertiesString()
{
    return &mInstanceName;
}

// SetRenderPropertiesString @0x82AE5A80 -- assign the instance name through the
// refcount-shared EAStringC::operator= (the console tail-calls it, returning the
// assigned string).
EAStringC& AptRenderItemSprite::SetRenderPropertiesString(const EAStringC& rString)
{
    return mInstanceName = rString;
}
