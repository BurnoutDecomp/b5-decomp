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
