// ===========================================================================
// EATech Apt -- AptRenderManagerItem: a node in the render-manager deferred
// render-root-teardown queue.   Reconstructed STRICTLY from the X360
// BURNOUT_X360_ARTIST.XEX pseudocode + asm:
//     AptRenderManagerItem::SetNext @ 0x82AD5778
//     AptRenderManagerItem::Clean   @ 0x82AE56B0
//
// The node holds a deferred render-root ANCHOR CELL (mppRootListHead): a pointer
// to a 4-byte cell whose contents are the head of an
// AptRenderTreeManager::_AptRenderItemRootList chain. Clean() reproduces exactly
// the inline teardown the producer (AptTarget::Shutdown @0x82B02328) performs when
// it runs ON the render thread -- shut the chain down, zero the cell, free the
// cell back to the shared Apt pool -- which is deferred to here when the shutdown
// happens off the render thread.
//
// The node block (8 bytes on the console) is freed by the OWNER
// (AptRenderManagerQueue::Clean) after this returns; Clean() only releases the
// anchor it carries. EA SDK identifiers kept verbatim.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h"
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"   // _AptRenderItemRootList::Shutdown
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager::Deallocate

// ---------------------------------------------------------------------------
// SetNext @ 0x82AD5778
//
//   stw r4, 0(r3)   ; mpNext = pNext
//   blr             ; return r3 (this)
//
// A chainable setter: store the forward link and return `this` (the X360 returns
// r3 unchanged), exactly like AptRenderItem::SetDepth/SetClipDepth.
// ---------------------------------------------------------------------------
AptRenderManagerItem* AptRenderManagerItem::SetNext(AptRenderManagerItem* pNext)
{
    mpNext = pNext;
    return this;
}

// ---------------------------------------------------------------------------
// Clean @ 0x82AE56B0
//
//   r30 = mppRootListHead            ; lwz r30, 4(r31)
//   if (*mppRootListHead)            ; lwz r3, 0(r30); cmplwi; beq
//   {
//       _AptRenderItemRootList::Shutdown(*mppRootListHead);  ; bl ...Shutdown
//       *mppRootListHead = nullptr;                          ; stw 0, 0(r30)
//   }
//   gpAptRenderManagerPool->Deallocate(mppRootListHead, 4);  ; Deallocate(pool, r30, 4)
//   mppRootListHead = nullptr;       ; stw 0, 4(r31)
//   mpNext          = nullptr;       ; stw 0, 0(r31)
//
// Releases the deferred anchor only (the queue frees this 8-byte node itself
// afterwards). The trailing Deallocate result the X360 leaves in r3 is never used
// by either caller (AptRenderManagerQueue::Clean / the AptTarget inline twin), so
// this is logically void.
// ---------------------------------------------------------------------------
void AptRenderManagerItem::Clean()
{
    // Tear the render-root chain down (if the anchor still holds a live head),
    // then clear the cell -- the same work AptTarget::Shutdown does inline on the
    // render thread.
    if (*mppRootListHead)
    {
        AptRenderTreeManager::_AptRenderItemRootList::Shutdown(*mppRootListHead);
        *mppRootListHead = nullptr;
    }

    // Free the 4-byte anchor cell itself back to the shared Apt pool (X360: the
    // literal size 4 == one console pointer; modelled portably as the pointer's
    // PC width so it matches however AptTarget allocated the cell).
    gpAptRenderManagerPool->Deallocate(
        mppRootListHead, sizeof(AptRenderTreeManager::_AptRenderItemRootList*));

    mppRootListHead = nullptr;
    mpNext          = nullptr;
}
