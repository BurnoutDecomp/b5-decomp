// ===========================================================================
// EATech Apt -- the RENDER-TREE WALK (the display-list flush to draw calls).
//
// DECOMPILED FAITHFULLY from the X360 ARTIST.XEX. This is the traversal that turns
// the per-target render-item tree (built by AptRenderTreeManager::Update_* as the
// display list mutates) into per-visible-node draw calls. Chain:
//
//   AptRender @0x82AF33E8         -- set the render-thread TLS, call the internal walk
//   AptRenderInternal @0x82AF3278 -- (sub_82AF3278) prune manager queue, reset clip
//                                    stack, bump the render tick, get the root render
//                                    item, kick the recursive traversal
//   AptDecoupleTreeTraversal @0x82AE4480        -- the recursive sibling/child walk
//   AptDecoupleMaskedTreeTraversal @0x82ADF6D8  -- a mask's own display list
//   AptDecoupleClippedTreeTraversal @0x82ADF940 -- a clip-start node's clipped span
//   AptDecoupleTreeTraversalClipper @0x82ADF808 -- walk the clipped span to the sentinel
//   AptRenderLeaf @0x82AD9880                   -- per-visible-node: PushRenderData ->
//                                                  Render -> PopRenderData (the draw)
//   AptDecoupleTreeCleanUpInvisibleRI @0x82ADF640 -- prune invisible movie-clip RIs
//
// The per-node "draw" is the render item's Render() virtual (X360 vtable slot +0x10),
// which for a shape is AptRenderItemShape::Render -> AptCharacter::render ->
// AptHook_DrawShape -> gAptFuncs.pfnDrawRenderingUnit (== CgsGui::AptCallbackRender::
// DrawRenderingUnit -> CgsGui::AptRenderHandler::Render). The render-open/close brackets
// are the PushRenderData/PopRenderData virtuals (slots +0x04/+0x0C) and the mask push is
// PushRenderDataAbsolute (slot +0x08) -- called BY NAME here (the reconstructed C++ vtable
// dispatches; the raw slot offsets are documentation only).
//
// Render-item fields the walk reads (X360 dword indices -> named members):
//   item[6]  +0x18  mFlags               (visibility / mask / clip / renderable flags)
//   item[5]  +0x14  mDepth (low16)        (*(item+20))
//   item[5]  +0x16  mClipDepth (high16)   (*(item+22))
//   item[7]  +0x1C  mpMask
//   item[11] +0x2C  mpManagerNextSibling
//   item[12] +0x30  mpManagerFirstChild
// Every child/sibling deref is preceded by Manager_GetRenderRevision(link,tick) +
// Manager_Update*(node,resolved) -- the tick-versioned link resolution. On the single-
// buffered PC path (the update tick never advances mid-frame) GetRenderRevision returns
// the live item, so the walk draws the current tree exactly.
//
// GLOBALS: the render tick is the CONSUMED bank gnCurrRenderTickConsumed (X360
// dword_8324E524): each pass banks the caller's elapsed milliseconds onto it, clamped
// strictly below the gnCurrUpdateTick (dword_8324E520) update bank -- the decoupled
// update/render tick pair AptUpdate.cpp's credit gate pairs with. The active
// target/manager is gpAptTargetTLS (off_8324E578, the render-side current target
// AptRenderTarget swaps), whose mppRenderRootAnchor (+0x2C) is the render-root chain
// head Render_GetRoot walks. The render CONTEXT (X360 dword_8324E2AC) is a single
// AptRenderingContext this TU owns (the console allocates one at render bring-up;
// here it is a file-static, brought up on first use).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"          // gpAptTargetTLS / mppRenderRootAnchor
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // gnCurrUpdateTick
#include "SDKs/EATech/Apt/AptMath.h"                    // AptMath::ClipStackMakeUnit
#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"  // AptRenderManagerQueue::Clean

// The global render-manager teardown queue (X360 dword_8324E7D8) -- defined in AptGlobals.cpp.
// sub_82AF3278 flushes it each render pass (it only holds cells deferred off the render thread,
// which does not happen on the single-threaded PC path, so the flush is normally a no-op).
extern AptRenderManagerQueue gAptRenderManagerQueue;

