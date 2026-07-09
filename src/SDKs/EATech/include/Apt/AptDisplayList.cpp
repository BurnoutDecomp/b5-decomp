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
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mnClipActionFlags / mpClipEventHandlers (_addToSetCaches)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // ExecuteInitActions
#include "SDKs/EATech/include/Apt/AptFile.h"                 // AptMovieData (the .apt root + import table)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // GetNewInsts / GetNewInstSize / DecNewInstSize / mInputSet
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"     // callFunction / the GC-root stacks (class binding)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"    // Push/PopStaticData (the register window)
#include "SDKs/EATech/include/Apt/AptPrototype.h"             // AptPrototype (the node prototype)
#include "SDKs/EATech/include/Apt/AptObject.h"                // AptValueWithHash (the live _global type)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"            // AptFilePtr (the placed char's animation-file bind)
#include "SDKs/EATech/include/Apt/AptTarget.h"               // gpAptTarget->GetAnimationTarget()
#include "SDKs/EATech/include/Apt/AptListenerSlotList.h"     // the clip-event set-cache add (_addToSetCaches)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"        // AptCXForm / AptUint32CXForm (ReplaceDisplyListItem merge)
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"        // AptMatrix (ReplaceDisplyListItem position merge)
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h" // authored text defaults (instantiateCharacter)
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"// dyn-text render-item seed (instantiateCharacter)
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"    // SetText (instantiateCharacter)
#include "SDKs/EATech/include/Apt/AptCharacterMorphInst.h"   // morph blend slot (placeObject)

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"    // gAptActionInterpreter.setVariable (AptCIH_CloneClassMembers / AssociateInstToClass)

#include <new>   // placement new (AptCIH::operator new + ctor for AptDLState_CreateInstAtDepth)

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
// AptCIH::tick is now called directly as a member (AptCIH.h is included below).
// AptCIH::GeneralisedProcess is now called directly as a member (AptCIH.h is included below).

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
            nResult |= pNode->tick();   // AptCIH::tick
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
            nResult |= static_cast<int>(pNode->GeneralisedProcess(
                           reinterpret_cast<AptCIH*>(static_cast<intptr_t>(nFlags)), nullptr));   // AptCIH::GeneralisedProcess
        }
    }

    return nResult;
}

