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
#include "SDKs/EATech/include/Apt/AptCIH.h"            // SetMaskedItem/ItemMoved take the scene node
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"  // AptCurrentRenderTreeManager + Update_*
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

// ===========================================================================
//  X360 ARTIST.XEX decompilations (faithful; named members + [c:0xNN] offsets).
// ===========================================================================

// ---------------------------------------------------------------------------
// Render-tree-manager UPDATE facades -- the REAL bodies are homed in
// AptRenderTreeManager.cpp (AptRTM_*). Declared extern here (this TU calls them);
// the earlier no-op stubs were removed to resolve the LNK2005 double-definition.
// ---------------------------------------------------------------------------
extern AptCIH* AptRTM_CloneItem(AptRenderTreeManager* pMgr, AptCIH* pNode,
                                int nSourceArg, int nTick);
extern AptCIH* AptRTM_ItemMoved(AptRenderTreeManager* pMgr, AptCIH* pNode, int nTick);

// The X360 render-tree helpers read the live manager through gpCurrentTargetSim's
// field [11] (console +0x2C) and the current update tick through dword_8324E520.
// Both are abstracted by AptCurrentRenderTreeManager()/gnCurrUpdateTick so the
// x64 layout stays correct (see AptCharacterInst.h).

// ---------------------------------------------------------------------------
// CloneItem @0x82AE1C98
//   return AptRenderTreeManager::Update_CloneItem(gpCurrentTargetSim[11], a1, a2,
//                                                 dword_8324E520);
// Tail-call into the manager's clone entry point; passes the scene node, the
// source argument (r4->r5), and the current update tick. FLAG: the live
// double-buffer clone is deferred -- AptRTM_CloneItem is the manager facade.
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::CloneItem(AptCIH* pNode, int nArg)
{
    return AptRTM_CloneItem(AptCurrentRenderTreeManager(), pNode, nArg, gnCurrUpdateTick);
}

// ---------------------------------------------------------------------------
// DestroyGCPointers @0x82AF5490
//   if ( mpRenderItem )                                              // [c:0x04]
//       GetRenderItemWritable()->Manager_SetDeletionMark(1);
//   if ( mpProperties )                                             // [c:0x0C]
//   {
//       mpProperties->DestroyGCPointers();
//       if ( mpProperties )
//           AptNativeHash::`scalar deleting destructor'(mpProperties, 1);
//       mpProperties = 0;
//   }
// The render item is marked for deletion (it is reference-counted; the manager
// frees it), then the per-instance property hash is torn down and freed back to
// the pool. The X360 scalar-deleting-destructor == ~AptNativeHash + sized
// Deallocate(this, sizeof(AptNativeHash)) from the non-GC pool.
// ---------------------------------------------------------------------------
void AptCharacterInst::DestroyGCPointers()
{
    if (mpRenderItem)
        GetRenderItemWritable()->Manager_SetDeletionMark(true);

    if (mpProperties)
    {
        mpProperties->DestroyGCPointers();
        if (mpProperties)
        {
            mpProperties->~AptNativeHash();
            gpNonGCPoolManager->Deallocate(mpProperties, sizeof(AptNativeHash));
        }
        mpProperties = nullptr;
    }
}

// ---------------------------------------------------------------------------
// GetCharacterWritable @0x82AE1CC0
//   return *(GetRenderItemWritable() + 4);     // render item [1] == mpCharacter
// ---------------------------------------------------------------------------
AptCharacter* AptCharacterInst::GetCharacterWritable()
{
    return GetRenderItemWritable()->mpCharacter;   // [c:0x04 of the render item]
}

// ---------------------------------------------------------------------------
// GetPositionMatrixWritable @0x82AE6650
//   return AptRenderItem::GetPositionMatrixWritable(GetRenderItemWritable());
// ---------------------------------------------------------------------------
AptMatrix* AptCharacterInst::GetPositionMatrixWritable()
{
    return GetRenderItemWritable()->GetPositionMatrixWritable();
}

// ---------------------------------------------------------------------------
// ItemFirstChildChanged @0x82AE1C78
//   return AptRenderTreeManager::Update_ItemFirstChildChanged(
//              gpCurrentTargetSim[11], a1, dword_8324E520);
// Tail-call notify the manager that the node's first child changed. (The X360
// passes the tick in r5; the manager facade's Update_ItemFirstChildChanged is
// the no-op single-buffer body.)
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::ItemFirstChildChanged(AptCIH* pNode)
{
    if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
        pMgr->Update_ItemFirstChildChanged(pNode);
    return pNode;
}

