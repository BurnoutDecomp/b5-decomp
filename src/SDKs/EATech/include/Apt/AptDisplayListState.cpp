// ===========================================================================
// EATech Apt -- AptDisplayListState.   DECOMPILED from the PS3 EXTERNAL ELF.
//   getLength 0x7E6890 / getValue 0x7E68B8 / insert 0x7FA508+0x7FA650 /
//   removeItem 0x7FA8C4 / remove 0x7FA9CC / findInst 0x7EE8A0 /
//   RegisterReferences 0x7E68FC.
//
// A depth-sorted doubly-linked list whose links live in the AptCIHs (the render
// spine's mpDisplayListPrevious/Next/Parent). The list holds one counted
// reference per node (AptValue AddRef on insert, Release on remove) and notifies
// the render-tree manager as it mutates (guarded -- the notifies are FLAG'd no-ops
// until a target sim/manager is wired; see AptRenderTreeManager.h).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptDisplayListState.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"      // AptCurrentRenderTreeManager
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // AddRef/Release

int AptDisplayListState::getLength() const
{
    int n = 0;
    for (AptCIH* p = mpFirst; p; p = p->GetDisplayListNext())
        ++n;
    return n;
}

AptCIH* AptDisplayListState::getValue(int nIndex) const
{
    AptCIH* p = mpFirst;
    for (int i = 0; p && i < nIndex; ++i)
        p = p->GetDisplayListNext();
    return p;
}

// insert pItem after pAfter @0x7FA508
AptCIH* AptDisplayListState::insert(AptCIH* pAfter, AptCIH* pItem)
{
    if (pAfter)
    {
        AptCIH* pNext = pAfter->GetDisplayListNext();
        pItem->SetDisplayListPrevious(pAfter);
        pItem->SetDisplayListNext(pNext);
        pAfter->SetDisplayListNext(pItem);
        if (pNext)
            pNext->SetDisplayListPrevious(pItem);
    }
    else
    {
        pItem->SetDisplayListPrevious(0);
        if (mpFirst)
        {
            pItem->SetDisplayListNext(mpFirst);
            mpFirst->SetDisplayListPrevious(pItem);
        }
        else
        {
            pItem->SetDisplayListNext(0);
        }
        mpFirst = pItem;
        if (pItem->GetDisplayListParent())
            if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
                pMgr->Update_ItemFirstChildChanged(pItem->GetDisplayListParent());
    }

    pItem->AddRef();   // the list holds a counted reference to each node

    if (pItem->GetCharacterInst())
        if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
            pMgr->Update_ItemInserted(pItem, 0);
    return pItem;
}

// insert pItem at depth nDepth @0x7FA650
AptCIH* AptDisplayListState::insert(int nDepth, AptCIH* pItem)
{
    AptCIH* pPrev = 0;
    AptCIH* pMatch = 0;
    findInst(nDepth, 0, &pPrev, &pMatch);
    AptCIH* pInserted = insert(pPrev, pItem);
    // Stamp the depth onto the node's (writable) render item.
    if (AptCharacterInst* pCI = pInserted->GetCharacterInst())
        pCI->SetDepth(nDepth);
    return pInserted;
}

// removeItem -- unlink pItem @0x7FA8C4
AptCIH* AptDisplayListState::removeItem(AptCIH* pItem)
{
    AptCIH* pPrev   = pItem->GetDisplayListPrevious();
    AptCIH* pNext   = pItem->GetDisplayListNext();
    AptCIH* pParent = pItem->GetDisplayListParent();

    if (pPrev)
        pPrev->SetDisplayListNext(pNext);
    if (pNext)
        pNext->SetDisplayListPrevious(pPrev);
    if (mpFirst == pItem)
        mpFirst = pNext;

    if (AptRenderTreeManager* pMgr = AptCurrentRenderTreeManager())
    {
        if (pPrev)        pMgr->Update_ItemNextSiblingChanged(pPrev);
        else if (pParent) pMgr->Update_ItemFirstChildChanged(pParent);
        else              pMgr->Update_SetRootItem(pNext, 0);
    }

    pItem->SetDisplayListNext(0);
    pItem->SetDisplayListPrevious(0);
    pItem->Release();   // balance the insert AddRef
    return pItem;
}