// ---------------------------------------------------------------------------
// FLAG (un-homed callees, bodies their own TUs):
//   AptDL_FramePlacementDispatch (sub_82B0B080) -- the frame-placement dispatcher:
//     creates/re-uses the AptCIH for the placement and returns it. A REAL standalone
//     X360 function (AddToDisplayList @0x82B0B150 does `bl sub_82B0B080`) that has no
//     per-address dossier in the export set yet -- export + decompile it to convert
//     the role-reconstruction below into attestation. Declared as the callee whose
//     result AddToDisplayList consumes.
// The freshly-placed node's just-instantiated hook -- the X360 calls its vtable[0] with
// (node, &node->mInstanceName) right after pushing it to the new-instance table -- is
// AptValue::AddRef (vtbl[0], arg2 discarded), invoked directly at the AddToDisplayList call site.
// ---------------------------------------------------------------------------
extern AptCIH* AptDL_FramePlacementDispatch(AptDisplayList* pThis, void** ppPlacement, AptCIH* pParentNode);

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

    // NATIVE-8 (2026-07-02; the console read the embedded animation at root+0x10 and
    // the record id at dword[3] -- both the 4-byte layout): the fixup back-link's
    // embedded AptCharacterAnimation sits at +KU_AptEmbeddedMovieOff (0x20), the
    // record id at cmd+0x10 (the 8-aligned body's +0x08). Sanity-guarded like the
    // sibling PlaceCommand chain.
    AptCharacter* const pRootChar = pOwnerChar->mpFixupLink;
    AptCharacterAnimation* const pAnim = (pRootChar != nullptr)
        ? reinterpret_cast<AptCharacterAnimation*>(
              reinterpret_cast<char*>(pRootChar) + KU_AptEmbeddedMovieOff)
        : nullptr;

    // ppPlacement[0] = the serialised PlaceObject record (native-8: charId @ +0x10);
    // ppPlacement[1] = the placement properties (or the pseudo-snapshot pun) whose
    // mpCharacter is the character to place (null when the placement names none).
    const int32_t nCharId = static_cast<const int32_t*>(ppPlacement[0])[4];   // [c: dword 3]
    AptCharacter* const pPlacedChar =
        static_cast<AptFramePlacementProps*>(ppPlacement[1])->mpCharacter;

    bool bAnimSane = false;
    if (pAnim != nullptr)
    {
        const uintptr_t luTablePtr = reinterpret_cast<uintptr_t>(pAnim->mpCharacterTable);
        bAnimSane =
            (pAnim->mnCharacterCount > 0 && pAnim->mnCharacterCount <= 0x10000) &&
            (luTablePtr >= 0x10000u) && ((luTablePtr >> 47) == 0u);
    }

    // FLAG (import not loaded -- the deferred boundary, mirrored from the live place
    // path): a fresh ADD whose placement names NO character (an unresolved-import slot,
    // or a MOVE node whose target depth never composed because ITS import place was
    // skipped) cannot be instantiated -- the dispatch would create a CIH over a null
    // character (AV; reproduced on the drive bundles' menu state jumps). Skip the add.
    // Authored console data never hits this (imports are always resolved by Fixup
    // pass-3, the follow-on).
    if (pPlacedChar == nullptr)
        return nullptr;

    if (bAnimSane)
        pAnim->ExecuteInitActions(pParentNode, nCharId);

    // Bind the placed character's animation file: a non-animation character (type
    // tag != 9) with none yet takes the import-table entry matching its id, or the
    // owner movie's own file when there is no matching import.
    if (nCharId != -1 && pPlacedChar != nullptr && bAnimSane
        && pPlacedChar->mnType != 9 && pPlacedChar->mpAnimationFile == nullptr)
    {
        AptFilePtr* pSrc = reinterpret_cast<AptFilePtr*>(&pOwnerChar->mpAnimationFile);
        for (int32_t i = 0; i < pAnim->mnImportCount; ++i)
        {
            if (pAnim->mpImportTable[i].mnId == nCharId)
            {
                pSrc = reinterpret_cast<AptFilePtr*>(&pAnim->mpImportTable[i].mpFile);
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

    // Add to the target's "new instances" table + reference the placed node for it.
    void** const pNewInsts = static_cast<void**>(AptAnimationTarget::GetNewInsts());
    pNewInsts[AptAnimationTarget::GetNewInstSize()] = pPlaced;
    pPlaced->AddRef();   // console (***node)(node, &mInstanceName) == vtbl[0] AddRef (arg2 discarded)
    AptAnimationTarget::DecNewInstSize();   // post-increments the new-instance count

    return pPlaced;
}

// ---------------------------------------------------------------------------
// ReplaceDisplyListItem @0x82B0B2B8 -- reconcile the node already at a depth
// (pExisting) against a re-issued frame placement. When the placement names a new
// character (props->mpCharacter set): drop the existing node, bind the placed
// character's import file (the same AddToDisplayList rebind), and re-run
// AddToDisplayList. Otherwise keep the node and -- unless it has been ActionScript-
// changed -- merge the placement's colour transform and/or position matrix onto it.
// ---------------------------------------------------------------------------
AptCIH* AptDisplayList::ReplaceDisplyListItem(AptNativeHash* pParentHash, AptCIH* pExisting,
                                              void** ppPlacement, AptCIH* pParentNode)
{
    AptFramePlacementProps* const pProps = static_cast<AptFramePlacementProps*>(ppPlacement[1]);

    if (pProps->mpCharacter != nullptr)
    {
        // A new character is named: replace the existing node.
        removeObject(pExisting);

        const int32_t nCharId = static_cast<const int32_t*>(ppPlacement[0])[4];   // native-8 +0x10 [c: +0xC]
        if (nCharId != -1)
        {
            AptCharacter* const pPlacedChar = pProps->mpCharacter;
            AptCharacter* const pOwnerChar =
                const_cast<AptCharacter*>(pParentNode->GetCharacterInst()->GetRenderItem()->mpCharacter);
            AptCharacter* const pRootChar = pOwnerChar->mpFixupLink;
            AptCharacterAnimation* const pAnim = (pRootChar != nullptr)
                ? reinterpret_cast<AptCharacterAnimation*>(
                      reinterpret_cast<char*>(pRootChar) + KU_AptEmbeddedMovieOff)
                : nullptr;

            // Same import-file bind as AddToDisplayList (the X360 open-codes it here too).
            if (pAnim != nullptr && pPlacedChar->mnType != 9 && pPlacedChar->mpAnimationFile == nullptr)
            {
                AptFilePtr* pSrc = reinterpret_cast<AptFilePtr*>(&pOwnerChar->mpAnimationFile);
                for (int32_t i = 0; i < pAnim->mnImportCount; ++i)
                {
                    if (pAnim->mpImportTable[i].mnId == nCharId)
                    {
                        pSrc = reinterpret_cast<AptFilePtr*>(&pAnim->mpImportTable[i].mpFile);
                        break;
                    }
                }
                *reinterpret_cast<AptFilePtr*>(&pPlacedChar->mpAnimationFile) = *pSrc;
            }
        }
        return AddToDisplayList(pParentHash, ppPlacement, pParentNode);
    }

    // No new character: keep the existing node, merge its visual state. An AS write
    // (mFlagsA bit31, ASChanged) owns the transform -- never clobber it from a frame.
    if (pExisting->GetASChanged())
        return nullptr;

    if ((pProps->mnFlags & 0x8) != 0)   // bit3: copy colour transform
    {
        AptCXForm* const pColor =
            pExisting->GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
        pColor->AptUint32CXFormCopy(pProps->mpColorTransform);
    }
    if ((pProps->mnFlags & 0x4) != 0)   // bit2: copy position matrix
    {
        AptMatrix* const pPos =
            pExisting->GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        if (pProps->mpPositionMatrix != nullptr)
            pPos->AptMatrixCopy(reinterpret_cast<const AptMatrix*>(pProps->mpPositionMatrix));
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// FLAG (un-homed): the per-frame clip-event queue + the action-frame id.
//   AptCIH_queueClipEvents -- queue the given clip events on a node for a frame.
//   gnAptActionFrameId (dword_8324E514) -- the current AS action frame, homed in AptMovie.cpp.
// ---------------------------------------------------------------------------
// Canonical sig: the X360/PS3 AptCIH::queueClipEvents(int, unsigned int, int) -- the
// frame-id (a3) is UNSIGNED. Reconciled across the three call-site TUs (was int here).
// AptCIH::queueClipEvents is now called directly as a member (AptCIH.h is included above).
extern int gnAptActionFrameId;

// ---------------------------------------------------------------------------
// _addToSetCaches @0x82AF46B8  (STATIC -- r3 = the scene NODE, r4 = bRunLoad)
//
// For a sprite/movie-clip character (type 5) with registered clip-event handlers,
// fold each handler's event mask into the node's clip-action flags. If any handler
// is a load/unload handler, register the node in the director's clip-event set cache
// (idempotently). When bRunLoad, raise the load-in-progress flags, queue the load +
// init clip events for this frame, then drop the transient flags.
// ---------------------------------------------------------------------------
void AptDisplayList::_addToSetCaches(AptCIH* pNode, uint8_t bRunLoad)
{
    AptCharacterInst* const pInst = pNode->GetCharacterInst();

    // Only the Flash sprite / movie-clip character (type 5) carries clip handlers;
    // the button character (type 4) and everything else return early.
    if (pInst->GetRenderItem()->mpCharacter->mnType != 5)
        return;

    AptCharacterSpriteInstBase* const pSprite = static_cast<AptCharacterSpriteInstBase*>(pInst);
    AptClipEventHandlerList* const pHandlers = pSprite->mpClipEventHandlers;
    if (!pHandlers)
        return;

    // FLAG (converter data boundary -- the same 4-byte-straddle class as the malformed
    // frame tables / char[1]): a few clipActions blocks in the apt_convert-produced
    // bundle put the record-array pointer at the XB1-aligned +0x08 instead of +0x04,
    // so the +0x04 read straddles {pad, offset} (observed 0x0000A838_00000000). Such a
    // block was also skipped by resolve64 (its records/streams never relocated/parsed),
    // so it is unusable: skip it here (the fold + set-cache add + load events).
    {
        const uintptr_t luRecs = reinterpret_cast<uintptr_t>(pHandlers->mpHandlers);
        if (luRecs < 0x10000u || (luRecs >> 47) != 0u ||
            (luRecs & 0xFFFFFFFFull) == 0u)   // the straddle leaves the low dword zero
            return;
    }


    // Fold each registered handler's event mask into mnClipActionFlags' high 24 bits,
    // and note whether any handler registers a load/unload event.
    bool bHasLoadUnload = false;
    for (int32_t i = 0; i < pHandlers->mnCount; ++i)
    {
        const uint32_t nEventFlags = pHandlers->mpHandlers[i].mnEventFlags;
        if (nEventFlags & 0x201C7u)
        {
            pSprite->mnClipActionFlags |= (nEventFlags << 8);
            if (nEventFlags & 0x200C0u)
                bHasLoadUnload = true;
        }
    }

    // A node with load/unload handlers joins the director's clip-event set cache so
    // the per-frame load/unload pass can find it -- added once (idempotent).
    if (bHasLoadUnload)
    {
        AptAnimationTargetSet* const pSetCache = &gpAptTarget->GetAnimationTarget()->mInputSet;
        bool bAlreadyPresent = false;
        for (uint32_t s = 0; s < pSetCache->mnCapacity; ++s)
        {
            if (reinterpret_cast<AptCIH*>(pSetCache->mppSlots[s]) == pNode)
            {
                bAlreadyPresent = true;
                break;
            }
        }
        if (!bAlreadyPresent)
        {
            // Add the node to the set cache's first free slot (then AddRef it, the just-
            // placed hook). NOTE: this must operate on the AptAnimationTargetSet layout
            // ({mnCount@0 u16, mnCapacity@2 u16, mppSlots@4}), NOT via AptListenerSlotList
            // -- those are DIFFERENT reconstructed layouts (AptListenerSlotList puts
            // muCapacity@+8 / mppSlots@+0x10), so reinterpreting the set as an
            // AptListenerSlotList made add() read the capacity/slot pointer from the wrong
            // offsets and write out of bounds (heap corruption). The scan is the same
            // forward-from-head wrap the fixed-slot add does, on the set's own fields.
            const uint32_t luCap = pSetCache->mnCapacity;
            if (luCap != 0)
            {
                uint32_t luNext = (static_cast<uint32_t>(pSetCache->mnCount) + 1u) % luCap;
                uint32_t luScanned = 0u;
                while (luScanned < luCap && pSetCache->mppSlots[luNext] != nullptr)
                {
                    luNext = (luNext + 1u) % luCap;
                    ++luScanned;
                }
                if (pSetCache->mppSlots[luNext] == nullptr)   // found a free slot
                {
                    pSetCache->mnCount = static_cast<uint16_t>(luNext);
                    pSetCache->mppSlots[luNext] = static_cast<AptValue*>(pNode);
                    pNode->AddRef();   // vtable[0] (the just-placed hook)
                }
            }
        }
    }

    // On the placing frame, queue this node's load + init clip events.
    if (bRunLoad)
    {
        pSprite->mnClipActionFlags |= 0x4020000u;
                pNode->queueClipEvents(512,     gnAptActionFrameId, 1);
        pNode->queueClipEvents(0x40000, gnAptActionFrameId, 1);
        pSprite->mnClipActionFlags &= 0xFBFDFFFFu;
    }
}

// ---------------------------------------------------------------------------
// FLAG (un-homed leaf-first callees / globals; bodies in their own TUs):
//   AptDLState_CreateInstAtDepth (sub_82B008B0) -- create a fresh AptCIH(char,parent),
//     find its insert-after slot at nDepth, stamp the render-item depth, insert; returns it.
//   AptDLState_ReinsertInstAtDepth (sub_82AEE788) -- re-insert an existing node at nDepth.
//   AptCIH_CloneClassMembers -- copy every non-reserved (__proto__/prototype-skipped)
//     member of an AS class object onto a freshly placed instance (AptActionInterpreter::
//     setVariable over the class hash); reached only with a non-null class object.
//   AptCIH_AssociateInstToClass (@0x82B073B8) -- register the placed instance with its class.
// ---------------------------------------------------------------------------
extern AptCIH* AptDLState_CreateInstAtDepth(AptDisplayListState* pState, int nDepth,
                                            AptCharacter* pCharacter, AptCIH* pParentNode);
extern AptCIH* AptDLState_ReinsertInstAtDepth(AptDisplayListState* pState, int nDepth, AptCIH* pNode);
extern void    AptCIH_CloneClassMembers(AptCIH* pNode, AptValue* pClassObject);
// AptCIH::AssociateInstToClass is now a member of AptCIH (declared in AptCIH.h).

// The class-binding tail's collaborators (see AptCIH_AssociateInstToClass below).
extern AptNativeHash* gpAptClassRegistry;             // dword_8324E2D4 (AptObject.cpp)
extern AptValueWithHash* gpAptGlobalFallback;         // off_8324E380 (the live _global)
extern const EAStringC gAptSpriteClassKey;            // dword_8324E640 "MovieClip"
extern AptActionInterpreter gAptActionInterpreter;    // &dword_8324E760

// ---------------------------------------------------------------------------
// instantiateCharacter @0x82B061D0 -- find-or-create the placed node at a depth and
// (re)bind it to a character. (Signature is the 8-reg + 1-stack-out asm prologue, not
// the Hex-Rays vararg over-count.)
// ---------------------------------------------------------------------------
AptRenderItem* AptDisplayList::instantiateCharacter(int nDepth, AptCharacter* pCharacter,
                                                    const EAStringC* pName, AptCIH* pParentNode,
                                                    int bForceRemove, int16_t nClipDepth,
                                                    AptCIH** ppOutNode, int* pbOutCreatedNew)
{
    AptCIH* pNode = nullptr;

    // Locate the node currently at this depth (findInst fills prev + match through the
    // head node reinterpreted as the list state).
    AptCIH* pPrev  = nullptr;
    AptCIH* pMatch = nullptr;
    AsState()->findInst(nDepth, pName, &pPrev, &pMatch);

    // bCreatedNew is false ONLY when an already-placed node is reused verbatim.
    bool bCreatedNew = true;
    if (pMatch != nullptr)
    {
        if (bForceRemove)
        {
            removeObject(pMatch);
        }
        else if (((pMatch->mnValueData >> 27) & 1u) != 0u)
        {
            pNode = pMatch;          // a real placed node -- reuse unchanged
            bCreatedNew = false;
        }
        else if (pName != nullptr && *pName == pMatch->GetInstanceName())
        {
            pMatch->setIsDefined(true);   // empty placeholder with our name -- reuse as slot
            pNode = pMatch;
        }
    }

    if (bCreatedNew)
    {
        if (pNode != nullptr)
        {
            // Reusing a placeholder: re-insert it at nDepth if it has drifted.
            if (nDepth != pNode->GetCharacterInst()->GetRenderItem()->GetDepth())
            {
                AsState()->remove(pNode);
                AptDLState_ReinsertInstAtDepth(AsState(), nDepth, pNode);
                pNode->Release();   // vtbl[1] -- drop the old list slot's reference
            }
            pNode->SetCharacterInst(AptCharacterInst::CreateCharacterInst(pCharacter), true);
        }
        else
        {
            pNode = AptDLState_CreateInstAtDepth(AsState(), nDepth, pCharacter, pParentNode);
        }

        // Created-on-frame: inherit the parent sprite/animation's current goto-frame.
        AptCharacterInst* const pParentInst = pParentNode->GetCharacterInst();
        const uint32_t nParentTag = pParentInst->GetTypeTag();
        if (nParentTag == 5 || nParentTag == 9)
            pNode->SetCreatedOnFrame(static_cast<AptCharacterSpriteInstBase*>(pParentInst)->mnGotoFrame);
        else
            pNode->SetCreatedOnFrame(0x3FFF);

        // Register the node under its instance name in the parent's property hash, unless
        // empty or it already maps to a live CIH.
        if (pName != nullptr)
        {
            pNode->SetInstanceName(*pName);
            if (!pName->IsEmpty())
            {
                AptNativeHash* const pParentHash = pParentInst->mpProperties;
                AptValue* const pHit = pParentHash->Lookup(*pName);
                bool bHitIsCIH = false;
                if (pHit != nullptr)
                {
                    const AptVirtualFunctionTable_Indices eType = pHit->getVtblIndex();
                    bHitIsCIH = (eType == AptVFT_CharacterInstHandle || eType == AptVFT_CIHNone);
                }
                if (!bHitIsCIH)
                    pParentHash->Set(*pName, static_cast<AptValue*>(pNode));
            }
        }

        // Per character-type init.
        AptCharacterInst* const pInst = pNode->GetCharacterInst();
        const uint32_t nTypeTag = pInst->GetTypeTag();
        if (nTypeTag == 5 || nTypeTag == 9 || nTypeTag == 4)
        {
            void** const pNewInsts = static_cast<void**>(AptAnimationTarget::GetNewInsts());
            pNewInsts[AptAnimationTarget::GetNewInstSize()] = pNode;
            AptAnimationTarget::DecNewInstSize();   // post-increments the count
            pNode->AddRef();                        // vtbl[0]
        }
        else if (nTypeTag == 2)
        {
            // Dynamic text: seed the render item from the authored character defaults.
            const AptCharacterDynamicText* const pAuthored =
                static_cast<const AptCharacterDynamicText*>(pInst->GetRenderItem()->mpCharacter);
            AptRenderItemDynamicText* const pText =
                static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());

            const char* const pTextDefault = pAuthored->mpDefaultText;
            pText->mTextValue = EAStringC(pTextDefault ? pTextDefault : "");
            // szVariable is an 8-byte char* on native-8 (wiki AptCharacterText.szVariable), read
            // directly -- NOT reinterpret_cast'd from a 4-byte int, which truncated the relocated
            // pointer and AV'd in InitFromBuffer. Matches the X360 `*(char+0x40)` pointer read.
            const char* const pVarDefault = pAuthored->mpVariableName;
            pText->mVarValue = EAStringC(pVarDefault ? pVarDefault : "");

            pText->SetAlignment(pAuthored->mnAuthoredReserved0);
            pText->SetBoxAlignment(3);
            pText->SetMultiline(pAuthored->mnAuthoredReserved3 > 0);
            pText->SetWordWrap(pAuthored->mnAuthoredReserved4 > 0);

            static_cast<AptCharacterTextInst*>(pInst)->SetText(pParentNode);

            pText->ClearStateFlags(1u);   // mark resolved
            pText->SetStateFlags(6u);     // dirty for layout
        }
    }

    if (bCreatedNew)
    {
        AptCharacterInst* const pInst = pNode->GetCharacterInst();
        if (pInst->GetRenderItem()->mpCharacter != pCharacter)
            pInst->GetRenderItemWritable()->SetCharacter(pCharacter);
    }
    else
    {
        // Reused existing placed node: rebind its parent (ref-counted swap) + character.
        if (pParentNode != pNode->GetDisplayListParent())
        {
            pParentNode->AddRef();   // vtbl[0]
            if (AptCIH* const pOldParent = pNode->GetDisplayListParent())
                pOldParent->Release();   // vtbl[1]
            pNode->SetDisplayListParent(pParentNode);
        }
        if (pNode->GetCharacterInst()->GetRenderItem()->mpCharacter != pCharacter)
            pNode->SetCharacterInst(AptCharacterInst::CreateCharacterInst(pCharacter), true);
    }

    AptRenderItem* const pResult = pNode->GetCharacterInst()->GetRenderItemWritable();
    pResult->mClipDepth = nClipDepth;
    *ppOutNode = pNode;
    *pbOutCreatedNew = bCreatedNew ? 1 : 0;
    return pResult;
}

// ---------------------------------------------------------------------------
// placeObject @0x82B097D8 -- place a character at a depth. (Signature/arg order from
// the asm prologue: a2=existing node, a3=depth, a4=character, a5=name, a6=parent node
// [also the dirtied node], a7=force-remove, a8=clip depth, f1=frame value; stack:
// colour CXForm, position matrix, placement field, AS class object.)
// ---------------------------------------------------------------------------
AptCIH* AptDisplayList::placeObject(AptCIH* pExistingNode, int nDepth, AptCharacter* pCharacter,
                                    const EAStringC* pName, AptCIH* pParentNode, int bForceRemove,
                                    int16_t nClipDepth, double fFrameValue, const AptCXForm* pColorXForm,
                                    const float* pPositionMatrix, const void* pPlacementClipActions /*console u32 field18*/,
                                    AptValue* pClassObject)
{
    AptCIH* pNode = pExistingNode;

    int bWasInstantiated = 0;
    if (pNode == nullptr)
    {
        AptCIH* pCreated = nullptr;
        instantiateCharacter(nDepth, pCharacter, pName, pParentNode, bForceRemove, nClipDepth,
                             &pCreated, &bWasInstantiated);
        pNode = pCreated;
    }

    // Mark the parent (the dirtied node) -- the callee takes only (this, bDirty).
    pParentNode->SetGeneralizedProcessDirtyState(true);

    if (pNode == nullptr)
        return pNode;

    AptCharacterInst* const pInst = pNode->GetCharacterInst();

    // Copy the supplied colour transform into the placed render item.
    if (pColorXForm != nullptr)
        pInst->GetRenderItemWritable()->GetColorMatrixWritable()->AptCXFormCopy(pColorXForm);

    // Copy the supplied position matrix (6-float affine) into the placed render item.
    if (pPositionMatrix != nullptr)
        pInst->GetRenderItemWritable()->GetPositionMatrixWritable()
            ->AptMatrixCopy(reinterpret_cast<const AptMatrix*>(pPositionMatrix));

    // Stamp the placement field at the char inst's +0x18 subclass slot when supplied
    // (the placement's clipActions/handler-list pointer; pointer-width on x64).
    if (pPlacementClipActions != nullptr)
        static_cast<AptCharacterSpriteInstBase*>(pInst)->mpClipEventHandlers =
            static_cast<AptClipEventHandlerList*>(const_cast<void*>(pPlacementClipActions));   // console stw 0x18(charInst)

    // A morph instance (type tag 8) takes the AS frame value as its blend amount.
    if ((pInst->mTypeFlags & 0xFC000000u) == 0x20000000u)
        *reinterpret_cast<float*>(&static_cast<AptCharacterMorphInst*>(pInst)->mMorphState_unknown) =
            static_cast<float>(fFrameValue);

    // The class-binding tail runs only for a freshly instantiated node.
    if (bWasInstantiated == 0)
        return pNode;

    AptDisplayList::_addToSetCaches(pNode, 1);

    const uint32_t nTypeTag = pNode->GetCharacterInst()->GetTypeTag();
    if (nTypeTag == 5 || nTypeTag == 9)
    {
        // When the supplied AS class object carries a class (mnValueData bit 27), clone
        // its non-reserved members onto the freshly placed instance.
        if (pClassObject != nullptr && ((pClassObject->mnValueData >> 27) & 1u) != 0u)
            AptCIH_CloneClassMembers(pNode, pClassObject);
        pNode->AssociateInstToClass();
    }

    return pNode;
}

// ---------------------------------------------------------------------------
// placeObjectNCXForm @0x82B0AD28 -- placeObject with the colour supplied as a packed-
// ARGB AptUint32CXForm* (expanded into a scratch AptCXForm), the clip-event hash null.
// ---------------------------------------------------------------------------
AptCIH* AptDisplayList::placeObjectNCXForm(AptCIH* pExistingNode, int nDepth, AptCharacter* pCharacter,
                                           const EAStringC* pName, AptCIH* pParentNode, int bForceRemove,
                                           int16_t nClipDepth, double fFrameValue,
                                           const float* pPositionMatrix, const void* pPlacementClipActions /*console u32 field18*/,
                                           const AptUint32CXForm* pPackedColor)
{
    AptCXForm scratchColor;   // default-constructed (helper vtables installed, channels zeroed)

    const AptCXForm* pColor = nullptr;
    if (pPackedColor != nullptr)
    {
        scratchColor.AptUint32CXFormCopy(pPackedColor);   // expand packed ARGB -> CXForm (2-operand)
        pColor = &scratchColor;
    }

    return placeObject(pExistingNode, nDepth, pCharacter, pName, pParentNode, bForceRemove,
                       nClipDepth, fFrameValue, pColor, pPositionMatrix, pPlacementClipActions,
                       /*pClassObject*/ nullptr);
}

// One source-frame placement node in mergeState's source chain (serialised .apt frame
// data; its first two pointers are the {record, props} pair = an AddToDisplayList/
// ReplaceDisplyListItem ppPlacement command).
struct AptMergeSourceNode
{
    void*                   mpRecord;   // +0x00  PlaceObject record ([0] dword == 3 == place)
    AptFramePlacementProps* mpProps;    // +0x04  runtime placement properties
    int32_t                 mnDepth;    // +0x08  placement depth
    AptMergeSourceNode*     mpNext;     // +0x0C  next placement node
};

// ---------------------------------------------------------------------------
// mergeState @0x82B0B438 -- reconcile this display list against a source frame's
// placement chain, walking both depth-ordered lists in lockstep.
// ---------------------------------------------------------------------------
AptCIH* AptDisplayList::mergeState(void** ppMergeInfo, AptNativeHash* pParentHash, char bKeepRemoved)
{
    AptCIH* pResult = nullptr;
    AptCIH* const pParentNode = static_cast<AptCIH*>(ppMergeInfo[1]);

    AptCIH* pNode = mpHead ? mpHead->mpFirst : nullptr;
    // The source chain head: the pseudo list's head NODE's mpNext (the head node
    // itself is a sentinel). The console read the raw +0xC (AptPseudoCIH_t::mpNext's
    // 32-bit offset); on x64 the link is the typed member at its native position.
    // Each chain node is an AptPseudoCIH_t read through the AptMergeSourceNode pun
    // (mpRecord==mpSource, mpProps==mpPseudoData snapshot overlaying the props,
    // mnDepth==mpContext's depth word, mpNext -- the layouts align on both ABIs).
    AptMergeSourceNode* pSrc = ppMergeInfo[0] != nullptr
        ? reinterpret_cast<AptMergeSourceNode*>(
              reinterpret_cast<AptPseudoCIH_t*>(ppMergeInfo[0])->mpNext)
        : nullptr;

    while (pNode)
    {
        AptCharacterInst* const pInst       = pNode->GetCharacterInst();
        AptRenderItem*    const pRenderItem = pInst->GetRenderItem();
        const int16_t           nExistingDepth = pRenderItem->GetDepth();

        if (nExistingDepth >= 0x4000)
            break;   // sentinel depth: drain remaining source after the loop

        if (!pSrc)
        {
            AptCIH* const pNext = pNode->GetDisplayListNext();
            if (!bKeepRemoved)
                removeObject(pNode);
            pNode = pNext;
            continue;
        }

        if (pSrc->mnDepth == nExistingDepth)
        {
            const uint32_t nTag = pInst->GetTypeTag();
            const bool bSpriteOrAnim = (nTag == 5 || nTag == 9);

            // An AS-owned sprite/animation node (render-item bit27) is left as-is.
            if (bSpriteOrAnim && ((pRenderItem->mFlags >> 27) & 1u) != 0u)
            {
                pNode = pNode->GetDisplayListNext();
                pSrc  = pSrc->mpNext;
                continue;
            }

            AptCIH* const pNextExisting = pNode->GetDisplayListNext();

            if (*static_cast<const int32_t*>(pSrc->mpRecord) != 3)   // not a PlaceObject command
            {
                removeObject(pNode);
                pSrc  = pSrc->mpNext;
                pNode = pNextExisting;
                continue;
            }

            AptFramePlacementProps* const pProps = pSrc->mpProps;
            if (!pProps)
            {
                pResult = ReplaceDisplyListItem(pParentHash, pNode,
                                                reinterpret_cast<void**>(pSrc), pParentNode);
                pSrc  = pSrc->mpNext;
                pNode = pNextExisting;
                continue;
            }

            // Placement-identity gate (evaluated BEFORE any in-place merge).
            const bool bGate1 = (pProps->mi16CharacterId == pNode->GetCreatedOnFrame());
            const bool bGate2 = ((pInst->mTypeFlags & 0xFC000000u) == 0x24000000u);
            if (!bGate1 && !bGate2)
            {
                pResult = ReplaceDisplyListItem(pParentHash, pNode,
                                                reinterpret_cast<void**>(pSrc), pParentNode);
                pSrc  = pSrc->mpNext;
                pNode = pNextExisting;
                continue;
            }

            if (pProps->mpCharacter == nullptr)
            {
                // No new character: merge colour/position in place (never over an AS write).
                if (!pNode->GetASChanged())
                {
                    if ((pProps->mnFlags & 0x8) != 0)
                        pInst->GetRenderItemWritable()->GetColorMatrixWritable()
                            ->AptUint32CXFormCopy(pProps->mpColorTransform);
                    if ((pProps->mnFlags & 0x4) != 0 && pProps->mpPositionMatrix != nullptr)
                        pInst->GetRenderItemWritable()->GetPositionMatrixWritable()
                            ->AptMatrixCopy(reinterpret_cast<const AptMatrix*>(pProps->mpPositionMatrix));
                }
                pSrc  = pSrc->mpNext;
                pNode = pNextExisting;
                continue;
            }

            // A character IS named: merge in place only when it is the same character
            // already here (or the inst is an animation); else replace.
            const bool bSameChar =
                (nTag == static_cast<uint32_t>(pProps->mpCharacter->mnType) &&
                 pProps->mpCharacter == pRenderItem->mpCharacter && bGate1);
            if (!(bSameChar || nTag == 9))
            {
                pResult = ReplaceDisplyListItem(pParentHash, pNode,
                                                reinterpret_cast<void**>(pSrc), pParentNode);
                pSrc  = pSrc->mpNext;
                pNode = pNextExisting;
                continue;
            }

            if (!pNode->GetASChanged())
            {
                if ((pProps->mnFlags & 0x8) != 0)
                {
                    pInst->GetRenderItemWritable()->GetColorMatrixWritable()
                        ->AptUint32CXFormCopy(pProps->mpColorTransform);
                }
                else
                {
                    // No colour supplied: reset the colour transform to identity
                    // (scale channels -> 255, translate channels -> 0).
                    AptCXForm* const pCX = pInst->GetRenderItemWritable()->GetColorMatrixWritable();
                    static const float kScaleId[4]     = { 255.0f, 255.0f, 255.0f, 255.0f };
                    static const float kTranslateId[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    pCX->scale.CopyFromFloatArray(kScaleId);
                    pCX->translate.CopyFromFloatArray(kTranslateId);
                }
                if ((pProps->mnFlags & 0x4) != 0 && pProps->mpPositionMatrix != nullptr)
                    pInst->GetRenderItemWritable()->GetPositionMatrixWritable()
                        ->AptMatrixCopy(reinterpret_cast<const AptMatrix*>(pProps->mpPositionMatrix));
            }
            pSrc  = pSrc->mpNext;
            pNode = pNextExisting;
            continue;
        }
        else if (pSrc->mnDepth > nExistingDepth)
        {
            // Existing node below the source range -> remove (unless keeping), advance.
            AptCIH* const pNext = pNode->GetDisplayListNext();
            if (!bKeepRemoved)
                removeObject(pNode);
            pNode = pNext;
            continue;
        }
        else
        {
            // Source below the existing node -> add every lower source node before it
            // (the existing node is re-examined next outer iteration; not advanced).
            while (pSrc && pSrc->mnDepth < pInst->GetRenderItem()->GetDepth())
            {
                if (*static_cast<const int32_t*>(pSrc->mpRecord) == 3)
                    pResult = AddToDisplayList(pParentHash, reinterpret_cast<void**>(pSrc), pParentNode);
                pSrc = pSrc->mpNext;
            }
            continue;
        }
    }

    // Drain the remaining source placement nodes.
    if (pSrc)
    {
        while (pNode && pSrc &&
               pSrc->mnDepth < pNode->GetCharacterInst()->GetRenderItem()->GetDepth())
        {
            if (*static_cast<const int32_t*>(pSrc->mpRecord) == 3)
                pResult = AddToDisplayList(pParentHash, reinterpret_cast<void**>(pSrc), pParentNode);
            pSrc = pSrc->mpNext;
        }
        while (pSrc)
        {
            if (*static_cast<const int32_t*>(pSrc->mpRecord) == 3)
                pResult = AddToDisplayList(pParentHash, reinterpret_cast<void**>(pSrc), pParentNode);
            pSrc = pSrc->mpNext;
        }
    }

    return pResult;
}

// ===========================================================================
// Leaf-first placement callees -- the un-homed-at-link helpers the placement
// spine above (instantiateCharacter / placeObject / AddToDisplayList) names.
// Decompiled faithfully from the X360 ARTIST.XEX (cross-checked vs the PS3
// DecFIGS DWARF where the X360 body is folded), homed here next to their callers.
// ===========================================================================

// FLAG (the process-wide AS VM -- homed by AptActionInterpreter, console
// dword_8324E760 == &gAptActionInterpreter.mnStackTop): the class-member clone
// stores variables through it (the console hard-codes &dword_8324E760 as the
// setVariable receiver). Committed extern in the sibling AptCIHNativeFunctionHelper.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// ---------------------------------------------------------------------------
// AptDLState_CreateInstAtDepth -- sub_82B008B0.
//   Create a fresh AptCIH(pCharacter, pParentNode), locate the insert-after slot
//   at nDepth (findInst's prev output; the match output is ignored here), stamp the
//   new node's render-item depth, insert it after that slot, return it.
//   X360: AptCIH::operator new(40) + AptCIH::AptCIH(.,char,parent); findInst(state,
//   depth, 0, &prev, &match); GetRenderItemWritable(node->mpCharacterInst); *(item+0x14)
//   = depth (sth); insert(state, prev, node).
// ---------------------------------------------------------------------------
AptCIH* AptDLState_CreateInstAtDepth(AptDisplayListState* pState, int nDepth,
                                     AptCharacter* pCharacter, AptCIH* pParentNode)
{
    void* const pMem = AptCIH::operator new(sizeof(AptCIH));
    AptCIH* const pNode = pMem ? ::new (pMem) AptCIH(pCharacter, pParentNode) : nullptr;

    AptCIH* pPrev  = nullptr;
    AptCIH* pMatch = nullptr;
    pState->findInst(nDepth, nullptr, &pPrev, &pMatch);   // findInst(state, depth, 0, &prev, &match)

    // Stamp the new node's render-item depth (console sth r28, 0x14(item)).
    pNode->GetCharacterInst()->GetRenderItemWritable()->SetDepth(nDepth);

    AptCIH* const pInserted = pState->insert(pPrev, pNode);   // insert after the located prev slot
    return pInserted;
}

// ---------------------------------------------------------------------------
// AptDLState_ReinsertInstAtDepth -- sub_82AEE788.
//   Re-insert an existing node at nDepth: locate the insert-after slot (findInst),
//   insert pNode there, then stamp the (re)inserted node's render-item depth.
//   X360 order: findInst(state, depth, 0, &prev, &match); node2 = insert(state, prev,
//   node); GetRenderItemWritable(node2->mpCharacterInst); *(item+0x14) = (i16)depth.
// ---------------------------------------------------------------------------
AptCIH* AptDLState_ReinsertInstAtDepth(AptDisplayListState* pState, int nDepth, AptCIH* pNode)
{
    const int16_t nDepth16 = static_cast<int16_t>(nDepth);   // console v5 = (__int16)a2

    AptCIH* pPrev  = nullptr;
    AptCIH* pMatch = nullptr;
    pState->findInst(nDepth, nullptr, &pPrev, &pMatch);

    AptCIH* const pInserted = pState->insert(pPrev, pNode);
    pInserted->GetCharacterInst()->GetRenderItemWritable()->SetDepth(nDepth16);
    return pInserted;
}

// ---------------------------------------------------------------------------
// AptDL_FramePlacementDispatch -- sub_82B0B080. A real standalone X360 function
// (`bl sub_82B0B080` in AddToDisplayList @0x82B0B150) with no per-address dossier in
// the export set yet, so this body is a ROLE reconstruction: field-for-field it
// mirrors the fully attested sibling AptMovie_PlaceCommand (sub_82B0AE08), which
// parses the same native-8 PlaceObject record; the caller's post-dispatch instance-
// name check proves the dispatcher binds the record's name. Export + decompile
// sub_82B0B080 to upgrade this to attestation. The dispatcher
// creates/re-uses the placed node for a serialised .apt frame-placement command and
// returns it. ppPlacement[1] is the AptFramePlacementProps/pseudo-data overlay; the
// authored name / clip-depth / clip-actions live in ppPlacement[0]'s raw PlaceObject
// record, the same native-8 body layout AptMovie_PlaceCommand parses.
//
// FLAG: sub_82B0B080's own body is folded (no per-address dossier in the X360 export
// and absent as a named function in the PS3 DWARF). The faithful observable behaviour
// -- materialise the placed node at the command's depth and return it -- is
// reconstructed through the homed placeObject, reading the placement command's named
// fields (the same AptFramePlacementProps the sibling ReplaceDisplyListItem reads).
// ---------------------------------------------------------------------------
AptCIH* AptDL_FramePlacementDispatch(AptDisplayList* pThis, void** ppPlacement, AptCIH* pParentNode)
{
    AptFramePlacementProps* const pProps = static_cast<AptFramePlacementProps*>(ppPlacement[1]);
    const AptPlaceObjectInfo_t* const pInfo =
        static_cast<const AptPlaceObjectInfo_t*>(ppPlacement[0]);

    const uint32_t nFlags = pInfo->muxFlags;
    const int32_t  nDepth = pInfo->mi32Depth;

    EAStringC nameStr;
    const EAStringC* pName = nullptr;
    if ((nFlags & 0x20u) != 0u)
    {
        const char* const pNamePtr = pInfo->mpName;
        if (pNamePtr != nullptr)
        {
            nameStr = EAStringC(pNamePtr);
            pName = &nameStr;
        }
    }

    const float* const pPosition =
        ((pProps->mnFlags & 0x4) != 0) ? pProps->mpPositionMatrix : nullptr;
    const AptUint32CXForm* const pPackedColor =
        ((pProps->mnFlags & 0x8) != 0) ? pProps->mpColorTransform : nullptr;
    const double fFrameValue = static_cast<double>(pInfo->mfRatio);
    const int16_t nClipDepth = static_cast<int16_t>(pInfo->miClipDepth);

    const void* pClipActions = nullptr;
    if ((nFlags & 0x80u) != 0u)
        pClipActions = pInfo->mpClipActions;

    return pThis->placeObjectNCXForm(
        /*pExistingNode*/ nullptr, nDepth, pProps->mpCharacter, pName, pParentNode,
        /*bForceRemove*/ 0, nClipDepth, fFrameValue, pPosition, pClipActions, pPackedColor);
}

// (AptCharInst_Get/SetPlacementField18 RETIRED 2026-07-07: the console inlines a single
//  deref of the sprite-base subclass slot at +0x18 == AptCharacterSpriteInstBase::
//  mpClipEventHandlers; the placeObject write + the InsertChild read now access that named
//  member directly at their call sites, so no free-function accessor remains.)

// ---------------------------------------------------------------------------
// AptCIH_CloneClassMembers -- the AS class-object member copy folded into the X360's
// InsertChild / placeObject AS-class tail (@0x82B09CA0): walk the class object's
// native hash and copy every member -- skipping the two reserved keys __proto__
// (StringPool::saConstant, console dword_8324E580) and prototype (gAptKeyPrototype,
// console dword_8324E698) -- onto the freshly placed instance via setVariable.
//   X360: hash = (*(*classObj+8))(classObj);  // GetNativeHashVirtual (vtbl slot 2)
//         for (i = GetFirstItem(); i; i = GetNextItem(hash, i))
//           if (!match(i,__proto__) && !match(i,prototype))
//             setVariable(&interp, inst, 0, i, *(i+4), 1, 1, 0);
// (The reserved-key match in the X360 is the same EAStringC compare the sibling
// init-object copy in AptActionInterpreterInterpHelpers.cpp uses.)
// ---------------------------------------------------------------------------
void AptCIH_CloneClassMembers(AptCIH* pNode, AptValue* pClassObject)
{
    AptNativeHash* const pHash = pClassObject->GetNativeHashVirtual();   // (*(*classObj+8))(classObj)
    if (!pHash)
        return;

    for (AptHashItem* pItem = pHash->GetFirstItem(); pItem != nullptr;
         pItem = pHash->GetNextItem(pItem))
    {
        // Skip the two reserved keys (__proto__ / prototype).
        if (pItem->mKey == StringPool::saConstant[0] || pItem->mKey == gAptKeyPrototype)
            continue;
        gAptActionInterpreter.setVariable(static_cast<AptValue*>(pNode), nullptr,
                                          &pItem->mKey, pItem->mpValue, 1, 1, 0);
    }
}

// ---------------------------------------------------------------------------
// AptCIH_AssociateInstToClass -- AptCIH::AssociateInstToClass @0x82B073B8.
//
// Bind a freshly placed sprite/animation instance to its registered ActionScript
// class. FAITHFUL HEAD (from the X360 ARTIST.XEX): gate on the instance being a
// sprite(5)/animation(0x10 tag) -- or a class-tagged (mTypeFlags 0xFC000000 ==
// 0x24000000) instance -- whose render item is not already class-bound (item->mFlags
// bit27 clear). The class binding itself (build the AptPrototype, wire __proto__,
// resolve the named class value off the registered class-name hash, run the AS
// constructor) is the deep AS-execution tail, routed through the cluster callee below.
//
// FLAG (deferred AS-execution tail, modelled inline as a marked no-op -- NO new
// unresolved symbol introduced): once gated, the X360 builds a fresh AptPrototype for
// the instance's property hash + wires its __proto__ to the registered base Object
// prototype (off_8324E380+8 hash, key dword_8324E640), locates the character in the
// owning movie's character table, resolves the class value the entry names (off the
// registered class-name hash dword_8324E2D4), chains __proto__ to that class's
// prototype, ticks the node, pushes it across the interpreter's instance/this stacks
// (off_8324E774/E798), runs the class constructor (AptActionInterpreter::callFunction
// over the register window off_8324E3D0 / the dword_8324E760 VM), pops the stacks, and
// -- when the node carries built-in events (FindAndSetEvents) -- registers it on the
// director's event-target set (off_8324E574). Every one of those steps reads/writes the
// process-wide AS-VM scratch globals (the registered class-name hash + the registered
// Object prototype + the interpreter register window/instance/this stacks) that are
// owned by the AS-name-registration + interpreter boot TUs and are NOT yet homed; the
// X360 short-circuits the entire tail when none of those classes is registered (the
// `*(v4+12) <= 0` / `dword_8324E2D4 == 0` / `class value not found` early-outs), which
// is exactly the boot state until that engine is homed. So the faithful observable
// result during bring-up is the gated early-out: bind nothing, return 0. This is the
// same deferred AS-execution sub-path queueClipEvents / ClearCIH route through a cluster
// callee -- modelled here as a no-op tail so the homed gate links with no new extern.
int AptCIH::AssociateInstToClass()
{
    AptCIH* const pNode = this;   // console r3 (`result`) is the node `this`
    AptCharacterInst* const pInst = pNode->GetCharacterInst();   // *(node+32)
    if (!pInst)
        return 0;

    // Gate: sprite(5) / animation(tag 0x10) instance, or a class-tagged instance
    // (mTypeFlags 0xFC000000 == 0x24000000), whose render item is not yet class-bound
    // (item->mFlags bit27 clear).
    const uint32_t nTag = pInst->mTypeFlags >> 26;            // v2[2] >> 26
    const bool bSpriteLike = (nTag == 5u || nTag == 16u);
    if (!bSpriteLike && (pInst->mTypeFlags & 0xFC000000u) != 0x24000000u)
        return 0;
    if (((pInst->GetRenderItem()->mFlags >> 27) & 1u) != 0u)   // already class-bound
        return 0;

    // ---- the class-binding tail (UN-DEFERRED 2026-07-05: the AS framework is live,
    // Object.registerClass populates gpAptClassRegistry) -- faithful to the X360
    // @0x82B073B8 body:
    //   1. fresh prototype on the node + __proto__ = the MovieClip builtin's prototype
    //      (console Lookup(_global+8, &dword_8324E640 "MovieClip") -> *(hash+12));
    //   2. resolve the placed char's EXPORT NAME (the owning ROOT movie's export table:
    //      match charTable[entry.id] == the char);
    //   3. look the name up in the class registry (dword_8324E2D4); on a defined hit:
    //      node.__proto__ = class.prototype; tick the node once (byte_82F733F7 gate,
    //      boot default 0) + SetHasClass(1) (vtbl slot 5, `(*(*v1+20))(v1, 1)`); RUN
    //      THE CLASS CONSTRUCTOR on the node under GC-root protection (interp stacks
    //      B(+0x0C)/E(+0x30) pushes + the register-window save,
    //      callFunction(node, class, 0), pops + restore);
    //   4. FindAndSetEvents (the clip-event member mask the dispatcher consumes).
    //      FLAG: the console then adds the node to the director's event-target set
    //      (off_8324E574 listener list) when events were found -- that container is
    //      un-homed; the member-mask wiring below is what queueClipEvents reads.
    AptCharacter* const pChar = pInst->GetRenderItem()->mpCharacter;
    if (pChar == nullptr || pChar->mpFixupLink == nullptr)
        return 0;
    const AptMovieData* const pMovie =
        reinterpret_cast<const AptMovieData*>(pChar->mpFixupLink);   // the ROOT char header

    // console: the prototype/__proto__ land on the CHAR INST's property hash
    // (v2[3] == pInst->mpProperties -- the same hash timeline variables use),
    // NOT the CIH's value hash (which is null for placed clips).
    AptNativeHash* const pNodeHash = pInst->mpProperties;
    if (pNodeHash == nullptr)
        return 0;

    // 1. fresh prototype + the MovieClip builtin's prototype as __proto__.
    AptPrototype* const pFreshProto = new AptPrototype();
    pNodeHash->SetPrototype(pFreshProto);
    if (gpAptGlobalFallback != nullptr)
    {
        AptNativeHash* const pGlobals = gpAptGlobalFallback->GetNativeHashVirtual();
        AptValue* const pMcClass = pGlobals ? pGlobals->Lookup(gAptSpriteClassKey) : nullptr;
        AptNativeHash* const pMcHash = pMcClass ? pMcClass->GetNativeHashVirtual() : nullptr;
        if (pMcHash != nullptr && pMcHash->GetPrototype() != nullptr)
            pNodeHash->Set__Proto__(pMcHash->GetPrototype());
    }

    // 2. the char's export name.
    const char* pExportName = nullptr;
    if (pMovie->mnExportCount > 0 && pMovie->mpExportTable != nullptr
        && pMovie->mpCharacterTable != nullptr)
    {
        for (int32_t li = 0; li < pMovie->mnExportCount; ++li)
        {
            const AptExportEntry& lrEntry = pMovie->mpExportTable[li];
            if (lrEntry.mnCharacterId >= 0 && lrEntry.mnCharacterId < pMovie->mnCharacterCount
                && pMovie->mpCharacterTable[lrEntry.mnCharacterId] == pChar)
            {
                pExportName = lrEntry.mpName;
                break;
            }
        }
    }
    if (pExportName == nullptr || gpAptClassRegistry == nullptr)
        return 0;

    // 3. the registered class.
    EAStringC lName(pExportName);
    AptValue* const pClass = gpAptClassRegistry->Lookup(lName);
    if (pClass == nullptr || !pClass->getIsDefined())
        return 0;

    AptNativeHash* const pClassHash = pClass->GetNativeHashVirtual();
    AptValue* const pClassProto = pClassHash ? pClassHash->GetPrototype() : nullptr;
    if (pClassProto != nullptr && pClassProto->getIsDefined())
        pNodeHash->Set__Proto__(pClassProto);

    // The instance is class-bearing before its constructor/onLoad method calls run
    // (X360 vtbl slot 5 call `(*(*v1+20))(v1, 1)` == SetHasClass(1)). Method dispatch
    // tests AptValue::GetHasClass on the receiver to bind `this`; leaving the CIH flag
    // clear made nested RegisterComponent calls execute on _global instead of this clip.
    pNode->SetHasClass(1);
    // FLAG (stand-in for the un-homed real setter): the X360 @0x82B073B8 body performs
    // NO store to the render item's flags -- something else marks the item class-bound
    // (bit 27) so this function's entry gate skips already-bound items on later passes.
    // Until that setter's home is recovered, set the gate bit here so a placed clip is
    // bound exactly once (removing it would re-run class ctors every placement pass).
    pInst->GetRenderItemWritable()->mFlags |= (1u << 27);

    // 4. run the class constructor on the node (GC-root protected).
    AptValue** const lppSavedRegs = AptScriptFunctionBase::PushStaticData();
    gAptActionInterpreter.mpCallStackB[gAptActionInterpreter.mnCallStackB_Count++] = pNode;
    pNode->AddRef();
    gAptActionInterpreter.mpCallStackE[gAptActionInterpreter.mnCallStackE_Count++] = pNode;
    pNode->AddRef();
    pNode->mFlagsA |= 0x8000000u;
    gAptActionInterpreter.callFunction(static_cast<AptValue*>(pNode), pClass, 0, nullptr, nullptr);
    pNode->mFlagsA &= ~0x8000000u;

    if (gAptActionInterpreter.mnCallStackE_Count > 0)
    {
        AptValue* const pTopE = reinterpret_cast<AptValue*>(
            gAptActionInterpreter.mpCallStackE[gAptActionInterpreter.mnCallStackE_Count - 1]);
        if (pTopE) pTopE->Release();
        --gAptActionInterpreter.mnCallStackE_Count;
    }
    if (gAptActionInterpreter.mnCallStackB_Count > 0)
    {
        AptValue* const pTopB = reinterpret_cast<AptValue*>(
            gAptActionInterpreter.mpCallStackB[gAptActionInterpreter.mnCallStackB_Count - 1]);
        if (pTopB) pTopB->Release();
        --gAptActionInterpreter.mnCallStackB_Count;
    }
    if (gAptActionInterpreter.mnStackTop > 0)
        gAptActionInterpreter.stackPop();   // the constructor's result
    AptScriptFunctionBase::PopStaticData(lppSavedRegs);

    // 5. wire the clip-event member mask.
    pNode->FindAndSetEvents();

    // The console's "tick once" (byte_82F733F7 gate, boot default 0) runs BEFORE the
    // ctor on the X360. FLAG (x64 onLoad-ORDER fix, boot-verified): ticked there, the
    // fresh(0x80) clip clears its fresh bit while HasEvent(onLoad) is still FALSE --
    // FindAndSetEvents (step 5) has not set the event-handler mask yet -- so onLoad was
    // never queued (0 SendAptEvent -> no AddNewAptComponent -> clips hidden -> black).
    // The tick is moved AFTER FindAndSetEvents so the fresh clip dispatches onLoad with
    // the mask set; verified: all 9 title-clip onLoad handlers then RUN. The ctor still
    // runs before onLoad (Flash order) and against a valid display list (the subtree is
    // composed by doFrameControls at placement, not by this tick). Verify vs XB1
    // AssociateInstToClass whether the retail builds fire onLoad from this binding tick
    // or leave 0x80 set for a later per-frame tick -- either way onLoad must fire AFTER
    // the event mask is set.
    pNode->tick();
    return 1;
}