// The update/render tick bank pair (AptGlobals.cpp) -- the render pass consumes elapsed
// milliseconds against the update side's banked credit (see the file-header GLOBALS note).
extern int gnCurrRenderTickConsumed;   // dword_8324E524

// The VM deferred-release queue the render pass drains first (font units unresolved off the
// VM thread park here; see AptCharacterAnimation.cpp's unresolve notes). Single-threaded PC
// unresolves release directly, so the drain is normally empty -- kept for the faithful shape.
extern int   gAptDeferredReleaseCount;               // dword_8324E508
extern void* gAptDeferredReleaseQueue;               // off_8324E2C8
extern void  AptFreeFontUnit(void* pUnit);           // dword_8324E870 host thunk
extern void* gAptUnresolveMutex;                     // unk_8324E728
extern void* gAptUnresolveMutexName;                 // unk_82143270
namespace EA { namespace Thread {
    extern void Mutex_Lock(void* pMutex, void* pName);
    extern void Mutex_Unlock(void* pMutex);
} }

// The render-thread current-target TLS mirror (unk_8324E814; the same object AptUpdate.cpp
// stores through -- defined in AptRenderLinkStubs.cpp, single-threaded slot on the PC).
namespace EA { namespace Thread {
    class ThreadLocalStorage
    {
    public:
        ThreadLocalStorage() : mTlsIndex(0) {}
        bool  SetValue(const void* pData);
        void* GetValue();
        u32   mTlsIndex;
    };
} }
extern EA::Thread::ThreadLocalStorage gAptTargetTls;

#include <new>

// AptCleanupInvisibleMovieClipInternal @0x82ADF590 (HOMED 2026-07-10, retiring the
// {} body -- menus toggle _visible constantly, so the invisible-clip cleanup IS a
// menu-path walk): recursively visit an invisible movie clip's render subtree
// through the render-tree manager's invisible-revision getters -- for each node in
// the sibling run, fetch its invisible child + mask revisions and recurse into any
// that is itself an invisible movie clip (mFlags bit26). The manager getters
// perform the revision bookkeeping; the walk's job is reaching every invisible
// descendant exactly once (asm: the do/while sibling loop + the two recursions).
static void AptCleanupInvisibleMovieClipInternal(AptRenderItem* pItem, int nTick)
{
    if (gpAptTargetTLS == nullptr)
        return;
    AptRenderTreeManager* const lpMgr =
        reinterpret_cast<AptRenderTreeManager*>(gpAptTargetTLS->mppRenderRootAnchor);
    if (lpMgr == nullptr)
        return;

    while (pItem != nullptr)   // console: the loc_82ADF5BC sibling loop
    {
        AptRenderItem* const lpChild = lpMgr->Render_GetChildInvisible(pItem, nTick);
        AptRenderItem* const lpMask  = lpMgr->Render_GetMaskInvisible(pItem, nTick);

        // console order: the MASK recursion first, then the child (bit26 == the
        // went-invisible-this-frame flag, `extrwi r11,1,5` on mFlags).
        if (lpMask && (lpMask->mFlags & 0x20u) != 0)
            AptCleanupInvisibleMovieClipInternal(lpMask, nTick);
        if (lpChild && (lpChild->mFlags & 0x20u) != 0)
            AptCleanupInvisibleMovieClipInternal(lpChild, nTick);

        pItem = lpMgr->Render_GetSiblingInvisible(pItem, nTick);
    }
}

namespace
{
    // The Apt AS mask-render op the walk threads as a4 (0 == Normal on the render pass).
    // The traversal is kicked with a4 == 0 (sub_82AF3278: AptDecoupleTreeTraversal(root, tick,
    // ctx, 0, 0, mask, 1)); it stays 0 down the normal-draw path (masks flip it locally).
    inline AptMaskRenderOperation NormalOp() { return static_cast<AptMaskRenderOperation>(0); }

    // The single render context the walk threads (X360 dword_8324E2AC). One instance, brought
    // up lazily -- the console allocates one at render init; here it is owned + reset each walk
    // is unnecessary (its stacks self-balance across a matched Push/Pop walk).
    AptRenderingContext* gpAptRenderContext = nullptr;
    AptRenderingContext  gAptRenderContextStorage;

