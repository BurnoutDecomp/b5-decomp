#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsContainers::BaseLinkedList - the non-template base of the intrusive doubly-
// linked list family (LinkedList<T>, LinkedListHelper<T,N>). It owns the head /
// tail / count state and all of the pointer surgery; the templated LinkedList<T>
// wrapper (declared in the DWARF but inlined at every call site, so not emitted
// here) just casts BaseLinkedListNode* to/from LinkedListNode<T>*.
//
// LAYOUT (DecFIGS DWARF, CgsLinkedList.h):
//   BaseLinkedListNode { +0 mpNext, +4 mpPrev }                       (protected)
//   BaseLinkedList     { +0 mpFirst, +4 mpLast, +8 miCount }          (protected)
//
// miCount carries the sentinel 0x7FFFFFFF before InternalInit() runs; every
// accessor asserts against it ("BaseLinkedList accessed when uninitialised").
//
// DECOMPILED from the X360 build (BURNOUT_X360_ARTIST.XEX). Bodies are in
// CgsLinkedList.cpp; the byte addresses are 0x821F0E48 (CountElements) and the
// 0x82814CE8..0x82815708 block (the Internal* surgery). InternalInit walks the
// caller's node array by a BYTE stride (liStride) because the real nodes are
// LinkedListNode<T> subclasses larger than BaseLinkedListNode.

namespace CgsContainers
{
    struct BaseLinkedList;

    // CgsLinkedList.h:56 - one intrusive link. The next/prev pointers are
    // protected; BaseLinkedList is a friend so its Internal* surgery can relink
    // nodes without exposing the pointers publicly.
    struct BaseLinkedListNode
    {
        friend struct BaseLinkedList;

    public:
        // CgsLinkedList.h:64 (DWARF). Public next accessor used to walk a sublist by hand.
        // The X360 LinkedListHelper iteration (e.g. ResourceRegistrar::UpdateRequests
        // @0x826B0560 / UpdateQueued @0x82702108 / GetResource @0x826B0B08) walks the live
        // list by chasing each node's mpNext directly (Hex-Rays `Head = *Head`); this exposes
        // that load without un-protecting the raw pointer pair.
        BaseLinkedListNode* GetNextNode() const { return mpNext; }

    protected:
        // CgsLinkedList.h:60
        BaseLinkedListNode* mpNext;
        // CgsLinkedList.h:61
        BaseLinkedListNode* mpPrev;
    };

    // CgsLinkedList.h:75
    struct BaseLinkedList
    {
    public:
        // CgsLinkedList.h:78 - start uninitialised (miCount == sentinel).
        BaseLinkedList()
            : mpFirst(0)
            , mpLast(0)
            , miCount(KI_UNINITIALISED)
        {
        }

        // CgsLinkedList.h:82 - cached live element count (0x821F0E48).
        s32 CountElements() const;

        // CgsLinkedList.h:85
        bool IsEmpty() const { return miCount == 0; }

    protected:
        // CgsLinkedList.h:97
        void InternalInit(BaseLinkedListNode* lpNodes, s32 liNodeCount, s32 liStride);

        // CgsLinkedList.h:100
        BaseLinkedListNode* InternalGetHead() const;

        // CgsLinkedList.h:103
        BaseLinkedListNode* InternalGetTail() const;

        // CgsLinkedList.h:107
        void InternalAddHead(BaseLinkedListNode* lpNode);

        // CgsLinkedList.h:111
        void InternalAddTail(BaseLinkedListNode* lpNode);

        // CgsLinkedList.h:116 - declared-only here; out-of-line body owned by another TU.
        void InternalAddAfter(BaseLinkedListNode* lpNodeInList, BaseLinkedListNode* lpNewNode);

        // CgsLinkedList.h:121
        void InternalAddBefore(BaseLinkedListNode* lpNodeInList, BaseLinkedListNode* lpNewNode);

        // CgsLinkedList.h:124
        BaseLinkedListNode* InternalRemoveHead();

        // CgsLinkedList.h:127 - declared-only here; out-of-line body owned by another TU.
        BaseLinkedListNode* InternalRemoveTail();

        // CgsLinkedList.h:131
        void InternalRemoveNode(BaseLinkedListNode* lpNode);

        // CgsLinkedList.h:135
        s32 InternalGetNodeIndex(BaseLinkedListNode* lpNode);

        // CgsLinkedList.h:89
        BaseLinkedListNode* mpFirst;
        // CgsLinkedList.h:90
        BaseLinkedListNode* mpLast;
        // CgsLinkedList.h:91
        s32 miCount;

    private:
        // Sentinel stored in miCount until InternalInit() runs.
        static const s32 KI_UNINITIALISED = 0x7FFFFFFF;
    };

    // CgsLinkedList.h - one pooled list node: the two intrusive link pointers (inherited
    // from BaseLinkedListNode at +0/+4) followed by the element payload at +8. The X360
    // AddTail body (below) writes the value straight into this +8 slot.
    template <typename T>
    struct LinkedListNode : public BaseLinkedListNode
    {
        // +8 - element payload (immediately after mpNext/mpPrev).
        T mData;
    };

