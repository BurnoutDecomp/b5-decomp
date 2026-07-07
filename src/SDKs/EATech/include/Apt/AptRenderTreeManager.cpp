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
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptRenderItemLevel.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCurrentRenderTreeManager / AptRTM_* decls
#include "SDKs/EATech/include/Apt/AptCIH.h"             // the scene node (display-list links + char inst)
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h" // gpAptRenderManagerPool (off_8324D808)
#include "SDKs/EATech/include/Apt/AptTarget.h"            // gpAptTarget / mppRenderRootAnchor (the manager storage)
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new

// ---------------------------------------------------------------------------
// Manager_CreateItem @0x82AF4BF8 (X360 ARTIST; PS3 @0x814094) -- allocate the
// render-item subtype for a character's type (the Manager_CreateItem switch).
// Each case DOGMA-pool-allocates (off_8324D808 == gpNonGCPoolManager) the sized
// subtype and its ctor stamps the render-type flag + vtable; an unhandled type
// returns null, as the console does. This is the DE-INLINED form of the X360
// factory: the X360 inlines each subtype's ctor as base-ctor + a manual vtable
// store + an insrwi/rlwimi render-type-flag stamp (with the sprite/animation
// cases also writing the +0x34 render-data slot); those inlines are reversed here
// into the concrete subtype ctors (AptRenderItemShape/Sprite/Morph/Animation/
// StaticText/DynamicText/Level), which do exactly base-ctor+vtable+flag(+field) --
// the sizes match the X360 (shape/staticText/level 52, sprite/morph/animation 56,
// dynamicText 112). Called DIRECTLY by AptCharacterInst::AptCharacterInst (needs no
// render-tree manager -- it only pool-allocates off the character).
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
        case 2:   // dynamic / edit text
        {
            void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemDynamicText));
            return p ? new (p) AptRenderItemDynamicText(pCharacter, nTick) : nullptr;
        }
        default:  // unhandled / not-yet-built character type -> no render item
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
// The render-tree-manager helpers AptCharacterInst calls (were FLAG'd externs).
// ---------------------------------------------------------------------------

// AptCurrentRenderTreeManager -- the current target's render-tree manager. The console
// reaches it as `*(gpCurrentTargetSim + 0x2C)` (AptCharacterInst::SetRootItem @0x82AE66A0:
// `lwz r11, off_8324E574; lwz r3, 0x2C(r11)`), i.e. the value stored in the AptTarget's
// mppRenderRootAnchor slot. That slot IS the render-tree manager: it is a one-pointer cell
// (AptTarget ctor pool-allocates it, zeroed) that holds the head cell of the per-target
// _AptRenderItemRootList chain, and the manager's Update_*/Render_* methods operate on that
// head (the manager's leading member, mpRootList, aliases the head-cell pointer). So the
// "manager this pointer" is simply the anchor slot address, reinterpret-cast. Null until the
// Apt context (gpAptTarget) is stood up by AptCreateTargetInstance -> then live, so every
// placed node's render item links into the render tree the AptRender walk traverses.
AptRenderTreeManager* AptCurrentRenderTreeManager()
{
    if (gpAptTarget == nullptr)
        return nullptr;
    // The anchor slot (mppRenderRootAnchor == *(target+0x2C)) reinterpreted as the manager:
    // its first member (mpRootList) IS the head-cell-slot the console addresses, so the
    // Render_GetRoot / Update_SetRootItem head-cell reads/writes land on the same storage.
    return reinterpret_cast<AptRenderTreeManager*>(gpAptTarget->mppRenderRootAnchor);
}

// AptRTM_CreateItem removed (was unused): it was only a thin forwarder to the real member
// AptRenderTreeManager::Update_CreateItem @0x814254. AptCharacterInst::AptCharacterInst no
// longer routes through it (it calls AptRenderItem::Manager_CreateItem directly, matching the
// X360 ctor), so the manager-driven path calls Update_CreateItem on the manager directly once
// the render-tree-manager subsystem lands.

