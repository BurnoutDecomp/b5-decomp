#pragma once

// ===========================================================================
// EATech Apt -- AptRenderTreeManager: the double-buffered render scene graph.
//
// It separates the UPDATE thread (which mutates the tree each tick -- create/
// move/insert/remove items, building a new revision) from the RENDER thread
// (which walks a stable revision). The Update_* entry points are what the scene
// nodes (AptCharacterInst/AptCIH) call; the Render_* entry points feed the
// renderer. Most of the real work is in AptRenderItem's Manager_* methods (the
// per-item revision/link machinery); this is a thin facade over them.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (20AptRenderTreeManager). The
// Update_* side is stateless delegation to the items, so it is reconstructed
// fully; the Render_* side + the concurrent double-buffering (revision cloning
// under a spin-lock) couple to the async render thread and are FLAG'd / deferred
// -- on the single-threaded bring-up path GetTickItemWritable returns the live
// item (single-buffer), which is correct without a concurrent reader.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"

struct AptCharacter;
struct AptCIH;

struct AptRenderTreeManager
{
    // -----------------------------------------------------------------------
    // NESTED: _AptRenderItemRootList -- one cell of the render-root chain.
    //
    // The render-tree manager keeps, per AptTarget, a singly-linked list of the
    // root render items currently live in the scene. Each cell is an 8-byte
    // record pulled from the shared Apt fixed-size DOGMA pool (X360 off_8324D808
    // == gpAptRenderManagerPool); the chain head lives in AptTarget's anchor
    // cell (the `*(target+44)` that AptRenderManagerQueue::Add defers, and that
    // AptRenderManagerItem holds as mppRootListHead).
    //
    // SHAPE reconstructed STRICTLY from the X360 ARTIST.XEX asm of the three
    // owned methods (there is no Feb-2007 source and no DWARF for this nested
    // type). Per the project x64 semantic-parity rule the cell is modelled by
    // NAMED members, not byte offsets (PC pointers are 8 bytes, so the PC layout
    // legitimately differs from the console's two dwords); the X360 offsets are
    // recorded in comments only:
    //   +0x00  mpItem   the root AptRenderItem this cell anchors (ref-counted;
    //                    released + zeroed before the cell is freed).      (*v)
    //   +0x04  mpNext   the next cell in the chain (null == tail).         (v[1])
    // -----------------------------------------------------------------------
    struct _AptRenderItemRootList
    {
        AptRenderItem*         mpItem;   // +0x00  (*v)
        _AptRenderItemRootList* mpNext;  // +0x04  (v[1])

        // Shutdown @ 0x82AE19E0 -- free the ENTIRE chain rooted at pHead: for
        // each cell, release its item reference (zeroing the slot first) and
        // return the 8-byte cell to the shared Apt pool. Static: the X360 takes
        // the head node directly in r3 (no `this`), and it is what
        // AptRenderManagerItem::Clean / AptTarget::Shutdown call as
        // _AptRenderItemRootList::Shutdown(*anchorCell). The X360 "returns" the
        // trailing Deallocate result (r3), which no caller uses -> void.
        static void Shutdown(_AptRenderItemRootList* pHead);

        // Get @ 0x82AE1A38 -- PRUNE expired roots from the chain whose anchor is
        // `this`, up to the first still-live root, and return the surviving cell.
        // A look-ahead cell's root is expired when (nTick - root->mCreatedOnTick)
        // >= 0 AND the root is NOT flagged the writable revision (mFlags bit 25);
        // each expired cell's item is released and the cell freed, then the walk
        // advances. (Called by AptRenderTreeManager::Render_GetRoot.)
        _AptRenderItemRootList* Get(int nTick);

        // InsertNewRoot @ 0x82AE1AC8 -- append a new chain cell for pItem at the
        // tail (allocating it from the shared Apt pool) and atomically take a
        // reference on pItem, unless this anchor's first slot already holds pItem
        // (then it is a no-op). Returns this anchor cell (the X360 returns r3).
        _AptRenderItemRootList* InsertNewRoot(AptRenderItem* pItem);
    };

    // FLAG: the root-revision list + the double-buffer state (used by the Render_*
    // walk). Minimal here -- the Update_* facade below is stateless.
    void* mpRootList;

    // ---- UPDATE side (called by AptCharacterInst / AptCIH) -----------------
    // @0x814254 -- create the render item for a character.
    AptRenderItem* Update_CreateItem(AptCharacter* pCharacter, int nTick)
    {
        return AptRenderItem::Manager_CreateItem(pCharacter, nTick);
    }

    // @0x7EC8E4 -- the writable render item for this tick. FLAG: when the item is
    // from a prior tick the console clones a new revision (Manager_CreateNewRevision,
    // under a spin-lock); single-buffered here -> return the live item.
    AptRenderItem* Update_GetTickItemWritable(const AptRenderItem* pItem, int nTick)
    {
        return const_cast<AptRenderItem*>(pItem);   // (IsWritableForThisTick(nTick) is true in single-buffer)
    }

    // @0x7E4990 -- mark an item removed.
    void Update_ItemRemoved(AptRenderItem* pItem, int /*nTick*/)
    {
        pItem->Manager_SetDeletionMark(true);
    }

    // Display-list change notifications (called by AptDisplayListState as the
    // per-frame display list mutates). They re-derive the render tree's
    // first-child / next-sibling / root links from the changed scene node.
    // FLAG: the render-tree-link re-derivation is the concurrent double-buffering
    // propagation -- deferred; no-op on the single-buffer bring-up path, where the
    // display-list links (in the AptCIHs) are themselves the source of truth.
    //   @0x7EDF6C / 0x7ED694 / 0x7F11D4 / 0x7F0524
    void Update_ItemFirstChildChanged(AptCIH* /*pCIH*/)  {}
    void Update_ItemNextSiblingChanged(AptCIH* /*pCIH*/) {}
    void Update_ItemInserted(AptCIH* /*pCIH*/, int /*nTick*/ = 0) {}
    void Update_SetRootItem(AptCIH* /*pCIH*/, int /*nTick*/ = 0) {}
};