    AptRenderingContext* GetRenderContext()
    {
        if (gpAptRenderContext == nullptr)
            gpAptRenderContext = new (&gAptRenderContextStorage) AptRenderingContext();
        return gpAptRenderContext;
    }
}

// Forward decls (mutually recursive walk).
static void AptDecoupleTreeTraversal(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                     AptMaskRenderOperation eOp, int nDepth, int nLayerMask, int bTopLevel);
static void AptDecoupleMaskedTreeTraversal(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                           AptMaskRenderOperation eOp, int nDepth, int nLayerMask);
static AptRenderItem* AptDecoupleClippedTreeTraversal(AptRenderItem* pItem, int nTick,
                                                      AptRenderingContext* pCtx, AptMaskRenderOperation eOp,
                                                      int nDepth, int nLayerMask);
static void AptDecoupleTreeTraversalClipper(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                            AptMaskRenderOperation eOp, int nDepth, int nLayerMask,
                                            int bTopLevel, AptRenderItem* pSentinel);
static void AptRenderLeaf(AptRenderItem* pItem, bool bBracket, AptRenderingContext* pCtx,
                          AptMaskRenderOperation eOp, int nDepth);
static void AptDecoupleTreeCleanUpInvisibleRI(AptRenderItem* pItem, int nTick);

// ---------------------------------------------------------------------------
// AptRenderLeaf @0x82AD9880 -- render one leaf node.
//   if (bracket) item->PushRenderData(ctx, op, depth);   // vtbl +0x04 (render-open)
//   item->Render(ctx, op, depth);                        // vtbl +0x10 (draw geometry)
//   if (bracket) item->PopRenderData(ctx, op, depth);    // vtbl +0x0C (render-close)
// bBracket is the "this node owns no separate child list, so bracket the push/pop here".
// ---------------------------------------------------------------------------
static void AptRenderLeaf(AptRenderItem* pItem, bool bBracket, AptRenderingContext* pCtx,
                          AptMaskRenderOperation eOp, int nDepth)
{
    if (bBracket)
        pItem->PushRenderData(pCtx, eOp, nDepth);
    pItem->Render(pCtx, eOp, nDepth);
    if (bBracket)
        pItem->PopRenderData(pCtx, eOp, nDepth);
}

// ---------------------------------------------------------------------------
// AptDecoupleTreeCleanUpInvisibleRI @0x82ADF640 -- for a node whose child/mask link is
// now an invisible movie clip (mFlags bit26 set), release that RI's render-tree refs.
//   mgr = gpAptTargetTLS->mppRenderRootAnchor;
//   child = mgr->Render_GetChildInvisible(item, tick);
//   mask  = mgr->Render_GetMaskInvisible (item, tick);
//   if (child && child->mFlags bit26) AptCleanupInvisibleMovieClipInternal(child, a2);
//   if (mask  && mask ->mFlags bit26) AptCleanupInvisibleMovieClipInternal(mask,  a2);
// ---------------------------------------------------------------------------
static void AptDecoupleTreeCleanUpInvisibleRI(AptRenderItem* pItem, int nTick)
{
    if (gpAptTargetTLS == nullptr)
        return;
    AptRenderTreeManager* lpMgr =
        reinterpret_cast<AptRenderTreeManager*>(gpAptTargetTLS->mppRenderRootAnchor);
    if (lpMgr == nullptr)
        return;

    AptRenderItem* lpChild = lpMgr->Render_GetChildInvisible(pItem, nTick);
    AptRenderItem* lpMask  = lpMgr->Render_GetMaskInvisible(pItem, nTick);

    if (lpChild && (lpChild->mFlags & 0x20u) != 0)
        AptCleanupInvisibleMovieClipInternal(lpChild, nTick);
    if (lpMask && (lpMask->mFlags & 0x20u) != 0)
        AptCleanupInvisibleMovieClipInternal(lpMask, nTick);
}

