// ===========================================================================
// EATech Apt -- AptRenderTreeManager factory + the scene-node render hooks.
// DECOMPILED from the PS3 EXTERNAL ELF.
//   AptRenderItem::Manager_CreateItem @0x814094 (the per-character-type factory).
//   AptRTM_CreateItem / AptRTM_GetTickItemWritable / AptCurrentRenderTreeManager
//   -- the helpers AptCharacterInst calls (homed here against the manager).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"
#include "SDKs/EATech/include/Apt/AptRenderItemMorph.h"
#include "SDKs/EATech/include/Apt/AptRenderItemStaticText.h"
#include "SDKs/EATech/include/Apt/AptRenderItemLevel.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCurrentRenderTreeManager / AptRTM_* decls
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h" // gpAptRenderManagerPool (off_8324D808)
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new

// ---------------------------------------------------------------------------
// Manager_CreateItem @0x814094 -- allocate the render-item subtype for a
// character's type (the Manager_CreateItem switch). Each case pool-allocates the
// sized subtype, whose ctor stamps the render-type flag + vtable; an unhandled
// type (and the deferred dynamic-text case) returns null, as the console does.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderItem::Manager_CreateItem(AptCharacter* pCharacter, int nTick)
{
    // Null character -> the stage "level" render item (the movie root).
    if (!pCharacter)
    {
        void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemLevel));
        return p ? new (p) AptRenderItemLevel(nullptr, nTick) : nullptr;
    }

    switch (pCharacter->mnType)
    {
        case 1:   // shape
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemShape));
            return p ? new (p) AptRenderItemShape(pCharacter, nTick) : nullptr;
        }
        case 5:   // sprite / movie clip
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemSprite));
            return p ? new (p) AptRenderItemSprite(pCharacter, nTick) : nullptr;
        }
        case 8:   // morph / tween
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemMorph));
            return p ? new (p) AptRenderItemMorph(pCharacter, nTick) : nullptr;
        }
        case 9:   // imported animation
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemAnimation));
            return p ? new (p) AptRenderItemAnimation(pCharacter, nTick) : nullptr;
        }
        case 10:  // static text
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemStaticText));
            return p ? new (p) AptRenderItemStaticText(pCharacter, nTick) : nullptr;
        }
        // case 2 (dynamic/edit text) -> AptRenderItemDynamicText: FLAG (deferred --
        // its 112-byte ctor needs the AptTextFormat layout, Wave 5). Until then a
        // dynamic-text character gets no render item (the console returns the real
        // subtype here).
        default:  // unhandled / not-yet-built character type -> no render item
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
// The render-tree-manager helpers AptCharacterInst calls (were FLAG'd externs).
// ---------------------------------------------------------------------------

// FLAG: the current target sim's render-tree manager (console gpCurrentTargetSim
// + 0x2C); wired by the AptTarget/AptInit startup. Null during bring-up, in which
// case AptCharacterInst creates no render item yet.
AptRenderTreeManager* AptCurrentRenderTreeManager()
{
    return 0;
}

AptRenderItem* AptRTM_CreateItem(AptRenderTreeManager* pMgr, AptCharacter* pCharacter, int nTick)
{
    return pMgr->Update_CreateItem(pCharacter, nTick);
}

AptRenderItem* AptRTM_GetTickItemWritable(AptRenderTreeManager* pMgr, const AptRenderItem* pItem, int nTick)
{
    return pMgr->Update_GetTickItemWritable(pItem, nTick);
}

// ===========================================================================
// AptRenderTreeManager::_AptRenderItemRootList -- the per-target render-root
// chain (one 8-byte pool cell per live root render item).
//
// Reconstructed STRICTLY from the X360 BURNOUT_X360_ARTIST.XEX pseudocode + asm
// of the three owned methods (no Feb-2007 source, no DWARF for this nested type):
//     ..._AptRenderItemRootList::Shutdown      @ 0x82AE19E0
//     ..._AptRenderItemRootList::Get           @ 0x82AE1A38
//     ..._AptRenderItemRootList::InsertNewRoot @ 0x82AE1AC8
//
// Each cell { AptRenderItem* mpItem; _AptRenderItemRootList* mpNext; } is pulled
// from the shared Apt fixed-size DOGMA pool (X360 off_8324D808 ==
// gpAptRenderManagerPool -- the same pool AptRenderManagerQueue / AptSharedPtr /
// the pseudo-display-list nodes draw from). The X360 reaches the pool through the
// module global off_8324D808; modelled here as the named extern shared with the
// sibling render-manager TUs (declared in AptRenderManagerItem.h).
// ===========================================================================