// remove @0x7FA9CC -- FLAG: the console re-finds the node's owning display-list
// state (via its parent) before removing; simplified to removing from `this`.
AptCIH* AptDisplayListState::remove(AptCIH* pItem)
{
    return removeItem(pItem);
}

// findInst @0x7EE8A0 -- by name, then by depth.
void AptDisplayListState::findInst(int nDepth, const EAStringC* pName,
                                   AptCIH** ppOutPrev, AptCIH** ppOutMatch) const
{
    AptCIH* pCur = mpFirst;
    if (!pCur)
    {
        *ppOutMatch = 0;
        *ppOutPrev  = 0;
        return;
    }

    if (pName)
    {
        AptCIH* pPrev = 0;
        for (AptCIH* p = mpFirst; p; pPrev = p, p = p->GetDisplayListNext())
        {
            // FLAG: the console also skips nodes flagged "pending remove"; the
            // name match is on the node's instance name.
            if (p->GetInstanceName() == *pName)
            {
                *ppOutMatch = p;
                *ppOutPrev  = pPrev;
                return;
            }
        }
        // not found by name -> fall through to the depth search
    }

    // Depth search: walk to the first node whose depth is >= nDepth.
    AptCIH* pPrev = 0;
    AptCIH* p = mpFirst;
    while (p && p->GetDepth() < nDepth)
    {
        pPrev = p;
        p = p->GetDisplayListNext();
    }
    *ppOutMatch = (p && p->GetDepth() == nDepth) ? p : 0;
    *ppOutPrev  = pPrev;
}

// RegisterReferences @0x7E68FC -- GC mark each listed node.
void AptDisplayListState::RegisterReferences(const AptValue* pOwner) const
{
    if (!AptValue::sReferenceRegistrationCb)
        return;
    for (AptCIH* p = mpFirst; p; p = p->GetDisplayListNext())
        AptValue::sReferenceRegistrationCb(pOwner, &p, "AptDisplayListState::DisplayListItem", 2);
}

// FLAG: the global current-update tick (X360 dword_8324E520), wired by the Apt
// update loop; the render-tree notifications stamp the revision being built.
extern int gnCurrUpdateTick;

// ---------------------------------------------------------------------------
// ChangeDepth @0x82AEE7E0 -- move pItem to a new depth in the depth-sorted list:
// unlink it, re-insert after the node that precedes nDepth (head if none), stamp
// the new depth onto the writable render item, and fire the render-tree notification
// for the slot it vacated. Returns pItem.
// (Render-tree facades are the homed no-op stubs on the single-buffer bring-up path.)
// ---------------------------------------------------------------------------
AptCIH* AptDisplayListState::ChangeDepth(int16_t nDepth, AptCIH* pItem)
{
    AptCIH* pOldPrevious = pItem->mpDisplayListPrevious;
    AptCIH* pOldParent   = pItem->mpDisplayListParent;
    AptCIH* pOldNext     = pItem->mpDisplayListNext;

    // Unlink pItem from its current spot.
    if (pOldPrevious)
        pOldPrevious->mpDisplayListNext = pOldNext;
    if (pOldNext)
        pOldNext->mpDisplayListPrevious = pOldPrevious;
    if (mpFirst == pItem)
        mpFirst = pOldNext;
    pItem->mpDisplayListPrevious = nullptr;
    pItem->mpDisplayListNext     = nullptr;

    // Find the node to insert after for nDepth (null => head).
    AptCIH* pInsertAfter = nullptr;
    AptCIH* pMatch       = nullptr;
    findInst(nDepth, nullptr, &pInsertAfter, &pMatch);

    AptRenderTreeManager* pManager = AptCurrentRenderTreeManager();

    if (pInsertAfter)
    {
        AptCIH* pNext = pInsertAfter->mpDisplayListNext;
        pItem->mpDisplayListPrevious    = pInsertAfter;
        pItem->mpDisplayListNext        = pNext;
        pInsertAfter->mpDisplayListNext = pItem;
        if (pNext)
            pNext->mpDisplayListPrevious = pItem;
    }
    else
    {
        // Head insert.
        pItem->mpDisplayListPrevious = nullptr;
        if (mpFirst)
        {
            pItem->mpDisplayListNext       = mpFirst;
            mpFirst->mpDisplayListPrevious = pItem;
        }
        else
        {
            pItem->mpDisplayListNext = nullptr;
        }
        mpFirst = pItem;
        if (pItem->mpDisplayListParent && pManager)
            pManager->Update_ItemFirstChildChanged(pItem->mpDisplayListParent);
    }

    // Notify the render tree about the slot the node left behind.
    if (pManager)
    {
        if (pOldPrevious)
            pManager->Update_ItemNextSiblingChanged(pOldPrevious);
        else if (pOldParent)
            pManager->Update_ItemFirstChildChanged(pOldParent);
        else
            pManager->Update_SetRootItem(pOldNext, gnCurrUpdateTick);
    }

    // Stamp the new depth onto the writable render item, re-announce the insert.
    pItem->mpCharacterInst->GetRenderItemWritable()->mDepth = nDepth;
    if (pItem->mpCharacterInst)
        AptCharacterInst::ItemInserted(pItem);

    return pItem;
}