// ---------------------------------------------------------------------------
// AptDecoupleTreeTraversalClipper @0x82ADF808 -- walk a clipped SPAN of siblings up to
// (and including) pSentinel, rendering each. Mirrors AptDecoupleTreeTraversal's leaf-vs-
// recurse decision, but its sibling loop stops at pSentinel (the clip-end node).
// ---------------------------------------------------------------------------
static void AptDecoupleTreeTraversalClipper(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                            AptMaskRenderOperation eOp, int nDepth, int nLayerMask,
                                            int bTopLevel, AptRenderItem* pSentinel)
{
    (void)bTopLevel;
    AptRenderItem* lpNode = pItem;
    while (lpNode != nullptr)
    {
        AptRenderItem* lpChildRev = lpNode->mpManagerFirstChild
            ? lpNode->mpManagerFirstChild->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateFirstChild(lpChildRev);
        AptRenderItem* lpChild = lpNode->mpManagerFirstChild;   // v14[12]

        // bracket == leaf when: no first child, OR the node is not a "recurse-as-parent"
        // mask-owner ((mFlags & 0xFC0000) != 0x400000).
        const bool lbMaskOwner = (lpChild != nullptr) && ((lpNode->mFlags & 0x3F00u) == 0x1000u);
        if (lpChild == nullptr || lbMaskOwner)
        {
            AptRenderLeaf(lpNode, lbMaskOwner, pCtx, eOp, nDepth);
        }
        else
        {
            lpNode->PushRenderData(pCtx, eOp, nDepth);
            AptDecoupleTreeTraversalClipper(lpChild, nTick, pCtx, eOp, nDepth, nLayerMask, 0, pSentinel);
            lpNode->PopRenderData(pCtx, eOp, nDepth);
        }

        if (lpNode == pSentinel)
            break;

        AptRenderItem* lpSibRev = lpNode->mpManagerNextSibling
            ? lpNode->mpManagerNextSibling->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateNextSibling(lpSibRev);
        lpNode = lpNode->mpManagerNextSibling;   // v14[11]
    }
}

// ---------------------------------------------------------------------------
// AptDecoupleClippedTreeTraversal @0x82ADF940 -- a clip-START node was found. Walk the run
// of following siblings that fall within its clip depth (`item->mClipDepth < sibling->mDepth`
// while `sibling->mClipDepth < 0x8000`), rendering each, and return the node the outer walk
// should resume at (the first sibling that is NOT clipped by this one).
// ---------------------------------------------------------------------------
static AptRenderItem* AptDecoupleClippedTreeTraversal(AptRenderItem* pItem, int nTick,
                                                      AptRenderingContext* pCtx, AptMaskRenderOperation eOp,
                                                      int nDepth, int nLayerMask)
{
    AptRenderItem* lpNode = pItem;
    AptRenderItem* lpResume = pItem;
    do
    {
        lpResume = lpNode;

        AptRenderItem* lpChildRev = lpNode->mpManagerFirstChild
            ? lpNode->mpManagerFirstChild->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateFirstChild(lpChildRev);
        AptRenderItem* lpChild = lpNode->mpManagerFirstChild;   // *(v12+48)

        const bool lbMaskOwner = (lpChild != nullptr) && ((lpNode->mFlags & 0x3F00u) == 0x1000u);
        if (lpChild == nullptr || lbMaskOwner)
        {
            AptRenderLeaf(lpNode, lbMaskOwner, pCtx, eOp, nDepth);
        }
        else
        {
            lpNode->PushRenderData(pCtx, eOp, nDepth);
            // The clipper walks the child span with no explicit sentinel (a8==0 -> to the end).
            AptDecoupleTreeTraversalClipper(lpChild, nTick, pCtx, eOp, nDepth, nLayerMask, 0, nullptr);
            lpNode->PopRenderData(pCtx, eOp, nDepth);
        }

        AptRenderItem* lpSibRev = lpNode->mpManagerNextSibling
            ? lpNode->mpManagerNextSibling->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateNextSibling(lpSibRev);
        lpNode = lpNode->mpManagerNextSibling;   // *(v12+44)
    }
    // continue while the next sibling exists AND is within this clip node's clip depth AND
    // is itself a normal (clipDepth < 0x8000) node.  *(a1+22)=item->mClipDepth ; *(v12+20)=
    // sibling->mDepth ; *(v12+22)=sibling->mClipDepth.
    while (lpNode != nullptr
           && pItem->mClipDepth < lpNode->mDepth
           && static_cast<u16>(lpNode->mClipDepth) < 0x8000u);

    return lpResume;
}

