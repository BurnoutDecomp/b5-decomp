#pragma once

// ===========================================================================
// EATech Apt -- AptRenderManagerQueue: the render-manager draw queue.
//
// A thread-safe singly-linked FIFO of AptRenderManagerItem nodes. The render
// manager appends queued render items at the tail (Add) and drains the whole
// queue (Clean), cleaning + freeing each node. Nodes are 8-byte records pulled
// from the shared Apt fixed-size DOGMA pool (X360 off_8324D808). Every mutation
// is bracketed by an interrupt-masking spin lock (X360 global unk_8324E6E4),
// because the queue is shared between the producer and the drain path.
//
// NO DWARF / Feb-2007 source exists for this class. All shape below is derived
// STRICTLY from the X360 BURNOUT_X360_ARTIST.XEX disassembly:
//     AptRenderManagerQueue::Add      @ 0x82AE5700   (AptTarget::Shutdown)
//     AptRenderManagerQueue::Clean    @ 0x82AEFB48   (sub_82AF3278)
//     AptRenderManagerQueue::IsEmpty  @ 0x82AD5780
//
// LAYOUT (the queue control block; proven by the asm offsets). Per the project
// x64 semantic-parity rule the queue is modelled by NAMED MEMBERS, not byte
// offsets (PC pointers are 8 bytes, so the PC layout legitimately differs from
// the console's two dwords); the X360 offsets are recorded in comments only:
//     +0x00 mpHead   first node to drain (null == empty)   (a1[0] / *a1)
//     +0x04 mpTail   last node (append point)              (a1[1])
//
//   Add  : tail-append -- if empty, mpHead = node; else mpTail->mpNext = node;
//          either way mpTail = node.
//   Clean: detach the whole list (mpHead = mpTail = 0), then for each node call
//          AptRenderManagerItem::Clean() and return it to the pool.
//   IsEmpty: mpHead == nullptr.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h"   // the queued node

struct AptRenderManagerQueue
{
    AptRenderManagerItem* mpHead;   // +0x00 first node (null when empty)
    AptRenderManagerItem* mpTail;   // +0x04 last node (append point)

    // Add @0x82AE5700 -- allocate a node from the shared Apt pool, store the
    // deferred render-root ANCHOR CELL in it, and append it at the tail. Returns
    // the new node (the X360 returns the allocation in r3; null when the pool is
    // exhausted). The payload is AptTarget::Shutdown's `*(target+44)` anchor cell
    // (a _AptRenderItemRootList** -- see AptRenderManagerItem.h), NOT a render
    // item: the queue defers that cell's teardown to AptRenderManagerItem::Clean.
    AptRenderManagerItem* Add(AptRenderTreeManager::_AptRenderItemRootList** ppRootListHead);

    // Clean @0x82AEFB48 -- detach and drain the whole queue: Clean() each node
    // then free it back to the pool, leaving the queue empty.
    void Clean();

    // IsEmpty @0x82AD5780 -- true when there is no head node.
    bool IsEmpty() const;
};
