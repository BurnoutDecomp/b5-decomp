// ===========================================================================
// EATech Apt -- AptDisplayList: the teardown / removal core of a movie clip's
// child display list.   Reconstructed from the X360 ARTIST.XEX pseudocode/asm
// (the authoritative spine; no Feb-2007 / DecFIGS body exists for this class):
//     AptDisplayList::AptDisplayList     @ 0x82AE4850  (pool-allocate the head node)
//     AptDisplayList::~AptDisplayList    @ 0x82AFE9F0  (clear + free the head node)
//     AptDisplayList::PreDestroy         @ 0x82AFD2B8  (clear + free + null the head)
//     AptDisplayList::clear              @ 0x82AFD1F8  (release every listed node)
//     AptDisplayList::removeObject       @ 0x82AFD0B0  (drop one node)
//     AptDisplayList::removeClonedObject @ 0x82AFD198  (drop a clone by depth)
//
// THE HEAD-NODE / STATE EQUIVALENCE.  AptDisplayList owns a single pointer
// (mpHead) to a small pool-allocated head node whose only field is an AptCIH*
// (the first listed scene node). That is exactly the layout of AptDisplayListState
// (a lone AptCIH* head, with the prev/next/parent list links living in the AptCIHs
// themselves). The X360 list-mutation paths confirm it: removeObject passes
// `*mpHead` (the head node) straight into AptDisplayListState::AddToDelayReleaseList
// as the `this`. So AsState() reinterprets the head node as the state object and
// the removal ops delegate to it -- a typed reinterpretation between two
// layout-identical 1-pointer SDK head structures, NOT an offset poke.
//
// The heavy behavioural surface (tick/GeneralisedProcess/GetBoundingRect/
// placeObject/instantiateCharacter/mergeState/AddToDisplayList/
// ReplaceDisplyListItem/placeObjectNCXForm/_addToSetCaches) is BLOCKED on
// un-homed AptCharacter frame/clip-event layout, module-static dispatch arrays,
// and several unnamed local subs -- see the note at the foot of AptDisplayList.h.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptDisplayList.h"
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"   // delegated list ops
#include "SDKs/EATech/include/Apt/AptCIH.h"                // the listed nodes + AddRef/Release/ClearCIH
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"      // GetCharacterInst()->GetDepth() (removeClonedObject)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // the parent's property hash (Lookup/Unset)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // the instance-name key
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // setGCRoot
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager::Allocate/Deallocate
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"           // gpAptPseudoDataPool (off_8324D808)

// ---------------------------------------------------------------------------
// FLAG (module-static, owned by the Apt GC layer, not yet homed): the X360 drains
// the deferred-release value vector (off_8324E51C / AptValueVector::ReleaseValues,
// guarded by the dword_8324E760 latch) every time a node is released during a
// clear. Encapsulated -- exactly as the AptActionInterpreter siblings do -- in
// AptApt_FlushDeferredReleases(); declared extern by name so this TU compiles
// against the same GC drain. (FLAG: off_8324E51C / dword_8324E760.)
// ---------------------------------------------------------------------------
extern void AptApt_FlushDeferredReleases();

// ---------------------------------------------------------------------------
// ctor @0x82AE4850
//   v2 = Allocate(off_8324D808, 4);  if (v2) { *v2 = 0; v3 = v2; } else v3 = 0;
//   *a1 = v3;
// Pool-allocate a single zeroed head node (its lone AptCIH* head nulled) and store
// it; mpHead stays null on allocation failure -- the X360 guards the store.
// ---------------------------------------------------------------------------
AptDisplayList::AptDisplayList()
{
    AptDisplayListNode* pHead = nullptr;
    if (void* pMem = gpAptPseudoDataPool->Allocate(sizeof(AptDisplayListNode)))   // 4 bytes
    {
        pHead = static_cast<AptDisplayListNode*>(pMem);
        pHead->mpFirst = nullptr;
    }
    mpHead = pHead;
}

// ---------------------------------------------------------------------------
// ~AptDisplayList @0x82AFE9F0
//   clear(0);  if (mpHead) Deallocate(off_8324D808, mpHead, 4);
// Release every listed node (NOT dropping their GC roots), then return the head
// node to the pool.
// ---------------------------------------------------------------------------
AptDisplayList::~AptDisplayList()
{
    clear(false);
    if (mpHead)
        gpAptPseudoDataPool->Deallocate(mpHead, sizeof(AptDisplayListNode));   // 4 bytes
}

// ---------------------------------------------------------------------------
// PreDestroy @0x82AFD2B8
//   if (mpHead) { clear(0); if (mpHead) Deallocate(off_8324D808, mpHead, 4);
//                 mpHead = 0; }
// The GC pre-destroy hook: tear down the list + free the head node and null it
// (so the later ~AptDisplayList is a no-op). Only acts when a head node exists.
// ---------------------------------------------------------------------------
void AptDisplayList::PreDestroy()
{
    if (!mpHead)
        return;

    clear(false);
    if (mpHead)
        gpAptPseudoDataPool->Deallocate(mpHead, sizeof(AptDisplayListNode));   // 4 bytes
    mpHead = nullptr;
}