// ---------------------------------------------------------------------------
// AptDecoupleMaskedTreeTraversal @0x82ADF6D8 -- walk a mask's own child display list. Same
// leaf-vs-recurse decision as the clipper, sibling loop to the end. (Bracketed by the mask
// push/pop the caller in AptDecoupleTreeTraversal issues around it.)
// ---------------------------------------------------------------------------
static void AptDecoupleMaskedTreeTraversal(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                           AptMaskRenderOperation eOp, int nDepth, int nLayerMask)
{
    AptRenderItem* lpNode = pItem;
    while (lpNode != nullptr)
    {
        AptRenderItem* lpChildRev = lpNode->mpManagerFirstChild
            ? lpNode->mpManagerFirstChild->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateFirstChild(lpChildRev);
        AptRenderItem* lpChild = lpNode->mpManagerFirstChild;   // v12[12]

        // NB: the mask-owner test here is on the ROOT pItem's flags (v6 == the entry item),
        // matching the asm (`(*(v6 + 24) & 0xFC0000) != 0x400000`).
        const bool lbMaskOwner = (lpChild != nullptr) && ((pItem->mFlags & 0x3F00u) == 0x1000u);
        if (lpChild == nullptr || lbMaskOwner)
        {
            AptRenderLeaf(lpNode, lbMaskOwner, pCtx, eOp, nDepth);
        }
        else
        {
            lpNode->PushRenderData(pCtx, eOp, nDepth);
            AptDecoupleMaskedTreeTraversal(lpChild, nTick, pCtx, eOp, nDepth, nLayerMask);
            lpNode->PopRenderData(pCtx, eOp, nDepth);
        }

        AptRenderItem* lpSibRev = lpNode->mpManagerNextSibling
            ? lpNode->mpManagerNextSibling->Manager_GetRenderRevision(nTick) : nullptr;
        lpNode->Manager_UpdateNextSibling(lpSibRev);
        lpNode = lpNode->mpManagerNextSibling;   // v12[11]
    }
}