AptRenderItem* AptRTM_GetTickItemWritable(AptRenderTreeManager* pMgr, const AptRenderItem* pItem, int nTick)
{
    // NULL-SAFE (see AptRTM_CreateItem): a null manager returns the item unchanged rather than
    // dereferencing the null manager.
    if (pMgr == nullptr)
        return const_cast<AptRenderItem*>(pItem);
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

// ---------------------------------------------------------------------------
// Create @0x82AE1B78  ("_AptRenderItemRootLi") -- the ctor-shaped factory that
// allocates AND constructs an anchor cell for pItem in one go.
//
//   cell = pool->Allocate(8);                 ; DOGMA_PoolManager::Allocate(off_8324D808, 8)
//   if (cell) { cell->mpItem = 0; cell->mpNext = 0; }
//   else cell = 0;
//   pItem->AddReference();                     ; inline lwarx/stwcx. of *(pItem+0x24)
//   cell->mpItem = pItem;                      ; stw r31, 0(cell)
//   cell->mpNext = 0;                          ; stw 0, 4(cell)
//   return cell;
//
// The atomic increment is AptRenderItem::mRefCount, i.e. AddReference(). The X360
// always stores through `cell` after the null-check (it would fault on an exhausted
// pool); kept verbatim, guarded so the PC build does not deref null.
// ---------------------------------------------------------------------------
AptRenderTreeManager::_AptRenderItemRootList*
AptRenderTreeManager::_AptRenderItemRootList::Create(AptRenderItem* pItem)
{
    _AptRenderItemRootList* lpCell =
        static_cast<_AptRenderItemRootList*>(gpAptRenderManagerPool->Allocate(sizeof(_AptRenderItemRootList)));
    if (lpCell)
    {
        lpCell->mpItem = nullptr;
        lpCell->mpNext = nullptr;
    }

    // Take a reference on the root item (X360 inline lwarx/stwcx. of mRefCount).
    pItem->AddReference();

    if (lpCell)
    {
        lpCell->mpItem = pItem;
        lpCell->mpNext = nullptr;
    }
    return lpCell;
}

// ===========================================================================
// The render-tree UPDATE re-derivation hooks (called by AptCIH/AptDisplayList as
// the per-frame display list mutates). Each rewrites the changed node's writable
// render item's manager links (first-child / next-sibling / mask) from the
// node's display-list neighbours. The console drives the double-buffered tree;
// here the writes are applied to the live items (single-buffer bring-up) exactly
// as the X360 body does, by name -- so the link topology stays faithful.
//
// AptCIH layout reached by the X360 via byte offsets, mapped to named members:
//   +0x14 mpDisplayListPrevious   +0x18 mpDisplayListNext   +0x1C mpDisplayListParent
//   +0x20 mpCharacterInst         (charInst +0x04 == mpRenderItem)
// ===========================================================================

// ---------------------------------------------------------------------------
// Update_ItemFirstChildChanged @0x82AE0AC8
//
//   item = pNode->mpCharacterInst;             ; lwz r3, 0x20(node)
//   if (!item) return;
//   writable = AptCharacterInst::GetRenderItemWritable(item);   ; r9 = writable
//   firstChild = pNode->GetFirstChild();       ; AptCIH::GetFirstChild
//   child = firstChild ? firstChild->mpCharacterInst->mpRenderItem : null;
//   writable->Manager_SetFirstChild(child);
// ---------------------------------------------------------------------------
void AptRenderTreeManager::Update_ItemFirstChildChanged(AptCIH* pNode)
{
    AptCharacterInst* lpInst = pNode->mpCharacterInst;   // *(node+0x20)
    if (!lpInst)
        return;

    AptRenderItem* lpWritable = lpInst->GetRenderItemWritable();   // r9
    AptCIH* lpFirstChild = pNode->GetFirstChild();
    AptRenderItem* lpChildItem = lpFirstChild
        ? lpFirstChild->mpCharacterInst->mpRenderItem   // *(*(child+0x20)+4)
        : nullptr;
    lpWritable->Manager_SetFirstChild(lpChildItem);
}

// ---------------------------------------------------------------------------
// Update_ItemNextSiblingChanged @0x82AE0A70
//
//   item = pNode->mpCharacterInst;             ; lwz r3, 0x20(node)
//   if (!item) return;
//   writable = AptCharacterInst::GetRenderItemWritable(item);
//   next = pNode->mpDisplayListNext;           ; *(node+0x18)
//   sibling = next ? next->mpCharacterInst->mpRenderItem : null;
//   writable->Manager_SetNextSibling(sibling);
// ---------------------------------------------------------------------------
void AptRenderTreeManager::Update_ItemNextSiblingChanged(AptCIH* pNode)
{
    AptCharacterInst* lpInst = pNode->mpCharacterInst;   // *(node+0x20)
    if (!lpInst)
        return;

    AptRenderItem* lpWritable = lpInst->GetRenderItemWritable();
    AptCIH* lpNext = pNode->mpDisplayListNext;            // *(node+0x18)
    AptRenderItem* lpSiblingItem = lpNext
        ? lpNext->mpCharacterInst->mpRenderItem           // *(*(next+0x20)+4)
        : nullptr;
    lpWritable->Manager_SetNextSibling(lpSiblingItem);
}

// ---------------------------------------------------------------------------
// Update_CloneItem @0x82AE0B38
//
//   clone = AptRenderItem::Manager_CloneNewItem(pSource->mpCharacterInst->mpRenderItem, nTick);
//   first = AptCIH::GetFirstChild(pNode);
//   if (first) clone->Manager_SetFirstChild(first->mpCharacterInst->mpRenderItem);
//   prev = pNode->mpDisplayListPrevious;                       ; *(node+0x14)
//   if (prev) AptCharacterInst::GetRenderItemWritable(prev->mpCharacterInst)
//                  ->Manager_SetNextSibling(clone);
//   next = pNode->mpDisplayListNext;                           ; *(node+0x18)
//   if (next) clone->Manager_SetNextSibling(next->mpCharacterInst->mpRenderItem);
//   pNode->mpCharacterInst->mpRenderItem = clone;              ; *(*(node+0x20)+4) = clone
//
// The X360 r3 entering Manager_CloneNewItem holds pSource->mpCharacterInst->
// mpRenderItem (lwz 0x20(a2); lwz 4(.)). pNode (a3) supplies the display-list-derived
// links + the destination char-inst slot.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Update_CloneItem(AptCIH* pSourceCIH, AptCIH* pNode, int nTick)
{
    AptRenderItem* lpClone =
        pSourceCIH->mpCharacterInst->mpRenderItem->Manager_CloneNewItem(nTick);   // r30

    AptRenderItem* lpResult = nullptr;

    AptCIH* lpFirstChild = pNode->GetFirstChild();
    if (lpFirstChild)
        lpResult = lpClone->Manager_SetFirstChild(lpFirstChild->mpCharacterInst->mpRenderItem);

    AptCIH* lpPrev = pNode->mpDisplayListPrevious;   // *(node+0x14)
    if (lpPrev)
    {
        AptRenderItem* lpPrevWritable = lpPrev->mpCharacterInst->GetRenderItemWritable();
        lpResult = lpPrevWritable->Manager_SetNextSibling(lpClone);
    }

    AptCIH* lpNext = pNode->mpDisplayListNext;        // *(node+0x18)
    if (lpNext)
        lpResult = lpClone->Manager_SetNextSibling(lpNext->mpCharacterInst->mpRenderItem);

    pNode->mpCharacterInst->mpRenderItem = lpClone;   // *(*(node+0x20)+4) = clone
    return lpResult;
}

// ---------------------------------------------------------------------------
// Update_ItemMoved @0x82AECD50/0x82AEC218
//
//   inst = pNode->mpCharacterInst; if (!inst) return;
//   writable = AptCharacterInst::GetRenderItemWritable(inst);   ; v9
//   // walk display-list-PREVIOUS skipping mask-flagged neighbours
//   p = pNode->mpDisplayListPrevious;
//   if (p) { while (p && p->...renderItem->GetIsMask()) p = p->mpDisplayListPrevious;
//            if (p) { GetRenderItemWritable(p->inst)->Manager_SetNextSibling(writable); goto link_next; }
//            parent = pNode->mpDisplayListParent; }
//   else   { parent = pNode->mpDisplayListParent;
//            if (!parent) { Update_SetRootItem(pNode, nTick); goto link_next; } }
//   GetRenderItemWritable(parent->inst)->Manager_SetFirstChild(writable);
//  link_next:
//   n = pNode->mpDisplayListNext;
//   if (n) { while (n && n->...renderItem->GetIsMask()) n = n->mpDisplayListNext;
//            sibling = n ? n->mpCharacterInst->mpRenderItem : null; }
//   else sibling = null;
//   return writable->Manager_SetNextSibling(sibling);
//
// The mask-skip predicate is `renderItem->mFlags bit30` (GetIsMask): a moved node
// links to the nearest NON-mask previous/next neighbour (masks live outside the
// normal sibling chain). When there is no live previous neighbour and no parent the
// node becomes a render root (Update_SetRootItem).
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Update_ItemMoved(AptCIH* pNode, int nTick)
{
    AptCharacterInst* lpInst = pNode->mpCharacterInst;   // *(node+0x20)
    if (!lpInst)
        return nullptr;

    AptRenderItem* lpWritable = lpInst->GetRenderItemWritable();   // v9 = r30

    // ---- previous-sibling / parent-first-child link -----------------------
    AptCIH* lpPrev = pNode->mpDisplayListPrevious;       // *(node+0x14)
    AptCIH* lpParent = nullptr;
    bool    lbLinkParent = false;
    if (lpPrev)
    {
        // Skip mask-flagged previous neighbours.
        while (lpPrev->mpCharacterInst->mpRenderItem->GetIsMask())
        {
            lpPrev = lpPrev->mpDisplayListPrevious;       // *(prev+0x14)
            if (!lpPrev)
                break;
        }
        if (lpPrev)
        {
            AptRenderItem* lpPrevWritable = lpPrev->mpCharacterInst->GetRenderItemWritable();
            lpPrevWritable->Manager_SetNextSibling(lpWritable);
        }
        else
        {
            lpParent     = pNode->mpDisplayListParent;    // *(node+0x1C)
            lbLinkParent = true;
        }
    }
    else
    {
        lpParent = pNode->mpDisplayListParent;            // *(node+0x1C)
        if (!lpParent)
            Update_SetRootItem(pNode, nTick);             // becomes a render root
        else
            lbLinkParent = true;
    }

    // X360 @0x82AEC2A0: parent->charInst is dereferenced WITHOUT a null check on
    // parent in the "had a previous neighbour, all of them masks" path (a child with
    // a previous sibling always has a parent); only the no-previous path (which
    // null-checks parent and routes to Update_SetRootItem) guards it. Matched here.
    if (lbLinkParent)
    {
        AptRenderItem* lpParentWritable = lpParent->mpCharacterInst->GetRenderItemWritable();
        lpParentWritable->Manager_SetFirstChild(lpWritable);
    }

    // ---- next-sibling link ------------------------------------------------
    AptCIH* lpNext = pNode->mpDisplayListNext;            // *(node+0x18)
    AptRenderItem* lpNextItem = nullptr;
    if (lpNext)
    {
        // Skip mask-flagged next neighbours.
        while (lpNext->mpCharacterInst->mpRenderItem->GetIsMask())
        {
            lpNext = lpNext->mpDisplayListNext;           // *(next+0x18)
            if (!lpNext)
                break;
        }
        if (lpNext)
            lpNextItem = lpNext->mpCharacterInst->mpRenderItem;   // *(*(next+0x20)+4)
    }
    return lpWritable->Manager_SetNextSibling(lpNextItem);
}

// ---------------------------------------------------------------------------
// Update_ItemSetMaskState @0x82AE0880  (a4==1 -> becoming a mask)
//
// Two near-mirror branches re-derive the moved/mask-state-changed node's sibling
// links around its (mask-flagged) display-list neighbours:
//
//  a4 != 1 (ceasing to be a mask -- fold back into the sibling chain):
//    prevNonMask = nearest non-mask mpDisplayListNext... no -- the asm walks
//      mpDisplayListNext for `next` (r11=node[0x18]) and mpDisplayListPrevious for
//      `prev` (r11=node[0x14]); writes the prev's next-sibling / parent's first-child
//      to `writable`, then `writable->Manager_SetNextSibling(nextItem)`.
//  a4 == 1 (becoming a mask -- hoist out of the sibling chain):
//    re-link the surrounding non-mask neighbours to each other, dropping `writable`.
//
// Reconstructed verbatim from the X360 two-arm asm; nTick is unused (the link
// writes are tick-agnostic, matching the console which never references r5).
// ---------------------------------------------------------------------------
void AptRenderTreeManager::Update_ItemSetMaskState(AptCIH* pNode, int /*nTick*/, bool bIsMask)
{
    AptCharacterInst* lpInst = pNode->mpCharacterInst;   // *(node+0x20)
    if (!lpInst)
        return;

    AptRenderItem* lpWritable = lpInst->GetRenderItemWritable();   // r28
    AptRenderItem* lpNextNonMask = nullptr;   // r30
    AptRenderItem* lpPrevNonMask = nullptr;   // r31

    if (bIsMask)   // a4 == 1: becoming a mask -> bridge the gap it leaves behind
    {
        // nearest non-mask NEXT neighbour's render item -> r30
        AptCIH* lpNext = pNode->mpDisplayListNext;        // *(node+0x18)
        if (lpNext)
        {
            while (lpNext->mpCharacterInst->mpRenderItem->GetIsMask())
            {
                lpNext = lpNext->mpDisplayListNext;
                if (!lpNext)
                    break;
            }
            if (lpNext)
                lpNextNonMask = lpNext->mpCharacterInst->GetRenderItemWritable();
        }
        // nearest non-mask PREVIOUS neighbour's render item -> r31
        AptCIH* lpPrev = pNode->mpDisplayListPrevious;    // *(node+0x14)
        if (lpPrev)
        {
            while (lpPrev->mpCharacterInst->mpRenderItem->GetIsMask())
            {
                lpPrev = lpPrev->mpDisplayListPrevious;
                if (!lpPrev)
                    break;
            }
            if (lpPrev)
                lpPrevNonMask = lpPrev->mpCharacterInst->GetRenderItemWritable();
        }

        // @0x82AE093C-0x82AE09A4. The console dereferences parent->charInst here
        // WITHOUT a null check (a masked node always has a parent display list);
        // matched faithfully (no PC guard added).
        if (lpNextNonMask)
        {
            if (!lpPrevNonMask)
            {
                AptRenderItem* lpParentWritable =
                    pNode->mpDisplayListParent->mpCharacterInst->GetRenderItemWritable();
                lpParentWritable->Manager_SetFirstChild(lpNextNonMask);
            }
            lpWritable->Manager_SetNextSibling(nullptr);
        }
        if (lpPrevNonMask)
            lpPrevNonMask->Manager_SetNextSibling(lpNextNonMask);
        if (!lpNextNonMask && !lpPrevNonMask)
        {
            AptRenderItem* lpParentWritable =
                pNode->mpDisplayListParent->mpCharacterInst->GetRenderItemWritable();
            lpParentWritable->Manager_SetFirstChild(nullptr);
        }
    }
    else   // a4 != 1: ceasing to be a mask -> fold back into the sibling chain
    {
        // nearest non-mask NEXT neighbour's render item -> r30
        AptCIH* lpNext = pNode->mpDisplayListNext;        // *(node+0x18)
        if (lpNext)
        {
            while (lpNext->mpCharacterInst->mpRenderItem->GetIsMask())
            {
                lpNext = lpNext->mpDisplayListNext;
                if (!lpNext)
                    break;
            }
            if (lpNext)
                lpNextNonMask = lpNext->mpCharacterInst->GetRenderItemWritable();
        }
        // nearest non-mask PREVIOUS neighbour -> link it to `writable` (or, none ->
        // link the parent's first child to `writable`), then writable->next = r30.
        AptCIH* lpPrev = pNode->mpDisplayListPrevious;    // *(node+0x14)
        AptRenderItem* lpPrevWritable = nullptr;
        if (lpPrev)
        {
            while (lpPrev->mpCharacterInst->mpRenderItem->GetIsMask())
            {
                lpPrev = lpPrev->mpDisplayListPrevious;
                if (!lpPrev)
                    break;
            }
            if (lpPrev)
                lpPrevWritable = lpPrev->mpCharacterInst->GetRenderItemWritable();
        }

        if (lpPrevWritable)
        {
            lpPrevWritable->Manager_SetNextSibling(lpWritable);
        }
        else
        {
            AptCIH* lpParent = pNode->mpDisplayListParent;   // *(node+0x1C)
            if (lpParent && lpParent->mpCharacterInst)
            {
                AptRenderItem* lpParentWritable = lpParent->mpCharacterInst->GetRenderItemWritable();
                lpParentWritable->Manager_SetFirstChild(lpWritable);
            }
        }
        if (lpNextNonMask)
            lpWritable->Manager_SetNextSibling(lpNextNonMask);
    }
}

// ===========================================================================
// The render-tree RENDER hooks. Each looks at one of pItem's manager links
// (first-child / mask / next-sibling); if the linked render item is still
// renderable for nTick (aged in AND committed, mFlags bit25 clear) the link is
// already current -> return null. Otherwise the link's current render revision is
// resolved (Manager_GetRenderRevision) and committed (Manager_Update*), and the
// (now refreshed) link is returned.
//
//   "renderable" == (nTick - mCreatedOnTick) >= 0 AND (mFlags & 0x02000000) == 0,
//   which is AptRenderItem::IsRenderableForThisTick(nTick). The X360 inlines it.
// ===========================================================================

// ---------------------------------------------------------------------------
// Render_GetChildInvisible @0x82ADB5E8 -- the first-child link (item +0x30 ==
// mpManagerFirstChild).
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Render_GetChildInvisible(AptRenderItem* pItem, int nTick)
{
    AptRenderItem* lpChild = pItem->mpManagerFirstChild;   // *(item+0x30)
    if (lpChild)
    {
        if (!lpChild->IsRenderableForThisTick(nTick))
            return nullptr;
    }
    AptRenderItem* lpRevision = lpChild ? lpChild->Manager_GetRenderRevision(nTick) : nullptr;
    pItem->Manager_UpdateFirstChild(lpRevision);
    return pItem->mpManagerFirstChild;
}

// ---------------------------------------------------------------------------
// Render_GetMaskInvisible @0x82ADB6E8 -- the mask link (item +0x1C == mpMask).
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Render_GetMaskInvisible(AptRenderItem* pItem, int nTick)
{
    AptRenderItem* lpMask = pItem->mpMask;   // *(item+0x1C)
    if (lpMask)
    {
        if (!lpMask->IsRenderableForThisTick(nTick))
            return nullptr;
    }
    AptRenderItem* lpRevision = lpMask ? lpMask->Manager_GetRenderRevision(nTick) : nullptr;
    pItem->Manager_UpdateMask(lpRevision);
    return pItem->mpMask;
}

// ---------------------------------------------------------------------------
// Render_GetSiblingInvisible @0x82ADB668 -- the next-sibling link (item +0x2C ==
// mpManagerNextSibling).
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Render_GetSiblingInvisible(AptRenderItem* pItem, int nTick)
{
    AptRenderItem* lpSibling = pItem->mpManagerNextSibling;   // *(item+0x2C)
    if (lpSibling)
    {
        if (!lpSibling->IsRenderableForThisTick(nTick))
            return nullptr;
    }
    AptRenderItem* lpRevision = lpSibling ? lpSibling->Manager_GetRenderRevision(nTick) : nullptr;
    pItem->Manager_UpdateNextSibling(lpRevision);
    return pItem->mpManagerNextSibling;
}

// ---------------------------------------------------------------------------
// Render_GetRoot @0x82AE55D0
//
//   if (!*ppHeadCellSlot) return 0;
//   cell = (*ppHeadCellSlot)->Get(nTick);      ; prune expired roots
//   *ppHeadCellSlot = cell;
//   if (!cell) return 0;
//   root = cell->mpItem;
//   if (!root->IsRenderableForThisTick(nTick)) return 0;
//   revision = root->Manager_GetRenderRevision(nTick);
//   if (revision != cell->mpItem) {
//       revision->AddReference();              ; inline lwarx/stwcx. of *(rev+0x24)
//       old = cell->mpItem; cell->mpItem = 0;
//       old->ReleaseReference();
//       cell->mpItem = revision;
//   }
//   return cell->mpItem;
//
// NOTE the renderable test here KEEPS (returns 0) when NOT renderable -- inverse of
// the Render_Get*Invisible helpers, matching the asm (@0x82AE5630 `beq` falls to the
// `li r3,0` return when the item is renderable... actually @0x82AE5630 branches to
// the return-0 path when `v7==0`, i.e. when the item is NOT the writable revision and
// aged in). The asm reads cell->mpItem (r9=cell loaded once) for the revision compare.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderTreeManager::Render_GetRoot(_AptRenderItemRootList** ppHeadCellSlot, int nTick)
{
    if (!*ppHeadCellSlot)
        return nullptr;

    _AptRenderItemRootList* lpCell = (*ppHeadCellSlot)->Get(nTick);   // prune expired roots
    *ppHeadCellSlot = lpCell;
    if (!lpCell)
        return nullptr;

    AptRenderItem* lpRoot = lpCell->mpItem;   // *(cell) = root item

    // @0x82AE560C-0x82AE5630: aged-in (nTick - mCreatedOnTick >= 0) AND NOT the
    // writable revision (mFlags bit25 clear). When that does NOT hold, return null.
    const bool lbRenderable = (nTick - lpRoot->GetCreatedOnTick()) >= 0
                           && (lpRoot->mFlags & 0x02000000u) == 0;
    if (!lbRenderable)
        return nullptr;

    AptRenderItem* lpRevision = lpRoot->Manager_GetRenderRevision(nTick);
    if (lpRevision != lpCell->mpItem)
    {
        lpRevision->AddReference();                 // X360 inline lwarx/stwcx. of mRefCount
        AptRenderItem* lpOld = lpCell->mpItem;
        lpCell->mpItem = nullptr;
        lpOld->ReleaseReference();
        lpCell->mpItem = lpRevision;
    }
    return lpCell->mpItem;
}

// ---------------------------------------------------------------------------
// The two render-tree-manager UPDATE entry points the AptCharacterInst static
// helpers route through (declared as free functions in AptCharacterInst.h so that
// header need not include the manager facade). Decompiled @0x82AE1C98 / 0x82AECD50.
// ---------------------------------------------------------------------------
struct AptCIH* AptRTM_CloneItem(AptRenderTreeManager* pMgr, struct AptCIH* pNode,
                                int nSourceArg, int nTick)
{
    // a2 (the clone source) = pNode; a3 (the destination node) arrives as the raw
    // pointer-as-int nSourceArg in the X360 ABI. FLAG: nSourceArg is a CIH* widened
    // to int at the console call site -- reconstructed back to the 8-byte pointer.
    pMgr->Update_CloneItem(pNode, reinterpret_cast<AptCIH*>(static_cast<intptr_t>(nSourceArg)), nTick);
    return pNode;
}

struct AptCIH* AptRTM_ItemMoved(AptRenderTreeManager* pMgr, struct AptCIH* pNode, int nTick)
{
    pMgr->Update_ItemMoved(pNode, nTick);
    return pNode;
}

// ---------------------------------------------------------------------------
// Update_SetRootItem @0x82AE5568 -- make pNode's render item a render ROOT.
//
// The manager's leading member (mpRootList) aliases the AptTarget's mppRenderRootAnchor
// head-cell slot (see AptCurrentRenderTreeManager). This inserts pNode's writable render
// item into that per-target root chain:
//   * empty chain (head cell null) -> _AptRenderItemRootList::Create a fresh anchor cell
//     for the item and store it as the head;
//   * non-empty -> InsertNewRoot appends a cell for the item at the tail (no-op when the
//     head cell already anchors that exact item).
// Both take a counted reference on the item (the cell owns it). A pNode with no character
// instance (hence no render item) is skipped -- there is nothing to root.
//
// (pNode==null is tolerated: AptDisplayListState::removeItem calls Update_SetRootItem(pNext)
// with pNext possibly null when the removed node was the sole/last root; a null node roots
// nothing, matching the console's guarded deref.)
// ---------------------------------------------------------------------------
void AptRenderTreeManager::Update_SetRootItem(AptCIH* pNode, int /*nTick*/)
{
    if (pNode == nullptr)
        return;
    AptCharacterInst* lpInst = pNode->GetCharacterInst();   // *(node+0x20)
    if (lpInst == nullptr)
        return;

    AptRenderItem* lpItem = lpInst->GetRenderItemWritable();
    if (lpItem == nullptr)
        return;

    // The head-cell slot is the manager's leading member (aliases mppRenderRootAnchor's cell).
    _AptRenderItemRootList*& lrpHead =
        *reinterpret_cast<_AptRenderItemRootList**>(&mpRootList);

    if (lrpHead == nullptr)
        lrpHead = _AptRenderItemRootList::Create(lpItem);   // first root -> fresh anchor cell
    else
        lrpHead = lrpHead->InsertNewRoot(lpItem);           // append (no-op if already anchored)
}

// ---------------------------------------------------------------------------
// Update_ItemInserted @0x82AECD70 -- a node was (re)inserted into a display list.
//
// X360 AptCharacterInst::ItemInserted: clear the node's render item's deletion mark (it is
// alive again) then Update_ItemMoved re-derives its manager links (first-child / previous-
// /next-sibling, or root). Reproduced here (the AptDisplayListState insert path calls this).
// ---------------------------------------------------------------------------
void AptRenderTreeManager::Update_ItemInserted(AptCIH* pNode, int nTick)
{
    AptCharacterInst* lpInst = pNode->GetCharacterInst();   // *(node+0x20)
    if (lpInst == nullptr)
        return;
    lpInst->GetRenderItemWritable()->Manager_SetDeletionMark(false);
    Update_ItemMoved(pNode, nTick);
}