// ---------------------------------------------------------------------------
// clear @0x82AFD1F8
//   if (mpHead && mpHead->mpFirst) {
//     for (node = mpHead->mpFirst; node; node = next) {
//       next = node->mpDisplayListNext;
//       node->AddRef();                 // (**node)(node)   vtbl[0]
//       removeObject(node);
//       if (bClearGCRoots) { node->setGCRoot(0); node->ClearCIH(true); }
//       AptApt_FlushDeferredReleases(); // off_8324E51C drain (guarded)
//       node->Release();                // (*(*node+4))(node) vtbl[1]
//     }
//   }
// Walk the listed AptCIH entries, releasing each. Each node is pinned (AddRef)
// across its removeObject so its destruction is deferred to the matching Release,
// after the GC deferred-release vector is drained. When bClearGCRoots, the node's
// GC-root count is zeroed and its character state is torn down (ClearCIH) first.
// ---------------------------------------------------------------------------
void AptDisplayList::clear(bool bClearGCRoots)
{
    if (!mpHead || !mpHead->mpFirst)
        return;

    for (AptCIH* pNode = mpHead->mpFirst; pNode; )
    {
        AptCIH* pNext = pNode->GetDisplayListNext();   // node->mpDisplayListNext (node[6])

        pNode->AddRef();          // pin the node across its removal
        removeObject(pNode);

        if (bClearGCRoots)
        {
            pNode->setGCRoot(0);
            pNode->ClearCIH(true);
        }

        AptApt_FlushDeferredReleases();   // drain the GC deferred-release vector

        pNode->Release();         // drop the pin (destroys the node at refcount 0)
        pNode = pNext;
    }
}

// ---------------------------------------------------------------------------
// The head node IS an AptDisplayListState (both: a lone AptCIH* head). @asm: the
// removal paths pass `*mpHead` straight in as the state `this`.
// ---------------------------------------------------------------------------
AptDisplayListState* AptDisplayList::AsState() const
{
    return reinterpret_cast<AptDisplayListState*>(mpHead);
}

// ---------------------------------------------------------------------------
// removeObject @0x82AFD0B0
//   if (pItem && ((pItem->mFlagsBitfield >> 27) & 1)) {   // a real placed node
//     parent = pItem->mpDisplayListParent;                // pItem[7]
//     if (parent) {
//       hash = parent->GetNativeHashVirtual();            // (*(*parent+8))(parent) vtbl[2]
//       if (!pItem->mInstanceName.IsEmpty() && hash &&
//           hash->Lookup(pItem->mInstanceName) == pItem)  // it is the named entry
//         hash->Unset(pItem->mInstanceName);
//     }
//     AsState()->AddToDelayReleaseList(pItem, true);
//   }
// Drop a placed node: if it is registered under its (non-empty) instance name in
// its parent's property hash, remove that mapping, then hand the node to the list
// state's delayed-release list. The "is placed" test is bit 27 of the node's
// AptValue bitfield word (pItem[1]); the parent property hash is reached through
// the parent CIH's GetNativeHashVirtual virtual (vtbl slot 2).
// ---------------------------------------------------------------------------
void AptDisplayList::removeObject(AptCIH* pItem)
{
    if (!pItem)
        return;

    // (pItem[1] >> 27) & 1 -- the "real placed node" flag in the AptValue bitfield.
    if (((pItem->mnValueData >> 27) & 1u) == 0u)
        return;

    if (AptCIH* pParent = pItem->GetDisplayListParent())   // pItem->mpDisplayListParent (pItem[7])
    {
        AptNativeHash* pHash = pParent->GetNativeHashVirtual();   // parent vtbl[2]
        const EAStringC& strName = pItem->GetInstanceName();      // pItem->mInstanceName (pItem[2])
        AptValue* pItemValue = pItem;   // AptCIH -> AptValue (single-inheritance upcast)
        if (!strName.IsEmpty() && pHash && pHash->Lookup(strName) == pItemValue)
            pHash->Unset(strName);
    }

    AsState()->AddToDelayReleaseList(pItem, true);
}

// ---------------------------------------------------------------------------
// removeClonedObject @0x82AFD198
//   depth = pSource->mpCharacterInst->mpRenderItem->mDepth;   // *(*(pSource+32)+4) +0x14
//   findInst(depth, 0, &scratch, &pMatch);                    // on this list's state
//   removeObject(pMatch);
// Locate the node currently at the depth of pSource (the clone's source) in this
// list and remove it. The depth is read through the source's character instance's
// render item (the named GetDepth() chain); the search reuses the state's findInst.
// ---------------------------------------------------------------------------
void AptDisplayList::removeClonedObject(AptCIH* pSource)
{
    const int nDepth = pSource->GetCharacterInst()->GetDepth();   // pSource[8]->mpRenderItem->mDepth

    AptCIH* pPrev  = nullptr;
    AptCIH* pMatch = nullptr;
    AsState()->findInst(nDepth, nullptr, &pPrev, &pMatch);
    removeObject(pMatch);
}
