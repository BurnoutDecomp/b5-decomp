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
}
