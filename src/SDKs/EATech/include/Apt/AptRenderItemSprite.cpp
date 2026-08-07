// ===========================================================================
// EATech Apt -- AptRenderItemSprite.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x810B6C / Render 0x7E49AC / PushRenderData 0x7F249C /
//   PopRenderData 0x7ECAD0 / PushRenderDataAbsolute 0x7F2958.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderHooks.h"   // the host render-data notify slots (dword_8324E8CC/D0)
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// ctor @0x810B6C -- base render item + an empty instance name + the sprite
// render-type flag.
AptRenderItemSprite::AptRenderItemSprite(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
    // mInstanceName default-constructs to the shared empty string.
{
    // Console encoding: rotate-mask of mFlags, 0x140000 == 5 << 18 (the X360
    // render-type field); the x64 twin is the bits-8-13 field, XB1-verified.
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

// PushRenderData @0x7F249C (X360 @0x82AEF300) -- notify the host render-data push
// hook (dword_8324E8CC; the PS3 twin slot is dword_1059C6FC) for a NAMED sprite --
// the shared-empty instance name (unk_82F72FF8) is skipped via the pointer-identity
// EAStringC::IsEmpty, the same guard + slots the custom-control siblings use --
// then bracket the children's render with the transforms.
void AptRenderItemSprite::PushRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation, int) const
{
    if (!mInstanceName.IsEmpty() && gpfnAptCustomControlPushRenderData)
        gpfnAptCustomControlPushRenderData(mInstanceName.GetBuffer());
    PushMatrices(pCtx, this);
}

// PopRenderData @0x7ECAD0 (X360 @0x82AEBFD0) -- pop the transforms, then notify the
// host render-data pop hook (dword_8324E8D0) for a named sprite.
void AptRenderItemSprite::PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation, int) const
{
    PopMatrices(pCtx, this);
    if (!mInstanceName.IsEmpty() && gpfnAptCustomControlPopRenderData)
        gpfnAptCustomControlPopRenderData(mInstanceName.GetBuffer());
}

// PushRenderDataAbsolute @0x7F2958 (X360 @0x82AEF378) -- the push-hook notify, then
// the absolute (world-space) matrix push.
void AptRenderItemSprite::PushRenderDataAbsolute(AptRenderingContext* pCtx) const
{
    if (!mInstanceName.IsEmpty() && gpfnAptCustomControlPushRenderData)
        gpfnAptCustomControlPushRenderData(mInstanceName.GetBuffer());
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