// ---------------------------------------------------------------------------
// Shutdown @ 0x82AE19E0   (static -- the head cell arrives directly in r3)
//
//   v1 = pHead
//   while (v1) {
//       item = v1->mpItem;            ; lwz r3, 0(r31)
//       next = v1->mpNext;            ; lwz r30, 4(r31)
//       v1->mpItem = nullptr;         ; stw r11(=0), 0(r31)
//       item->ReleaseReference();     ; bl AptRenderItem__ReleaseReference (r3 = item)
//       pool->Deallocate(v1, 8);      ; Deallocate(off_8324D808, r31, 8)
//       v1 = next;
//   }
//
// Frees the whole chain. The item ref is released (and the slot zeroed) BEFORE
// the cell is freed; the successor is read up front so the freed cell is never
// re-read. The trailing Deallocate result the X360 leaves in r3 is unused by any
// caller (AptRenderManagerItem::Clean / AptTarget::Shutdown) -> void.
// ---------------------------------------------------------------------------
void AptRenderTreeManager::_AptRenderItemRootList::Shutdown(_AptRenderItemRootList* pHead)
{
    for (_AptRenderItemRootList* lpCell = pHead; lpCell != nullptr; )
    {
        AptRenderItem*          lpItem = lpCell->mpItem;   // r3 = *node
        _AptRenderItemRootList* lpNext = lpCell->mpNext;   // r30 = node[1] (read next first)

        lpCell->mpItem = nullptr;                          // *node = 0
        if (lpItem)
            lpItem->ReleaseReference();                    // release (deletes at zero)

        gpAptRenderManagerPool->Deallocate(lpCell, sizeof(_AptRenderItemRootList));  // free 8-byte cell

        lpCell = lpNext;
    }
}

// ---------------------------------------------------------------------------
// Get @ 0x82AE1A38   (this = the anchor cell; r4 = nTick)
//
//   v7 = this->mpNext;                          ; lwz r31, 4(r3)
//   for (cur = this; v7; v7 = v7->mpNext) {
//       diff = nTick - v7->mpItem->mCreatedOnTick;        ; r11=*v7; r10=item[0x20]; subf.
//       expired = (diff >= 0) && !(v7->mpItem->mFlags & 0x02000000);  ; item[0x18] & bit25
//       if (!expired) break;
//       item = cur->mpItem;        cur->mpItem = nullptr; ; lwz r3,0(r30); stw 0,0(r30)
//       item->ReleaseReference();                         ; bl ...ReleaseReference
//       pool->Deallocate(cur, 8);                         ; Deallocate(off_8324D808, r30, 8)
//       cur = v7;
//   }
//   return cur;                                           ; mr r3, r30
//
// PRUNE: drop leading cells whose ROOT has expired -- expired == the root was
// created on/before nTick (nTick - mCreatedOnTick >= 0) AND it is not flagged the
// writable revision for this tick (mFlags bit 25, the same flag
// AptRenderItem::IsWritableForThisTick tests). Releases each pruned cell's item
// and frees the cell; returns the first surviving cell. (The cell whose item is
// released is the CURRENT cell, while the expiry test inspects the LOOK-AHEAD
// cell's item -- matching the asm's r30/r31 cursor pair exactly.)
//
// (The X360's `bl AptRenderItem__ReleaseReference` sets only r3 = the item; the
// trailing arg-register spills Hex-Rays renders as extra ReleaseReference args
// are not real parameters -- ReleaseReference is the const no-arg member.)
// ---------------------------------------------------------------------------
AptRenderTreeManager::_AptRenderItemRootList*
AptRenderTreeManager::_AptRenderItemRootList::Get(int nTick)
{
    _AptRenderItemRootList* lpCur       = this;
    _AptRenderItemRootList* lpLookAhead = mpNext;

    while (lpLookAhead != nullptr)
    {
        const AptRenderItem* lpItem = lpLookAhead->mpItem;
        // expired = created on/before nTick AND NOT flagged the writable revision
        // for this tick. asm @0x82AE1A64: subf./blt (age >= 0); @0x82AE1A70:
        // rlwinm. r11,r11,0,6,6 tests ONLY mFlags bit 25 (0x02000000). Do NOT use
        // IsWritableForThisTick here -- it also folds in (mCreatedOnTick == nTick),
        // which the prune predicate does not test (that would mis-keep a look-ahead
        // root created this tick with bit 25 clear).
        const bool lbExpired = (nTick - lpItem->GetCreatedOnTick()) >= 0
                            && (lpItem->mFlags & 0x02000000u) == 0;
        if (!lbExpired)
            break;

        AptRenderItem* lpRelease = lpCur->mpItem;   // release the CURRENT cell's item
        lpCur->mpItem = nullptr;
        if (lpRelease)
            lpRelease->ReleaseReference();

        gpAptRenderManagerPool->Deallocate(lpCur, sizeof(_AptRenderItemRootList));

        lpCur       = lpLookAhead;
        lpLookAhead = lpLookAhead->mpNext;
    }

    return lpCur;
}

