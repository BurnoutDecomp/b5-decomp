// ===========================================================================
// EATech Apt -- AptCharacterInst.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x81431C / dtor 0x7F8414 / CreateCharacterInst 0x817D40 /
//   GetRenderItem 0x7DF008 / GetRenderItemWritable 0x7EC910 / SetRenderItem
//   0x7E20E8 / SetCharacter 0x80F324 / the const reads 0x7E87xx/0x7E73xx /
//   the writes 0x7F071C/0x7F074C/0x7ED7F4.
//
// The const accessors read straight through mpRenderItem; the writes take the
// tick-writable render item from the render-tree manager first (which
// double-buffers per tick) and swap it in.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"        // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new (factory)

// ctor @0x81431C
AptCharacterInst::AptCharacterInst(AptCharacter* pCharacter)
{
    mpProperties = nullptr;

    AptRenderItem* pItem = nullptr;
    if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
        pItem = AptRTM_CreateItem(pMgr, pCharacter, gnCurrUpdateTick);
    mpRenderItem = pItem;

    // High 6 bits of mTypeFlags = the character's type tag (15 when none).
    const uint32_t uType = pCharacter ? static_cast<uint32_t>(pCharacter->mnType) : 15u;
    mTypeFlags = (mTypeFlags & 0x03FFFFFFu) | (uType << 26);

    if (pItem)
        pItem->AddReference();
}

// dtor @0x7F8414
AptCharacterInst::~AptCharacterInst()
{
    if (mpProperties)
    {
        mpProperties->~AptNativeHash();
        gpNonGCPoolManager->Deallocate(mpProperties, sizeof(AptNativeHash));
        mpProperties = nullptr;
    }
    if (mpRenderItem)
    {
        AptRenderItem* pItem = mpRenderItem;
        mpRenderItem = nullptr;
        pItem->ReleaseReference();
    }
}

// CreateCharacterInst @0x817D40 -- factory. The null-character case builds a base
// "none" instance. FLAG: the console switches on the character type to build a
// typed AptCharacterInst subtype (sprite/button/text/...); those subtypes are a
// follow-on, so the base instance is used for every character for now -- the
// render item it owns is still type-correct (AptRTM_CreateItem -> the right
// AptRenderItem subtype), only the instance-side behaviour is base-only.
AptCharacterInst* AptCharacterInst::CreateCharacterInst(AptCharacter* pCharacter)
{
    AptCharacterInst* pInst =
        static_cast<AptCharacterInst*>(gpNonGCPoolManager->Allocate(sizeof(AptCharacterInst)));
    new (pInst) AptCharacterInst(pCharacter);
    return pInst;
}

// ---- render item ----------------------------------------------------------
AptRenderItem* AptCharacterInst::GetRenderItem() const { return mpRenderItem; }

AptRenderItem* AptCharacterInst::GetRenderItemWritable()
{
    AptRenderItem* pWritable =
        AptRTM_GetTickItemWritable(AptCurrentRenderTreeManager(), mpRenderItem, gnCurrUpdateTick);
    if (mpRenderItem != pWritable)
    {
        pWritable->AddReference();
        AptRenderItem* pOld = mpRenderItem;
        mpRenderItem = nullptr;
        pOld->ReleaseReference();
        mpRenderItem = pWritable;
    }
    return pWritable;
}

AptRenderItem* AptCharacterInst::SetRenderItem(AptRenderItem* pItem)
{
    if (mpRenderItem != pItem)
    {
        pItem->AddReference();
        AptRenderItem* pOld = mpRenderItem;
        mpRenderItem = nullptr;
        pOld->ReleaseReference();
        mpRenderItem = pItem;
    }
    return pItem;
}

AptCharacter* AptCharacterInst::SetCharacter(AptCharacter* pCharacter)
{
    return GetRenderItemWritable()->SetCharacter(pCharacter);
}

// ---- const reads (through the render item) --------------------------------
const AptCharacter* AptCharacterInst::GetCharacterConst() const { return mpRenderItem->GetCharacterConst(); }
int16_t AptCharacterInst::GetDepth() const     { return mpRenderItem->GetDepth(); }
int16_t AptCharacterInst::GetClipDepth() const { return mpRenderItem->GetClipDepth(); }
bool AptCharacterInst::GetIsVisible() const     { return mpRenderItem->GetIsVisible(); }
bool AptCharacterInst::GetIsMask() const        { return mpRenderItem->GetIsMask(); }
bool AptCharacterInst::GetHasMask() const       { return mpRenderItem->GetHasMask(); }
const AptMatrix* AptCharacterInst::GetPositionMatrixConst() const { return mpRenderItem->GetPositionMatrixConst(); }
const AptCXForm* AptCharacterInst::GetColorMatrixConst() const    { return mpRenderItem->GetColorMatrixConst(); }

// ---- writes (through the writable render item) ----------------------------
void AptCharacterInst::SetDepth(int nDepth)         { GetRenderItemWritable()->SetDepth(nDepth); }
void AptCharacterInst::SetClipDepth(int nClipDepth) { GetRenderItemWritable()->SetClipDepth(nClipDepth); }
void AptCharacterInst::SetIsVisible(bool bVisible)
{
    // FLAG: AptRenderItem::SetIsVisible (visibility propagation @0x7ED720) is a
    // follow-on; the writable item + the flag bit are set here.
    AptRenderItem* pItem = GetRenderItemWritable();
    if (bVisible) pItem->mFlags |= 0x80000000u;
    else          pItem->mFlags &= ~0x80000000u;
}
