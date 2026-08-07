#pragma once

// ===========================================================================
// EATech Apt -- AptRenderManagerItem: one node in the AptRenderManagerQueue.
//
// The render-manager queue is the DEFERRED render-root-teardown worklist. When an
// AptTarget is shut down off the render thread (AptTarget::Shutdown @0x82B02328),
// it cannot free its render-root anchor cell directly (the render thread may still
// be walking the tree), so it hands the anchor to AptRenderManagerQueue::Add. The
// render thread later flushes the queue (AptRenderManagerQueue::Clean), which
// AptRenderManagerItem::Clean()s each node -- performing exactly the teardown the
// on-render-thread branch of AptTarget::Shutdown does inline.
//
// SHAPE reconstructed STRICTLY from the X360 ARTIST.XEX asm of the two owned
// methods + their producer (AptTarget::Shutdown) -- there is no Feb-2007 source
// and no DWARF for this TU:
//     AptRenderManagerItem::SetNext @ 0x82AD5778  (AptRenderManagerQueue::Add)
//     AptRenderManagerItem::Clean   @ 0x82AE56B0  (AptRenderManagerQueue::Clean)
//
// LAYOUT (8-byte pool block; proven by the asm offsets -- the node is allocated
// from the shared Apt fixed-size DOGMA pool, off_8324D808, by ...Queue::Add):
//   +0x00  mpNext           the next queued node (SetNext writes this; the queue
//                           drain reads it).                                  [0]
//   +0x04  mppRootListHead  the deferred render-root ANCHOR CELL: a pointer to a
//                           separately pool-allocated 4-byte cell that holds the
//                           head of an AptRenderTreeManager::_AptRenderItemRootList
//                           chain. (This is AptTarget's `*(target+44)` passed
//                           verbatim into ...Queue::Add as the payload.)       [1]
//
// AptRenderManagerItem::Clean reproduces AptTarget::Shutdown's inline teardown of
// that cell: if *cell holds a live list, _AptRenderItemRootList::Shutdown() frees
// the chain and *cell is zeroed; then the 4-byte cell itself is freed back to the
// pool. (member [1] is a pointer-TO-pointer because the queue node owns the cell,
// not the list head directly -- both the chain and the cell are released.)
//
// This is vendor/SDK code reconstructed in its canonical Apt home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptRenderManagerItem, SetNext,
// Clean) are an external/middleware API and kept verbatim; reconstructed members
// follow the mX prefixes.
// ===========================================================================

#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"   // _AptRenderItemRootList(::Shutdown)

// ---------------------------------------------------------------------------
// The shared fixed-size Apt DOGMA pool the queue nodes (and the root-list nodes
// + anchor cells) come from. The X360 loads it from off_8324D808 in both owned
// bodies. DEFINED in AptGlobals.cpp and installed by AptCommonInitialize
// (AptInit.cpp:261); the single underlying pool object is shared with the other
// Apt families (the sibling Apt headers declare the same off_8324D808 slot under
// their own names -- gpAptSharedPtrPool / gpAptSingleListPool /
// gpAptPseudoDataPool / ...).
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptRenderManagerPool;   // off_8324D808

struct AptRenderManagerItem
{
    AptRenderManagerItem*                          mpNext;          // +0x00 [0]
    AptRenderTreeManager::_AptRenderItemRootList** mppRootListHead; // +0x04 [1]

    // SetNext @ 0x82AD5778 -- set the forward link. Returns `this` (the X360
    // leaves r3 unchanged), matching the chainable-setter idiom used across the
    // Apt render-item API (AptRenderItem::SetDepth/SetClipDepth/...).
    AptRenderManagerItem* SetNext(AptRenderManagerItem* pNext);

    // Clean @ 0x82AE56B0 -- tear this node's deferred render-root anchor down
    // (does NOT free the node itself; the owning queue frees the 8-byte block
    // afterwards). Shuts the root-list chain down, zeroes + frees the anchor cell,
    // then clears both members. The X360 "returns" the trailing Deallocate result,
    // which no caller uses -> void.
    void Clean();
};