// ---------------------------------------------------------------------------
// InsertNewRoot @ 0x82AE1AC8   (this = the anchor cell; r4 = pItem)
//
//   if (this->mpItem == pItem) return this;             ; lwz r11,0(r3); cmplw; beq
//   tail = this;                                        ; r31 = r3
//   for (n = this->mpNext; n; n = n->mpNext) tail = n;  ; walk to the last cell
//   cell = pool->Allocate(8);                           ; DOGMA_PoolManager::Allocate
//   if (cell) { cell->mpItem = 0; cell->mpNext = 0; }   ; zero both slots
//   pItem->AddReference();                              ; lwarx/stwcx. of item[0x24] (mRefCount)
//   cell->mpItem = pItem;                               ; stw r30, 0(r11)
//   cell->mpNext = nullptr;                             ; stw 0, 4(r11)
//   tail->mpNext = cell;                                ; stw r11, 4(r31)
//   return this;
//
// Append a fresh chain cell for pItem at the tail and take a reference on pItem.
// No-op (early return) when this anchor's FIRST slot already holds pItem -- the
// X360 only compares the head cell's item, not the whole chain. The inline
// lwarx/stwcx. increment of *(pItem+0x24) is AptRenderItem::mRefCount, i.e.
// AddReference(). The function returns the anchor cell (the sole caller
// AptRenderTreeManager::Update_SetRootItem uses r3 == the incoming anchor).
// ---------------------------------------------------------------------------
AptRenderTreeManager::_AptRenderItemRootList*
AptRenderTreeManager::_AptRenderItemRootList::InsertNewRoot(AptRenderItem* pItem)
{
    // No-op when the head cell already anchors this exact item.
    if (mpItem == pItem)
        return this;

    // Walk to the tail cell.
    _AptRenderItemRootList* lpTail = this;
    for (_AptRenderItemRootList* lpNode = mpNext; lpNode != nullptr; lpNode = lpNode->mpNext)
        lpTail = lpNode;

    // Allocate + zero a fresh cell from the shared Apt pool.
    _AptRenderItemRootList* lpCell =
        static_cast<_AptRenderItemRootList*>(gpAptRenderManagerPool->Allocate(sizeof(_AptRenderItemRootList)));
    if (lpCell)
    {
        lpCell->mpItem = nullptr;
        lpCell->mpNext = nullptr;
    }

    // Take a reference on the root (X360 inline lwarx/stwcx. of mRefCount).
    pItem->AddReference();

    // Fill the cell and link it at the tail.
    lpCell->mpItem = pItem;
    lpCell->mpNext = nullptr;
    lpTail->mpNext = lpCell;

    return this;
}