// ---------------------------------------------------------------------------
// AptDecoupleTreeTraversal @0x82AE4480 -- the top-level recursive walk over a sibling chain.
// For each node (skipping those outside the layer mask when bTopLevel):
//   * a CLIP-START node (mClipDepth < 0x8000 -- a node that clips following siblings) opens a
//     clipped span (AptDecoupleClippedTreeTraversal returns where to resume);
//   * a normal node: if invisible (mFlags>=0) run the invisible-RI cleanup; else if it has a
//     MASK (mpMask, and hasMask gate mFlags bit29 + mask non-null) push the mask + walk the
//     masked subtree + pop; then, unless the "skip children" bit (mFlags bit1) is set, either
//     recurse into its first child (bracketed by Push/Pop) or AptRenderLeaf it; then, if a mask
//     was pushed, pop it.
//   * A clip span left open across siblings of increasing depth is closed by the Clipper when
//     the depth range ends.
// Reconstructed from the X360 pseudocode + asm; the clip-pending pair (v13/v14) is the deferred
// clip node + its resume point.
// ---------------------------------------------------------------------------
static void AptDecoupleTreeTraversal(AptRenderItem* pItem, int nTick, AptRenderingContext* pCtx,
                                     AptMaskRenderOperation eOp, int nDepth, int nLayerMask, int bTopLevel)
{
    AptRenderItem* lpClipPending = nullptr;   // v13 -- a clip-start node awaiting its close
    AptRenderItem* lpClipResume  = nullptr;   // v14 -- where the clip span resumes
    AptRenderItem* lpNode        = pItem;     // v15
    if (lpNode == nullptr)
        return;

    do
    {
        // Layer-mask gate: at the top level, skip a node whose depth bit is not in nLayerMask.
        //   ((1 << node->mDepth) & nLayerMask) != 0   (only when bTopLevel).
        bool lbInLayer = true;
        if (bTopLevel != 0)
            lbInLayer = ((1 << (lpNode->mDepth & 31)) & nLayerMask) != 0;

        if (lbInLayer)
        {
            // Close a still-open clip span whose clip depth the current node exceeds.
            // X360 @0x82AE4480: `AptDecoupleTreeTraversalClipper(v13, a2, a3, -1, --a5, ...)` --
            // the clip node re-renders with op == SUBTRACT (li -1 into r6), which is what pops
            // the mask (DrawRenderingUnit Subtract -> END_MASK). (Was eOp/Normal -- that drew
            // the mask shape a second time instead of closing the mask. FIXED 2026-07-12.)
            if (lpClipPending != nullptr)
            {
                if (lpClipPending->mClipDepth < lpNode->mDepth)
                {
                    AptDecoupleTreeTraversalClipper(lpClipPending, nTick, pCtx,
                                                    static_cast<AptMaskRenderOperation>(-1), nDepth - 1,
                                                    nLayerMask, bTopLevel, lpClipResume);
                    lpClipPending = nullptr;
                    lpClipResume  = nullptr;
                    --nDepth;
                }
            }

            if (lpClipPending != nullptr)
            {
                // still clipped: fall through to the shared node-render below (LABEL_11)
            }
            else if (static_cast<u16>(lpNode->mClipDepth) < 0x8000u)
            {
                // This node is a CLIP-START: open a clipped span; resume at the returned node.
                // X360: `v14 = AptDecoupleClippedTreeTraversal(v15, a2, a3, 1, a5, a6, a7)` --
                // the clip node renders with op == ADD (li 1 into r6), so its shape goes out as
                // MASK GEOMETRY (DrawRenderingUnit Add -> PUSH_MASK_GEOMETRY), not a visible
                // quad; the in-span siblings then render normally UNDER the latched mask. (Was
                // eOp/Normal -- the mask shape drew as a visible textured/white rectangle and
                // no mask was ever pushed: the prompt's opaque white boxes. FIXED 2026-07-12.)
                lpClipResume  = AptDecoupleClippedTreeTraversal(lpNode, nTick, pCtx,
                                                                static_cast<AptMaskRenderOperation>(1),
                                                                nDepth + 1, nLayerMask);
                lpClipPending = lpNode;
                ++nDepth;
                lpNode = lpClipResume;
                goto next_sibling;
            }

            // ---- shared node render (LABEL_11) --------------------------------------------
            {
                const u32 luFlags = lpNode->mFlags;   // *(v15+24)
                if (static_cast<int>(luFlags) >= 0)
                {
                    // isVisible bit (bit31) clear -> node invisible: run the invisible-RI cleanup.
                    AptDecoupleTreeCleanUpInvisibleRI(lpNode, nTick);
                }
                else
                {
                    // ---- mask (if hasMask bit29 set AND the mask link is non-null) ----
                    AptRenderItem* lpMaskItem = nullptr;
                    if (((luFlags >> 29) & 1u) != 0 && lpNode->mpMask != nullptr)   // *(v15+28)
                    {
                        AptRenderItem* lpMaskRev = lpNode->mpMask->Manager_GetRenderRevision(nTick);
                        lpNode->Manager_UpdateMask(lpMaskRev);
                        lpMaskItem = lpNode->mpMask;
                        // push the mask (2-arg PushRenderDataAbsolute), walk its subtree with
                        // op == ADD, and (below) pop it after the node's own children. X360:
                        // `AptDecoupleMaskedTreeTraversal(v17, a2, a3, 1, a5, a6, a7)` then
                        // `(*(*v17 + 12))(v17, a3, 1, a5++)` -- the open walk + pop both carry
                        // op 1 (Add), which is what routes the mask's shapes through
                        // DrawRenderingUnit's PUSH_MASK_GEOMETRY arm instead of a visible draw.
                        // (Was NormalOp() -- FIXED 2026-07-12; the close below already
                        // carried the faithful -1/Subtract.)
                        lpMaskItem->PushRenderDataAbsolute(pCtx);           // vtbl +0x08
                        AptDecoupleMaskedTreeTraversal(lpMaskItem, nTick, pCtx,
                                                       static_cast<AptMaskRenderOperation>(1),
                                                       nDepth + 1, nLayerMask);
                        lpMaskItem->PopRenderData(pCtx, static_cast<AptMaskRenderOperation>(1),
                                                  nDepth + 1);              // vtbl +0x0C
                        ++nDepth;
                    }

                    // ---- the node's own children, unless the skip-children bit (bit1) is set ----
                    if (((luFlags >> 30) & 1u) == 0)   // *(v15+24) bit30 clear
                    {
                        AptRenderItem* lpChildRev = lpNode->mpManagerFirstChild
                            ? lpNode->mpManagerFirstChild->Manager_GetRenderRevision(nTick) : nullptr;
                        lpNode->Manager_UpdateFirstChild(lpChildRev);
                        AptRenderItem* lpChild = lpNode->mpManagerFirstChild;   // *(v15+48)

                        const bool lbMaskOwner = (lpChild != nullptr)
                            && ((lpNode->mFlags & 0x3F00u) == 0x1000u);
                        if (lpChild == nullptr || lbMaskOwner)
                        {
                            AptRenderLeaf(lpNode, lbMaskOwner, pCtx, eOp, nDepth);
                        }
                        else
                        {
                            lpNode->PushRenderData(pCtx, eOp, nDepth);
                            AptDecoupleTreeTraversal(lpChild, nTick, pCtx, eOp, nDepth, nLayerMask, 0);
                            lpNode->PopRenderData(pCtx, eOp, nDepth);
                        }
                    }

                    // ---- close the mask ----
                    if (lpMaskItem != nullptr)
                    {
                        --nDepth;
                        lpMaskItem->PushRenderDataAbsolute(pCtx);            // vtbl +0x08 (mask pop bracket)
                        AptDecoupleMaskedTreeTraversal(lpMaskItem, nTick, pCtx, static_cast<AptMaskRenderOperation>(-1),
                                                       nDepth, nLayerMask);
                        lpMaskItem->PopRenderData(pCtx, static_cast<AptMaskRenderOperation>(-1), nDepth);
                    }
                }
            }
        }

    next_sibling:
        {
            AptRenderItem* lpSibRev = lpNode->mpManagerNextSibling
                ? lpNode->mpManagerNextSibling->Manager_GetRenderRevision(nTick) : nullptr;
            lpNode->Manager_UpdateNextSibling(lpSibRev);
            lpNode = lpNode->mpManagerNextSibling;   // *(v15+44)
        }
    }
    while (lpNode != nullptr);

    // A clip span still open at the end of the sibling chain is closed now. X360:
    // `AptDecoupleTreeTraversalClipper(v13, a2, a3, -1, a5 - 1, a6, a7, v14)` -- op ==
    // SUBTRACT (the mask pop), same fix as the in-loop close above (was eOp/Normal).
    if (lpClipPending != nullptr)
        AptDecoupleTreeTraversalClipper(lpClipPending, nTick, pCtx,
                                        static_cast<AptMaskRenderOperation>(-1), nDepth - 1,
                                        nLayerMask, bTopLevel, lpClipResume);
}
// ---------------------------------------------------------------------------
// AptRenderInternal (X360 sub_82AF3278) -- the render pass body.
//   * drain the VM deferred-release queue (font units unresolved off the VM thread)
//     under the unresolve mutex;
//   * flush the render-manager teardown queue when it holds deferred cells;
//   * push a fresh identity/full-screen clip (++word_8324E390 + the unit store);
//   * bank the caller's elapsed milliseconds onto the CONSUMED render tick, clamped
//     strictly below the update side's banked credit (dword_8324E524 vs dword_8324E520;
//     the console commits the new value with an interrupt-masked lwarx/stwcx. --
//     single-threaded here, a plain store reproduces the observable state);
//   * get the root render item (Render_GetRoot prunes expired roots + resolves the
//     newest renderable revision for the tick), and if present kick
//     AptDecoupleTreeTraversal(root, tick, ctx, 0, 0, layerMask, 1);
//   * pop the clip stack (--word_8324E390).
// The console reads the render context from dword_8324E2AC (this TU's GetRenderContext).
// ---------------------------------------------------------------------------
static void AptRenderInternal(int nElapsedMs, int nLayerMask)
{
    AptTarget* lpTarget = gpAptTargetTLS;
    if (lpTarget == nullptr)
        return;
    // The render-enabled gate: the console checks *(off_8324E578 + 0x18) (the animation target).
    if (lpTarget->mpAnimationTarget == nullptr)
        return;

    // Drain the VM deferred-release queue (dword_8324E508 / off_8324E2C8 under unk_8324E728):
    // each parked font unit goes back through the host free thunk, its slot zeroed. The
    // single-threaded PC unresolve releases directly, so the queue is normally empty.
    if (gAptDeferredReleaseCount != 0)
    {
        EA::Thread::Mutex_Lock(gAptUnresolveMutex, gAptUnresolveMutexName);
        void** lppQueue = static_cast<void**>(gAptDeferredReleaseQueue);
        for (int liIndex = 0; liIndex < gAptDeferredReleaseCount; ++liIndex)
        {
            AptFreeFontUnit(lppQueue[liIndex]);
            lppQueue[liIndex] = nullptr;
        }
        gAptDeferredReleaseCount = 0;
        EA::Thread::Mutex_Unlock(gAptUnresolveMutex);
    }

    // Flush the render-manager teardown queue only when it holds deferred cells (the X360
    // gates the Clean call on the dword_8324E7D8 head word).
    if (gAptRenderManagerQueue.mpHead != nullptr)
        gAptRenderManagerQueue.Clean();

    // Push a fresh identity/full-screen clip for this pass (X360 ++word_8324E390 then the
    // unit store == ClipStackPush + ClipStackMakeUnit; popped after the walk).
    AptMath::ClipStackPush();
    AptMath::ClipStackMakeUnit();

    // Consume the elapsed milliseconds against the update side's banked tick credit: the
    // render tick may never reach the update tick (strictly below), so items committed
    // this update become renderable once the render bank catches up (the decoupled pair).
    int nTick = gnCurrRenderTickConsumed + nElapsedMs;
    if (nTick - gnCurrUpdateTick >= 0)
        nTick = gnCurrUpdateTick - 1;
    gnCurrRenderTickConsumed = nTick;   // console: interrupt-masked lwarx/stwcx. commit

    // The render-root chain head lives in the target's anchor slot (mppRenderRootAnchor).
    AptRenderTreeManager::_AptRenderItemRootList** ppHead = lpTarget->mppRenderRootAnchor;
    if (ppHead != nullptr)
    {
        AptRenderTreeManager* lpMgr =
            reinterpret_cast<AptRenderTreeManager*>(lpTarget->mppRenderRootAnchor);
        AptRenderItem* lpRoot = lpMgr->Render_GetRoot(ppHead, nTick);

        if (lpRoot != nullptr)
            AptDecoupleTreeTraversal(lpRoot, nTick, GetRenderContext(), NormalOp(), 0, nLayerMask, 1);
    }

    AptMath::ClipStackPop();
}