// ---------------------------------------------------------------------------
// swapDepths @0x82AEE560 -- exchange two nodes' positions in the depth-sorted list
// AND their render-item depths, handling the adjacent and non-adjacent cases, then
// fire the first-child notification + re-announce both inserts. Returns void.
// ---------------------------------------------------------------------------
void AptDisplayListState::swapDepths(AptCIH* pA, AptCIH* pB)
{
    AptCIH* pANext = pA->mpDisplayListNext;
    AptCIH* pAPrev = pA->mpDisplayListPrevious;
    AptCIH* pBNext = pB->mpDisplayListNext;
    AptCIH* pBPrev = pB->mpDisplayListPrevious;

    if (pANext == pB)
    {
        // pA immediately before pB.
        pA->mpDisplayListNext = pBNext;
        if (pBNext)
            pBNext->mpDisplayListPrevious = pA;
        pB->mpDisplayListNext     = pA;
        pA->mpDisplayListPrevious = pB;
        pB->mpDisplayListPrevious = pAPrev;
        if (pAPrev)
            pAPrev->mpDisplayListNext = pB;
    }
    else if (pAPrev != pB)
    {
        // Non-adjacent: detach each and re-attach to the other's neighbours.
        pA->mpDisplayListNext = pBNext;
        if (pBNext)
            pBNext->mpDisplayListPrevious = pA;
        pA->mpDisplayListPrevious = pBPrev;
        if (pBPrev)
            pBPrev->mpDisplayListNext = pA;
        pB->mpDisplayListNext = pANext;
        if (pANext)
            pANext->mpDisplayListPrevious = pB;
        pB->mpDisplayListPrevious = pAPrev;
        if (pAPrev)
            pAPrev->mpDisplayListNext = pB;
    }
    else
    {
        // pB immediately before pA.
        pB->mpDisplayListNext = pANext;
        if (pANext)
            pANext->mpDisplayListPrevious = pB;
        pA->mpDisplayListNext     = pB;
        pB->mpDisplayListPrevious = pA;
        pA->mpDisplayListPrevious = pBPrev;
        if (pBPrev)
            pBPrev->mpDisplayListNext = pA;
    }

    // Exchange the two render-item depths (read both, then write).
    AptCharacterInst* pInstA = pA->mpCharacterInst;
    const int16_t nDepthA = pInstA->mpRenderItem->mDepth;
    const int16_t nDepthB = pB->mpCharacterInst->mpRenderItem->mDepth;
    pInstA->GetRenderItemWritable()->mDepth = nDepthB;
    pB->mpCharacterInst->GetRenderItemWritable()->mDepth = nDepthA;

    // If the head was one of them it becomes the other; either way the parent's
    // first child changed.
    AptRenderTreeManager* pManager = AptCurrentRenderTreeManager();
    if (mpFirst == pA)
    {
        mpFirst = pB;
        if (pManager)
            pManager->Update_ItemFirstChildChanged(pA->mpDisplayListParent);
    }
    else if (mpFirst == pB)
    {
        mpFirst = pA;
        if (pManager)
            pManager->Update_ItemFirstChildChanged(pA->mpDisplayListParent);
    }

    if (pA->mpCharacterInst)
        AptCharacterInst::ItemInserted(pA);
    if (pB->mpCharacterInst)
        AptCharacterInst::ItemInserted(pB);
}
