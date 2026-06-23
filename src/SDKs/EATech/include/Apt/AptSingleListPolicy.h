#pragma once

// ===========================================================================
// EATech Apt -- AptSingleListPolicy<T> (single-linked-list container policy).
//
// IDA names this TU `AptSingleListPolicy>` -- the trailing '>' is a truncated
// template-argument close (`AptSingleListPolicy<T>`). There is no leak/DWARF for
// it. SHAPE is reconstructed STRICTLY from the X360 ARTIST.XEX pseudocode/asm of
// the three owned methods plus their call sites:
//     AptSingleListPolicy<T>::PushFront @ 0x82AEC3A0  (AptLoader::Load)
//     AptSingleListPolicy<T>::PopFront  @ 0x82AEC418  (AptLoader::Invalidate, ~AptLoader)
//     AptSingleListPolicy<T>::Erase     @ 0x82AEC470  (AptLoader::~AptLoader)
//
// All three callers are AptLoader list-management paths, so this is the loader's
// intrusive-by-allocation forward list: a bare head-pointer cell whose nodes are
// 8-byte {payload, next} records pool-allocated from the shared Apt DOGMA pool
// (off_8324D808). The payload the X360 stores/compares is a single 32-bit word
// (slwi-free dword copy at node+0; `T = void*` for the loaded-item pointer this
// list tracks). Modelled as a policy template so the payload type stays explicit
// without fabricating AptLoader's element identity (AptLoader is owned by another
// TU, not yet homed).
//
// LAYOUT (proven by the asm offsets):
//   Container : just a Node* head cell. The methods take `&head` (the X360 `a1`
//               points at the head-pointer slot, *a1 == first node).
//   Node (8B) : +0x00 mData   (the payload word, T)
//               +0x04 mpNext  (next Node*, or null at the tail)
//
// This is vendor/SDK code reconstructed in its canonical Apt home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifier (AptSingleListPolicy, PushFront/
// PopFront/Erase) is kept verbatim; reconstructed members follow the mX prefixes.
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager

// ---------------------------------------------------------------------------
// FLAG (un-homed, owned by the Apt allocator boot TU): the shared DOGMA pool the
// node allocations come from. The X360 loads it from off_8324D808 in all three
// methods. Declared as an extern so this TU compiles + links; the single
// underlying pool object is shared with the other Apt families (the sibling Apt
// headers declare the same off_8324D808 slot under their own names).
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptSingleListPool;   // off_8324D808

template <typename T>
class AptSingleListPolicy
{
public:

    // The 8-byte pool-allocated link record. mData @ +0x00, mpNext @ +0x04.
    struct Node
    {
        T     mData;    // +0x00 payload word
        Node* mpNext;   // +0x04 forward link
    };

    AptSingleListPolicy()
        : mpHead(0)
    {
    }

    // PushFront @ 0x82AEC3A0 -- allocate an 8-byte node, copy the payload (*a2)
    // into it, then link it as the new head. X360 store order: node->mData =
    // *pValue; node->mpNext = 0; node->mpNext = mpHead; mpHead = node. (The two
    // mpNext writes are preserved: the alloc path zeroes mpNext, then it is
    // overwritten with the old head outside the alloc-success branch.)
    Node* PushFront(const T* pValue)
    {
        Node* lpNode = static_cast<Node*>(gpAptSingleListPool->Allocate(sizeof(Node)));
        if (lpNode)
        {
            lpNode->mData  = *pValue;
            lpNode->mpNext = 0;
        }
        lpNode->mpNext = mpHead;
        mpHead = lpNode;
        return lpNode;
    }

    // PopFront @ 0x82AEC418 -- if the list is non-empty, unlink and free the head
    // node, advancing mpHead to its successor. Returns the (X360 r3) Deallocate
    // result / unchanged head pointer when empty.
    void PopFront()
    {
        Node* lpFirst = mpHead;
        if (lpFirst)
        {
            Node* lpNext = lpFirst->mpNext;
            gpAptSingleListPool->Deallocate(lpFirst, sizeof(Node));
            mpHead = lpNext;
        }
    }

    // Erase @ 0x82AEC470 -- remove a specific node (pTarget points at the node
    // pointer to erase). If it is the head, this is PopFront. Otherwise walk to
    // the target's predecessor, unlink the target, and free it.
    //
    // X360: r9 = *pTarget (the node to erase), r11 = *pHead (first node).
    //   if (*pTarget == *pHead) return PopFront();
    //   pred = first;
    //   while (pred->mpNext != *pTarget && pred->mpNext != 0) pred = pred->mpNext;
    //   victim = pred->mpNext;
    //   if (victim) pred->mpNext = victim->mpNext;
    //   Deallocate(victim, 8);
    void Erase(Node* const* pTarget)
    {
        Node* lpTarget = *pTarget;
        if (lpTarget == mpHead)
        {
            PopFront();
            return;
        }

        Node* lpPred = mpHead;
        if (lpPred)
        {
            for (;;)
            {
                Node* lpNext = lpPred->mpNext;
                if (lpNext == lpTarget || !lpNext)
                    break;
                lpPred = lpNext;
            }
        }

        Node* lpVictim = lpPred->mpNext;
        if (lpVictim)
            lpPred->mpNext = lpVictim->mpNext;
        gpAptSingleListPool->Deallocate(lpVictim, sizeof(Node));
    }

    Node* mpHead;   // the head-pointer cell (the X360 container == &mpHead)
};