// ---------------------------------------------------------------------------
// AptRender @0x82AF33E8 -- the public render entry for the CURRENT target: store the
// render-side current target (off_8324E578) into the TLS mirror (unk_8324E814), then
// run the internal walk with the caller's elapsed milliseconds + layer mask.
// ---------------------------------------------------------------------------
void AptRender(int nElapsedMs, int nLayerMask)
{
    gAptTargetTls.SetValue(gpAptTargetTLS);
    AptRenderInternal(nElapsedMs, nLayerMask);
}

// ---------------------------------------------------------------------------
// AptRenderTarget @0x82AF4ED0 -- run AptRender with pTarget swapped in as the render-side
// current context (off_8324E578 + the TLS mirror), restoring the previous context after.
// The X360 body is un-exported; the PS3 External ELF exports the full body
// (_Z15AptRenderTargetPvj13AptAnimLevelE @0x7FBFCC) and the X360 span (0x82AF4ED0..
// 0x82AF4F30, whose only outbound calls are the TLS SetValue + AptRender this body makes)
// matches it -- the exact mirror of AptUpdateTarget @0x82B0DE80 on the render side.
// ---------------------------------------------------------------------------
void AptRenderTarget(AptTarget* pTarget, int nElapsedMs, int nLevelMask)
{
    AptTarget* lpPrevTarget = gpAptTargetTLS;
    gpAptTargetTLS = pTarget;
    gAptTargetTls.SetValue(pTarget);

    AptRender(nElapsedMs, nLevelMask);

    gpAptTargetTLS = lpPrevTarget;
    gAptTargetTls.SetValue(lpPrevTarget);
}
