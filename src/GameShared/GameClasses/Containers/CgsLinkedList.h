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

    private:
        // The Internal* surgery is protected on BaseLinkedList; a thin subclass re-exposes
        // exactly the two operations AddTail needs so the helper can drive them.
        struct Sublist : public BaseLinkedList
        {
        public:
            using BaseLinkedList::InternalRemoveHead;
            using BaseLinkedList::InternalAddTail;
        };

        Node    maNodePool[N];   // backing storage handed out to the two sublists
        Sublist maFreeList;      // +0xC4 - nodes available to hand out
        Sublist maLiveList;      // +0xD0 - nodes currently in use
    };
}