// ---------------------------------------------------------------------------
// ItemInserted @0x82AECD70
//   v2 = dword_8324E520;                                  // the update tick
//   v3 = gpCurrentTargetSim[11];                          // the manager
//   GetRenderItemWritable(a1->mpCharacterInst)->Manager_SetDeletionMark(0);
//   return AptRenderTreeManager::Update_ItemMoved(v3, a1, v2);
// Static: clears the node's char-inst render-item deletion mark (it is alive
// again), then notifies the manager the item moved/(re)inserted.
//   a1 + 32 (console +0x20) == AptCIH::mpCharacterInst [8].
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::ItemInserted(AptCIH* pNode)
{
    AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager();   // gpCurrentTargetSim[11]
    pNode->mpCharacterInst->GetRenderItemWritable()->Manager_SetDeletionMark(false);  // [c:0x20]
    if (pMgr)
        AptRTM_ItemMoved(pMgr, pNode, gnCurrUpdateTick);
    return pNode;
}

// ---------------------------------------------------------------------------
// ItemMoved @0x82AECD50
//   return AptRenderTreeManager::Update_ItemMoved(gpCurrentTargetSim[11], a1,
//                                                 dword_8324E520);
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::ItemMoved(AptCIH* pNode)
{
    return AptRTM_ItemMoved(AptCurrentRenderTreeManager(), pNode, gnCurrUpdateTick);
}

// ---------------------------------------------------------------------------
// MoveRenderDataFrom @0x82AF4630
//   GetRenderItemWritable(a1)->CopyRenderDataFrom(a2->mpRenderItem);  // vtbl[20]/0x14
//   v5 = a2->mpRenderItem->mDepth;                  // render item [c:0x14]
//   GetRenderItemWritable(a1)->mDepth = v5;
//   if ( a2->mpProperties )                         // [c:0x0C]
//   {
//       if ( a1->mpProperties )
//       {
//           a1->mpProperties->DestroyGCPointers();
//           if ( a1->mpProperties )
//               AptNativeHash::`scalar deleting destructor'(a1->mpProperties, 1);
//       }
//       a1->mpProperties = a2->mpProperties;
//       a2->mpProperties = 0;
//   }
// The X360 `(**(*item + 20))(item, src)` is the render item's vtbl slot at +0x14;
// for the base AptRenderItem that virtual is CopyRenderDataFrom. Then mDepth is
// copied straight across, and the property hash ownership is stolen from pSource
// (tearing down ours first if present).
// ---------------------------------------------------------------------------
void AptCharacterInst::MoveRenderDataFrom(AptCharacterInst* pSource)
{
    GetRenderItemWritable()->CopyRenderDataFrom(pSource->mpRenderItem);   // vtbl[+0x14]
    int16_t nDepth = pSource->mpRenderItem->mDepth;                       // [c:0x14]
    GetRenderItemWritable()->mDepth = nDepth;

    if (pSource->mpProperties)                                           // [c:0x0C]
    {
        if (mpProperties)
        {
            mpProperties->DestroyGCPointers();
            if (mpProperties)
            {
                mpProperties->~AptNativeHash();
                gpNonGCPoolManager->Deallocate(mpProperties, sizeof(AptNativeHash));
            }
        }
        mpProperties = pSource->mpProperties;
        pSource->mpProperties = nullptr;
    }
}

// ---------------------------------------------------------------------------
// SetHasMask @0x82AE1D50
//   return AptRenderItem::SetHasMask(GetRenderItemWritable(a1), a2, a3);
// ---------------------------------------------------------------------------
void AptCharacterInst::SetHasMask(bool bHasMask, AptRenderItem* pMask)
{
    GetRenderItemWritable()->SetHasMask(bHasMask, pMask);
}

// ---------------------------------------------------------------------------
// SetIsMask @0x82AECDC0
//   return AptRenderItem::SetIsMask(GetRenderItemWritable(a1), a2, a3);
// ---------------------------------------------------------------------------
void AptCharacterInst::SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix)
{
    GetRenderItemWritable()->SetIsMask(bIsMask, pMaskMatrix);
}

// ---------------------------------------------------------------------------
// SetMaskedItem @0x82AE1C50
//   return AptRenderTreeManager::Update_ItemSetMaskState(gpCurrentTargetSim[11],
//              a1, dword_8324E520, a2);
// Tail-call notify the manager of the node's mask state (the X360 marshals the
// node into r4, the tick into r5, the mask flag into r6).
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::SetMaskedItem(AptCIH* pNode, bool bIsMask)
{
    if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
        pMgr->Update_ItemSetMaskState(pNode, gnCurrUpdateTick, bIsMask);
    return pNode;
}

// ---------------------------------------------------------------------------
// SetRootItem @0x82AE66A0
//   return AptRenderTreeManager::Update_SetRootItem(gpCurrentTargetSim[11], a1,
//                                                   dword_8324E520);
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::SetRootItem(AptCIH* pNode)
{
    if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
        pMgr->Update_SetRootItem(pNode, gnCurrUpdateTick);
    return pNode;
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' @0x82AF7F28 -- compiler-generated thunk
// (~AptCharacterInst, then operator delete when the low bit of the flags arg is
// set). Intentionally NOT hand-written: the C++ compiler emits the deleting
// destructor for AptCharacterInst from the dtor above + the (pool-backed)
// operator delete, exactly as for the sibling instance types.
// ---------------------------------------------------------------------------
