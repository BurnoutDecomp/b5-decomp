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
#include "SDKs/EATech/include/Apt/AptRenderItem.h"           // GetClipDepth (the per-node bounds gate)
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"          // AptRect accumulator
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // the parent's property hash (Lookup/Unset)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // the instance-name key
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // setGCRoot
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager::Allocate/Deallocate
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"           // gpAptPseudoDataPool (off_8324D808)
#include "SDKs/EATech/include/Apt/AptCharacter.h"            // mpFixupLink / mnType / mpAnimationFile (placed char binding)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // ExecuteInitActions
#include "SDKs/EATech/include/Apt/AptFile.h"                 // AptMovieData (the .apt root + import table)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // GetNewInsts / GetNewInstSize / DecNewInstSize
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"            // AptFilePtr (the placed char's animation-file bind)

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
// FLAG (un-homed AptCIH behavioural surface): the per-node tick / generalised-
// process hooks the display-list walks (AptCIH::tick @0x82B0BED8 / AptCIH::
// GeneralisedProcess @0x82AE0228). Declared as the same free-function shims the
// AptLinker TU uses (AptCIH_tick), so this TU compiles against the AptCIH
// behavioural cluster by name without redeclaring AptCIH's interface here. Each
// returns the OR-accumulated "did work" flag the X360 bodies return in r3.
// ---------------------------------------------------------------------------
int AptCIH_tick(AptCIH* pCIH);                       // AptCIH::tick
int AptCIH_GeneralisedProcess(AptCIH* pCIH, int a2); // AptCIH::GeneralisedProcess

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

// ---------------------------------------------------------------------------
// GetBoundingRect @0x82AD9B38 -- accumulate the world-space bounds of the placed
// nodes in this list. Walk the listed AptCIHs (mpHead->mpFirst, then each one's
// next-sibling link); for every node that is defined, owns a non-"level" character
// instance, and is not acting as a clip mask (its render item's clip depth still
// has its high bit set == the default unclipped sentinel), recurse into the node's
// AptCIH::GetBoundingRect to expand pAccumulator. Returns pAccumulator.
// ---------------------------------------------------------------------------
AptRect* AptDisplayList::GetBoundingRect(int nMode, const AptMatrix* pTransform, AptRect* pAccumulator)
{
    for (AptCIH* pNode = mpHead ? mpHead->mpFirst : nullptr; pNode; pNode = pNode->GetDisplayListNext())
    {
        // gate on the "real placed node" bit -- mnValueData bit 27 (a meValueType bit),
        // exactly as removeObject does. The asm (0x82AD9B5C extrwi r11,r11,1,4) reads bit
        // 27, NOT mbIsDefined (bit 4, what getIsDefined() reads) -- those select different
        // node sets, so getIsDefined here accumulated bounds over the wrong children.
        if (((pNode->mnValueData >> 27) & 1u) == 0u)
            continue;
        AptCharacterInst* pCharInst = pNode->GetCharacterInst();
        if (pCharInst != nullptr && pCharInst->GetTypeTag() != 15 &&
            static_cast<uint16_t>(pCharInst->GetRenderItem()->GetClipDepth()) >= 0x8000u)
        {
            pNode->GetBoundingRect(nMode, pTransform, pAccumulator);
        }
    }
    return pAccumulator;
}

// ---------------------------------------------------------------------------
// tick @0x82AD9BB8 -- advance every eligible placed node in this list one frame
// (the per-frame movie-clip update walk). For each listed AptCIH:
//   * gate: when bUseDepthLayerMask, only nodes whose render-item depth layer is
//     set in nDepthLayerMask ((1 << GetDepth()) & nDepthLayerMask); otherwise
//     skip nodes whose CIHState == 3 (mFlagsA bits 29-30 both set: a fully
//     removed / dead node);
//   * type gate: only sprite (5) / animation (9) / button (4) character instances
//     actually tick (those are the only AS-driven container/interactive types);
//   * accumulate each node's AptCIH::tick result into the OR'd return ("did any
//     node do work this frame").
// Walks the next-sibling display-list links (mpDisplayListNext). Returns the OR
// of every ticked node's result.
// ---------------------------------------------------------------------------
int AptDisplayList::tick(int nDepthLayerMask, uint8_t bUseDepthLayerMask)
{
    int nResult = 0;

    for (AptCIH* pNode = mpHead ? mpHead->mpFirst : nullptr; pNode; pNode = pNode->GetDisplayListNext())
    {
        if (bUseDepthLayerMask)
        {
            // (1 << charInst->renderItem->GetDepth()) & nDepthLayerMask -- the node's
            // depth used as a render-layer bit index; off this frame's mask -> skip.
            if (((1 << pNode->GetCharacterInst()->GetRenderItem()->GetDepth()) & nDepthLayerMask) == 0)
                continue;
        }
        // (mFlagsA bits 29-30) == CIHState 3 == a dead/removed node -> skip.
        else if ((pNode->mFlagsA & 0x60000000u) == 0x60000000u)
        {
            continue;
        }

        AptCharacterInst* pCharInst = pNode->GetCharacterInst();
        const uint32_t nTypeTag = pCharInst->GetTypeTag();
        bool bTick = (nTypeTag == 5 || nTypeTag == 9);
        // (charInst->mTypeFlags & 0xFC000000) == 0x10000000  <=>  GetTypeTag() == 4 (button).
        if (bTick || (pCharInst->mTypeFlags & 0xFC000000u) == 0x10000000u)
            nResult |= AptCIH_tick(pNode);   // FLAG: AptCIH::tick (un-homed behavioural cluster)
    }

    return nResult;
}

