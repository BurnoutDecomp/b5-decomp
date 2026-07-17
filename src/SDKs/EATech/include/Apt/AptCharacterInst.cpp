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
#include "SDKs/EATech/include/Apt/AptTarget.h"        // gpAptTarget (off_8324E574 -- the ctor's create-item guard)
#include "SDKs/EATech/include/Apt/AptDefine.h"        // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"   // the sprite/movie-clip subtype (type 5)
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"          // the dynamic-text subtype (type 2)
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"                 // gpAptPseudoDataPool (off_8324D808, the ctor's pool)

#include <new>   // placement new (factory)

// ctor @0x82AF7E70 (X360 ARTIST; PS3 @0x81431C)
//   *this = off_82145F80;                              // vtable (family models it as the
//                                                      //   manual mpVTable_unused; not written)
//   mpProperties = 0;                                  // a1[3] (this+0xC)
//   Item = 0;
//   if ( off_8324E574 )                                // if (gpAptTarget)  -- the create-item guard
//       Item = AptRenderItem::Manager_CreateItem(pCharacter, dword_8324E520);
//   mpRenderItem = Item;                               // a1[1] (this+0x4)
//   mTypeFlags = ((pCharacter ? pCharacter->mnType : 15) << 26) | (mTypeFlags & 0x3FFFFFF);
//   if ( Item ) Item->AddReference();                  // atomic ++Item[9] (render item ref count)
//
// FAITHFUL create-item dispatch (was an invented AptRTM_CreateItem null-manager fallback): the
// X360 ctor guards on gpAptTarget (off_8324E574 -- the current AS animation target) and calls the
// AptRenderItem::Manager_CreateItem FACTORY DIRECTLY. That factory needs no render-tree manager (it
// just DOGMA-pool-allocates the typed render item off the character), so there is no manager routing
// here at all; the render item is created iff an Apt target is active. gpAptTarget is live once the
// GUI Apt host runs AptCreateTargetInstance/AptChangeTargetInstance (BrnGuiAptRuntime), so every
// AptCharacterInst gets a real, type-correct render item carrying mpCharacter.
AptCharacterInst::AptCharacterInst(AptCharacter* pCharacter)
{
    mpProperties = nullptr;

    AptRenderItem* pItem = nullptr;
    if (gpAptTarget)   // off_8324E574 -- create the render item only when an Apt target is active
        pItem = AptRenderItem::Manager_CreateItem(pCharacter, gnCurrUpdateTick);
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

// CreateCharacterInst @0x82AFFF70 (X360; PS3 @0x817D40) -- factory. DECOMPILED FAITHFULLY:
// switch on the character type to build the correctly-SIZED + typed AptCharacterInst subtype.
//   type 5 (sprite/movie-clip): Allocate(36) + AptCharacterSpriteInstBase (owns mnGotoFrame /
//                               mnClipActionFlags / mpClipEventHandlers / mDisplayList / etc.)
//   type 2 (dynamic text): Allocate(32) + base AptCharacterInst
//   type 4 (button): return null (built elsewhere)
//   type 1/10/other/null: Allocate(16) + base AptCharacterInst
// This MUST allocate the sprite subtype for type 5: the placement path (instantiateCharacter /
// placeObject / _addToSetCaches / AptCIH::tick) reads/writes the sprite-base members at +0x10..
// +0x20 (mnGotoFrame / mDisplayList / ...) -- if a type-5 char got only the 16-byte base, those
// accesses fell OUTSIDE the allocation and CORRUPTED the heap (non-deterministic AVs during the
// first frame-0 place). Allocates from the DOGMA pool (off_8324D808 == gpAptPseudoDataPool), the
// console's pool. FLAG: the per-type console VTABLE (off_82145FE0 etc.) is the family's manual
// mpVTable_unused slot the reconstruction does not model (dispatch is via mTypeFlags), so it is
// not written -- the type tag (mTypeFlags high 6 bits) still identifies the type. Text/button
// subtype BEHAVIOUR beyond the base remains a follow-on; the render item stays type-correct.
AptCharacterInst* AptCharacterInst::CreateCharacterInst(AptCharacter* pCharacter)
{
    const int32_t nType = pCharacter ? pCharacter->mnType : -1;

    if (nType == 5)
    {
        // Sprite / movie-clip: the 36-byte AptCharacterSpriteInstBase (mDisplayList + play state).
        void* const pMem = gpAptPseudoDataPool->Allocate(sizeof(AptCharacterSpriteInstBase));   // 36
        if (pMem == nullptr)
            return nullptr;
        return ::new (pMem) AptCharacterSpriteInstBase(pCharacter);
    }
    if (nType == 4)
        return nullptr;   // button: built elsewhere (the console returns 0 here)

    if (nType == 2)
    {
        // Dynamic text: the AptCharacterTextInst -- the base + its 4 cached layout scalars
        // (mMaxScroll / mTextWidth / mTextHeight / mLength). Console 32 bytes (16 base + 16);
        // x64 48 (32 base + 16). The block MUST be the full text-inst size: the prior
        // max(32, sizeof(base)) under-sized it on x64 (32), and the text path's scalar writes
        // (EnsureStringAllocated's measure fold at inst +32..+47) landed past the block --
        // corrupting the adjacent DOGMA pool block (a live render item / CIH node) and
        // crashing the render walk.
        void* const pMem = gpAptPseudoDataPool->Allocate(sizeof(AptCharacterTextInst));
        if (pMem == nullptr)
            return nullptr;
        AptCharacterTextInst* const pTextInst =
            static_cast<AptCharacterTextInst*>(::new (pMem) AptCharacterInst(pCharacter));
        // Seed the cached layout scalars (the console text-inst ctor's seeds; the render
        // item's own ctor seeds its scroll to 1 the same way).
        pTextInst->mMaxScroll  = 1;
        pTextInst->mTextWidth  = 0.0f;
        pTextInst->mTextHeight = 0.0f;
        pTextInst->mLength     = 0.0f;
        return pTextInst;
    }

    // Every other type (1 / 10 / null / etc.) is the plain base.
    void* const pMem = gpAptPseudoDataPool->Allocate(sizeof(AptCharacterInst));
    if (pMem == nullptr)
        return nullptr;
    return ::new (pMem) AptCharacterInst(pCharacter);
}

// ---- render item ----------------------------------------------------------
AptRenderItem* AptCharacterInst::GetRenderItem() const { return mpRenderItem; }

AptRenderItem* AptCharacterInst::GetRenderItemWritable()
{
    AptRenderItem* pWritable =
        AptGetTickItemWritable(AptCurrentRenderTreeManager(), mpRenderItem, gnCurrUpdateTick);
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
    // NULL-SAFE (x64 bring-up): no writable render item when the render-tree manager is the null stub.
    AptRenderItem* pItem = GetRenderItemWritable();
    return pItem ? pItem->SetCharacter(pCharacter) : nullptr;
}

// ---- const reads (through the render item) --------------------------------
// NULL-SAFE (x64 bring-up): mpRenderItem is null when the render-tree manager is the FLAG'd null stub
// (no render item was created). The console always has a render item; on our partial bring-up these
// return safe defaults instead of dereferencing null. FLAG: remove the guards once the render-tree
// manager lands (every AptCharacterInst will then own a render item).
const AptCharacter* AptCharacterInst::GetCharacterConst() const { return mpRenderItem ? mpRenderItem->GetCharacterConst() : nullptr; }
int16_t AptCharacterInst::GetDepth() const     { return mpRenderItem ? mpRenderItem->GetDepth() : static_cast<int16_t>(0); }
int16_t AptCharacterInst::GetClipDepth() const { return mpRenderItem ? mpRenderItem->GetClipDepth() : static_cast<int16_t>(0); }
bool AptCharacterInst::GetIsVisible() const     { return mpRenderItem ? mpRenderItem->GetIsVisible() : false; }
bool AptCharacterInst::GetIsMask() const        { return mpRenderItem ? mpRenderItem->GetIsMask() : false; }
bool AptCharacterInst::GetHasMask() const       { return mpRenderItem ? mpRenderItem->GetHasMask() : false; }
const AptMatrix* AptCharacterInst::GetPositionMatrixConst() const { return mpRenderItem ? mpRenderItem->GetPositionMatrixConst() : nullptr; }
const AptCXForm* AptCharacterInst::GetColorMatrixConst() const    { return mpRenderItem->GetColorMatrixConst(); }

// ---- writes (through the writable render item) ----------------------------
// NULL-SAFE (x64 bring-up): GetRenderItemWritable() returns null when the render-tree manager is the
// null stub. The console always has a writable item; here we skip the write rather than deref null.
void AptCharacterInst::SetDepth(int nDepth)
{
    if (AptRenderItem* pItem = GetRenderItemWritable())
        pItem->SetDepth(nDepth);
}
void AptCharacterInst::SetClipDepth(int nClipDepth)
{
    if (AptRenderItem* pItem = GetRenderItemWritable())
        pItem->SetClipDepth(nClipDepth);
}
void AptCharacterInst::SetIsVisible(bool bVisible)
{
    // FLAG: AptRenderItem::SetIsVisible (visibility propagation @0x7ED720) is a
    // follow-on; the writable item + the flag bit are set here.
    AptRenderItem* pItem = GetRenderItemWritable();
    if (pItem == nullptr)
        return;
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
extern AptCIH* AptCloneManagedItem(AptRenderTreeManager* pMgr, AptCIH* pNode,
                                int nSourceArg, int nTick);
extern AptCIH* AptManagedItemMoved(AptRenderTreeManager* pMgr, AptCIH* pNode, int nTick);

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
// double-buffer clone is deferred -- AptCloneManagedItem is the manager facade.
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::CloneItem(AptCIH* pNode, int nArg)
{
    return AptCloneManagedItem(AptCurrentRenderTreeManager(), pNode, nArg, gnCurrUpdateTick);
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
        AptManagedItemMoved(pMgr, pNode, gnCurrUpdateTick);
    return pNode;
}

// ---------------------------------------------------------------------------
// ItemMoved @0x82AECD50
//   return AptRenderTreeManager::Update_ItemMoved(gpCurrentTargetSim[11], a1,
//                                                 dword_8324E520);
// ---------------------------------------------------------------------------
AptCIH* AptCharacterInst::ItemMoved(AptCIH* pNode)
{
    return AptManagedItemMoved(AptCurrentRenderTreeManager(), pNode, gnCurrUpdateTick);
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
    // NULL-SAFE (x64 bring-up): render items are null when the render-tree manager is the null stub.
    // The console always has both items; here we move the render data only when both exist, else just
    // transfer the property-hash ownership below. FLAG: render-data move resumes once the RTM lands.
    AptRenderItem* pDst = GetRenderItemWritable();
    if (pDst != nullptr && pSource->mpRenderItem != nullptr)
    {
        pDst->CopyRenderDataFrom(pSource->mpRenderItem);   // vtbl[+0x14]
        pDst->mDepth = pSource->mpRenderItem->mDepth;      // [c:0x14]
    }

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
