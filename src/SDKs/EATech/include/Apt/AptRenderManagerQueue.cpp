// ===========================================================================
// EATech Apt -- AptRenderManagerQueue: the render-manager draw queue.
//
// Reconstructed STRICTLY from the X360 BURNOUT_X360_ARTIST.XEX pseudocode + asm:
//     AptRenderManagerQueue::Add      @ 0x82AE5700
//     AptRenderManagerQueue::Clean    @ 0x82AEFB48
//     AptRenderManagerQueue::IsEmpty  @ 0x82AD5780
//
// The nodes come from the shared Apt fixed-size DOGMA pool (X360 off_8324D808 ==
// gpAptRenderManagerPool, the same pool AptSharedPtr / AptSingleListPolicy / the
// pseudo-display-list nodes draw from). Both mutating ops bracket their list work
// in an interrupt-masking spin lock (X360 unk_8324E6E4): the console emits the
// classic mfmsr/mtmsree/lwarx/stwcx. test-and-set acquire and store-0 release.
// Modelled portably with DOGMA_SpinLock (the same lock abstraction DogmaAllocator
// uses for its three free-list locks).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"            // DOGMA_PoolManager + DOGMA_SpinLock
// gpAptRenderManagerPool (off_8324D808) comes from AptRenderManagerItem.h (pulled
// in via AptRenderManagerQueue.h) -- the render-manager family shares one alias of
// the shared Apt pool with its queued nodes.

// The render-manager-queue spin lock (X360 global unk_8324E6E4). One lock guards
// every Add/Clean mutation of the head/tail list; defined here as the single
// shared instance (mirrors DogmaAllocator.cpp's per-free-list locks).
static DOGMA_SpinLock gAptRenderManagerQueueLock;   // unk_8324E6E4

// ---------------------------------------------------------------------------
// Add @ 0x82AE5700
//
// Allocate an 8-byte AptRenderManagerItem from the shared Apt pool, store the
// deferred render-root anchor cell in it, and append it at the tail of the queue
// (O(1) via mpTail). The whole mutation is under the queue spin lock. Returns the
// new node (the X360 returns the raw allocation in r3 -- null when the pool is
// exhausted; the link still runs in that case, exactly as the console does).
// ---------------------------------------------------------------------------
AptRenderManagerItem* AptRenderManagerQueue::Add(AptRenderTreeManager::_AptRenderItemRootList** ppRootListHead)
{
    gAptRenderManagerQueueLock.Lock();

    AptRenderManagerItem* lpNode =
        static_cast<AptRenderManagerItem*>(gpAptRenderManagerPool->Allocate(sizeof(AptRenderManagerItem)));
    if (lpNode)
    {
        lpNode->mppRootListHead = ppRootListHead;   // result[1] = a2  (+0x04)
        lpNode->mpNext          = nullptr;          // *result   = 0   (+0x00)
    }

    // Tail-append. Empty list -> the node becomes the head; otherwise link it
    // after the current tail. Either way it is the new tail. (X360: when alloc
    // failed lpNode == null and a null node is appended, matching the asm, which
    // does not guard the link on the allocation succeeding.)
    if (mpHead)
        mpTail->mpNext = lpNode;
    else
        mpHead = lpNode;
    mpTail = lpNode;

    gAptRenderManagerQueueLock.Unlock();
    return lpNode;
}

// ---------------------------------------------------------------------------
// Clean @ 0x82AEFB48
//
// Detach the entire list (head/tail cleared up front, under the lock), then walk
// it Clean()-ing each node and returning it to the pool. The successor is read
// (node->mpNext) before Clean()/free so the freed node is never re-read.
// ---------------------------------------------------------------------------
void AptRenderManagerQueue::Clean()
{
    gAptRenderManagerQueueLock.Lock();

    AptRenderManagerItem* lpNode = mpHead;   // v8 = *result
    mpHead = nullptr;                        // *result     = 0
    mpTail = nullptr;                        // *(result+4) = 0

    while (lpNode)
    {
        AptRenderManagerItem* lpNext = lpNode->mpNext;   // v9 = *v8 (read next first)
        lpNode->Clean();                                 // AptRenderManagerItem::Clean(v8)
        gpAptRenderManagerPool->Deallocate(lpNode, sizeof(AptRenderManagerItem));  // free 8 bytes
        lpNode = lpNext;
    }

    gAptRenderManagerQueueLock.Unlock();
}

// ---------------------------------------------------------------------------
// IsEmpty @ 0x82AD5780 -- the queue is empty when it has no head node.
// (X360: lwz mpHead; cntlzw; extrwi -> mpHead == 0.)
// ---------------------------------------------------------------------------
bool AptRenderManagerQueue::IsEmpty() const
{
    return mpHead == nullptr;
}