// ---------------------------------------------------------------------------
// GeneralisedProcess @0x82AE01B0 -- run the "generalised process" pass (the
// deferred AS-action / dirty-state flush) over every eligible placed node. For
// each listed AptCIH: when bUseDepthLayerMask, only process nodes whose render-
// item depth layer is set in nDepthLayerMask ((1 << GetDepth()) & mask); else
// process unconditionally. Accumulates each node's AptCIH::GeneralisedProcess
// result into the OR'd return. Walks the next-sibling links (mpDisplayListNext).
// ---------------------------------------------------------------------------
int AptDisplayList::GeneralisedProcess(int nFlags, int nDepthLayerMask, uint8_t bUseDepthLayerMask)
{
    int nResult = 0;

    for (AptCIH* pNode = mpHead ? mpHead->mpFirst : nullptr; pNode; pNode = pNode->GetDisplayListNext())
    {
        if (!bUseDepthLayerMask ||
            ((1 << pNode->GetCharacterInst()->GetRenderItem()->GetDepth()) & nDepthLayerMask) != 0)
        {
            nResult |= AptCIH_GeneralisedProcess(pNode, nFlags);   // FLAG: AptCIH::GeneralisedProcess (un-homed)
        }
    }

    return nResult;
}

// ---------------------------------------------------------------------------
// FLAG (un-homed callees, bodies their own TUs):
//   AptDL_FramePlacementDispatch (sub_82B0B080) -- the frame-placement dispatcher:
//     creates/re-uses the AptCIH for the placement and returns it. No standalone
//     export (its placement logic is folded inline in the X360); declared as the
//     callee whose result AddToDisplayList consumes.
//   AptCIH_DispatchInstantiatedHook -- the X360 calls the freshly-placed node's
//     vtable[0] with (node, &node->mInstanceName) right after pushing it to the
//     new-instance table (the per-node "just instantiated" hook); declared as a shim
//     so this TU compiles against that virtual without re-declaring the vtable.
// ---------------------------------------------------------------------------
extern AptCIH* AptDL_FramePlacementDispatch(AptDisplayList* pThis, void** ppPlacement, AptCIH* pParentNode);
extern void    AptCIH_DispatchInstantiatedHook(AptCIH* pPlacedNode);

// ---------------------------------------------------------------------------
// AddToDisplayList @0x82B0B150
//
// Run the placed character's frame init actions, bind its import file, dispatch the
// frame placement to get the placed AptCIH, register that node under its instance
// name in the parent's property hash, and add it to the target's "new instances to
// tick this frame" table (running each node's just-instantiated hook).
//
// ppPlacement is the .apt frame-placement command: ppPlacement[0] is the placement
// record (its +0xC dword is the placed character id), ppPlacement[1] is the
// AptCharacter being placed. The record + the movie import table are serialised .apt
// data (relocated in place), walked here by their fixed layout; the runtime objects
// (the owner character / movie / placed node) are reached by named members.
// ---------------------------------------------------------------------------
AptCIH* AptDisplayList::AddToDisplayList(AptNativeHash* pParentHash, void** ppPlacement, AptCIH* pParentNode)
{
    AptCharacterInst* const pInst = pParentNode->GetCharacterInst();
    AptCharacter* const pOwnerChar =
        const_cast<AptCharacter*>(pInst->GetRenderItem()->mpCharacter);

    // The owner's loaded .apt root (the AptCharacter fixup back-link points at the
    // AptMovieData) + the movie animation embedded at root+0x10.
    AptMovieData* const pMovie = reinterpret_cast<AptMovieData*>(pOwnerChar->mpFixupLink);
    AptCharacterAnimation* const pAnim =
        reinterpret_cast<AptCharacterAnimation*>(reinterpret_cast<char*>(pMovie) + 0x10);

    // The .apt placement command (serialised): record +0xC = placed char id.
    const int32_t nCharId = static_cast<const int32_t*>(ppPlacement[0])[3];
    AptCharacter* const pPlacedChar = static_cast<AptCharacter*>(ppPlacement[1]);

    pAnim->ExecuteInitActions(pParentNode, nCharId);

    // Bind the placed character's animation file: a non-animation character (type
    // tag != 9) with none yet takes the import-table entry matching its id, or the
    // owner movie's own file when there is no matching import.
    if (nCharId != -1 && pPlacedChar->mnType != 9 && pPlacedChar->mpAnimationFile == nullptr)
    {
        AptFilePtr* pSrc = reinterpret_cast<AptFilePtr*>(&pOwnerChar->mpAnimationFile);
        for (int32_t i = 0; i < pMovie->mnImportCount; ++i)
        {
            if (pMovie->mpImportTable[i].mnId == nCharId)
            {
                pSrc = reinterpret_cast<AptFilePtr*>(&pMovie->mpImportTable[i].mpFile);
                break;
            }
        }
        // mpAnimationFile is an AptFilePtr (a counted AptFile*); assign via the
        // shared-ptr operator= so the refcount is maintained (X360 AptFile_::operator=).
        *reinterpret_cast<AptFilePtr*>(&pPlacedChar->mpAnimationFile) = *pSrc;
    }

    AptCIH* const pPlaced = AptDL_FramePlacementDispatch(this, ppPlacement, pParentNode);

    // Register the placed node under its instance name (when it has one).
    if (!pPlaced->GetInstanceName().IsEmpty())
        pParentHash->Set(pPlaced->GetInstanceName(), static_cast<AptValue*>(pPlaced));

    // Add to the target's "new instances" table + run the per-node just-placed hook.
    void** const pNewInsts = static_cast<void**>(AptAnimationTarget::GetNewInsts());
    pNewInsts[AptAnimationTarget::GetNewInstSize()] = pPlaced;
    AptCIH_DispatchInstantiatedHook(pPlaced);
    AptAnimationTarget::DecNewInstSize();   // post-increments the new-instance count

    return pPlaced;
}