    // CgsLinkedList.h - LinkedListHelper<T, N> owns a fixed pool of N LinkedListNode<T>
    // plus two BaseLinkedList sublists: a free list (nodes available to hand out) and a
    // live list (nodes currently in use). This is the pointer-based sibling of
    // IndexedLinkedList<T, Index>; the X360 emits AddTail out of line for some T while
    // inlining the rest, so the generic body lives here and covers every instantiation.
    //
    // LAYOUT (recovered from the X360 AddTail @ 0x826A5E88, a BrnSound::Logic::Resource-
    // Registrar instantiation): the free BaseLinkedList sits at +0xC4 and the live one at
    // +0xD0 relative to the owning object; here they are ordered maFreeList then maLiveList
    // so the same +0xC4/+0xD0 spacing falls out of the node pool that precedes them.
    template <typename T, u32 N>
    class LinkedListHelper
    {
    public:
        typedef LinkedListNode<T> Node;

        // Seed the two sublists from the embedded node pool: the free list owns every node,
        // the live list starts empty. Generic body shared by every instantiation; recovered
        // from the X360 ResourceRegistrar::Construct (0x826B0470), which drives a pair of
        // InternalInit calls per helper -- e.g. for the loading-queued list:
        //   InternalInit( free, &maNodePool[0], miNumNodes(==N), sizeof(Node)/*336*/ );
        //   InternalInit( live, 0, 0, sizeof(Node) );
        // The X360 reads the count from a stored member (`*a1`); here it is the template
        // parameter N (kept also as miNumNodes below for the faithful member sequence).
        // InternalInit chains the pool by BYTE stride sizeof(Node) (matching the X360 336/316
        // strides for the QueuedResource/RequestedResource pools).
        void Construct()
        {
            miNumNodes = static_cast<s32>(N);
            maFreeList.InternalInit(maNodePool, static_cast<s32>(N), static_cast<s32>(sizeof(Node)));
            maLiveList.InternalInit(0, 0, static_cast<s32>(sizeof(Node)));
        }

        // Append a value to the live list. Generic body shared by every instantiation
        // (X360 0x826A5E88 = a BrnSound::Logic::ResourceRegistrar AddTail, CgsLinkedList.h:307):
        // pull a node off the free list, assert the pool was not exhausted, copy the value
        // into the node's payload, then chain the node onto the tail of the live list.
        Node* AddTail(const T& lrElement)
        {
            Node* lpNode = static_cast<Node*>(maFreeList.InternalRemoveHead());
            CGS_ASSERT(lpNode != 0, "We've run out of nodes.");
            lpNode->mData = lrElement;
            maLiveList.InternalAddTail(lpNode);
            return lpNode;
        }

        // Push a value at the HEAD of the live list (the X360 ClearUnreferancedFiles path
        // appends recycled unloading items via AddHead, asserting the pool was not exhausted:
        // "mUnLoadingQueuedResourceList.AddHead( lUnloadingItem )" @ BrnResourceRegistrar.cpp:320).
        // Pull a node off the free list, copy the payload, chain onto the live-list head.
        Node* AddHead(const T& lrElement)
        {
            Node* lpNode = static_cast<Node*>(maFreeList.InternalRemoveHead());
            CGS_ASSERT(lpNode != 0, "We've run out of nodes.");
            lpNode->mData = lrElement;
            maLiveList.InternalAddHead(lpNode);
            return lpNode;
        }

        // First live node (0 when empty). Callers chase node->GetNextNode() to walk the list
        // (the X360 InternalGetHead + `Head = *Head` iteration shape).
        Node*       GetHead()       { return static_cast<Node*>(maLiveList.InternalGetHead()); }
        const Node* GetHead() const { return static_cast<const Node*>(maLiveList.InternalGetHead()); }

        // Cached live-element count (BaseLinkedList::CountElements @ 0x821F0E48 on the live list).
        s32 CountElements() const { return maLiveList.CountElements(); }

        // Recycle one live node back to the free list (the X360 live->free move:
        // InternalRemoveNode(live,n) ; InternalAddHead(free,n), e.g. UpdateQueued @0x82702108
        // promoting/recycling QueuedResource nodes). The node must currently be in the live list.
        void RecycleNode(Node* lpNode)
        {
            maLiveList.InternalRemoveNode(lpNode);
            maFreeList.InternalAddHead(lpNode);
        }

    private:
        // The Internal* surgery is protected on BaseLinkedList; a thin subclass re-exposes
        // exactly the operations the helper drives (the grow-in adds InternalInit /
        // InternalGetHead / InternalAddHead / InternalRemoveNode; InternalRemoveHead /
        // InternalAddTail were already exposed for AddTail).
        struct Sublist : public BaseLinkedList
        {
        public:
            using BaseLinkedList::InternalRemoveHead;
            using BaseLinkedList::InternalAddTail;
            using BaseLinkedList::InternalInit;
            using BaseLinkedList::InternalGetHead;
            using BaseLinkedList::InternalAddHead;
            using BaseLinkedList::InternalRemoveNode;
        };

        // X360 member SEQUENCE (recovered from ResourceRegistrar::Construct 0x826B0470, which
        // reads the count as `*a1` then passes `a1+2` as the pool base -- i.e. the count word
        // precedes the node pool): the live node count N, then the backing node pool, then the
        // free/live sublists. LAYOUT NOTE: X360 byte gaps (count word + pool alignment) are
        // 4-byte-pointer artifacts; members are pinned by name+sequence only, not asserted on
        // the 64-bit host.
        s32     miNumNodes;      // +0   - capacity (== N); X360 reads it as the InternalInit count
        Node    maNodePool[N];   // backing storage handed out to the two sublists
        Sublist maFreeList;      // nodes available to hand out
        Sublist maLiveList;      // nodes currently in use
    };
}
