#include "GameShared/GameClasses/Containers/CgsLinkedList.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// CgsContainers::BaseLinkedList - out-of-line surgery for the intrusive doubly-
// linked list base. DECOMPILED from the X360 build (BURNOUT_X360_ARTIST.XEX):
//   CountElements        @ 0x821F0E48
//   InternalInit         @ 0x82814CE8
//   InternalGetHead      @ 0x82814D60
//   InternalGetTail      @ 0x82814DC0
//   InternalAddHead      @ 0x82814E20
//   InternalAddTail      @ 0x82814EC8
//   InternalRemoveHead   @ 0x82814F70
//   InternalRemoveTail   @ 0x82815018
//   InternalGetNodeIndex @ 0x828150C8
//   InternalAddBefore    @ 0x82815638
//   InternalRemoveNode   @ 0x82815708
//
// miCount holds the sentinel 0x7FFFFFFF (KI_UNINITIALISED) until InternalInit()
// runs; every accessor asserts against it. The node array passed to InternalInit
// is walked by a BYTE stride (liStride) because the live nodes are LinkedListNode<T>
// subclasses larger than BaseLinkedListNode.

namespace CgsContainers
{
    namespace
    {
        const char KAC_LINKED_LIST_FILE[] = "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\containers\\CgsLinkedList.cpp";

        // Advance a node pointer by a byte stride (the live nodes are larger
        // subclasses, so InternalInit chains them by element size, not sizeof(node)).
        inline BaseLinkedListNode* StrideNode(BaseLinkedListNode* lpNode, s32 liStride)
        {
            return reinterpret_cast<BaseLinkedListNode*>(reinterpret_cast<char*>(lpNode) + liStride);
        }
    }

    // 0x821F0E48 - return the cached element count (just reads miCount).
    s32 BaseLinkedList::CountElements() const
    {
        return miCount;
    }

    // 0x82814CE8 - bind to a caller-owned node array and pre-chain it. With a
    // non-null array the nodes are linked 0 -> 1 -> ... -> liNodeCount-1 (walked by
    // liStride bytes), head prev / tail next nulled; with a null array start empty.
    void BaseLinkedList::InternalInit(BaseLinkedListNode* lpNodes, s32 liNodeCount, s32 liStride)
    {
        if (!lpNodes)
        {
            miCount = 0;
            mpLast  = 0;
            mpFirst = 0;
            return;
        }

        miCount = liNodeCount;
        mpFirst = lpNodes;
        lpNodes->mpPrev = 0;

        for (s32 liIndex = 1; liIndex < liNodeCount; ++liIndex)
        {
            BaseLinkedListNode* lpThisNode = StrideNode(lpNodes, liIndex * liStride);
            BaseLinkedListNode* lpPrevNode = StrideNode(lpNodes, (liIndex - 1) * liStride);
            lpThisNode->mpPrev = lpPrevNode;
            lpPrevNode->mpNext = lpThisNode;
        }

        mpLast = StrideNode(lpNodes, (liNodeCount - 1) * liStride);
        mpLast->mpNext = 0;
    }

    // 0x82814D60 - head of the list.
    BaseLinkedListNode* BaseLinkedList::InternalGetHead() const
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 84 /* ARTIST 0x82814D90 */));
        return mpFirst;
    }

    // 0x82814DC0 - tail of the list.
    BaseLinkedListNode* BaseLinkedList::InternalGetTail() const
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 92 /* ARTIST 0x82814DF0 */));
        return mpLast;
    }

    // 0x82814E20 - prepend a node.
    void BaseLinkedList::InternalAddHead(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 100 /* ARTIST 0x82814E58 */));

        lpNode->mpPrev = 0;
        lpNode->mpNext = mpFirst;
        if (mpFirst)
        {
            mpFirst->mpPrev = lpNode;
        }
        if (miCount == 0)
        {
            mpLast = lpNode;
        }
        mpFirst = lpNode;
        ++miCount;
    }

    // 0x82814EC8 - append a node.
    void BaseLinkedList::InternalAddTail(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 123 /* ARTIST 0x82814F00 */));

        lpNode->mpNext = 0;
        lpNode->mpPrev = mpLast;
        if (mpLast)
        {
            mpLast->mpNext = lpNode;
        }
        if (miCount == 0)
        {
            mpFirst = lpNode;
        }
        mpLast = lpNode;
        ++miCount;
    }

    // 0x82815638 - insert lpNewNode before lpNodeInList. NOTE: the X360 build routes
    // the "before the current head" case to InternalAddTail (verified in the asm at
    // 0x828156CC); preserve that behaviour exactly rather than "correcting" it.
    void BaseLinkedList::InternalAddBefore(BaseLinkedListNode* lpNodeInList, BaseLinkedListNode* lpNewNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 167 /* ARTIST 0x82815674 */));
        CGS_ASSERT(InternalGetNodeIndex(lpNodeInList) >= 0,
            CGS_ASSERT_MESSAGE_AT(
                "InternalGetNodeIndex( lpNodeInList ) >= 0", KAC_LINKED_LIST_FILE, 168 /* ARTIST 0x828156A4 */));

        if (lpNodeInList == mpFirst)
        {
            InternalAddTail(lpNewNode);
        }
        else
        {
            lpNewNode->mpNext = lpNodeInList;
            BaseLinkedListNode* lpPrev = lpNodeInList->mpPrev;
            lpNewNode->mpPrev = lpPrev;
            lpPrev->mpNext = lpNewNode;
            lpNewNode->mpNext->mpPrev = lpNewNode;
            ++miCount;
        }
    }

    // 0x82814F70 - unlink and return the head node.
    BaseLinkedListNode* BaseLinkedList::InternalRemoveHead()
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 188 /* ARTIST 0x82814FA0 */));

        BaseLinkedListNode* lpNode = mpFirst;
        if (mpFirst)
        {
            BaseLinkedListNode* lpNext = lpNode->mpNext;
            mpFirst = lpNext;
            if (lpNext)
            {
                lpNext->mpPrev = 0;
            }
        }
        if (--miCount == 0)
        {
            mpLast = 0;
        }
        if (lpNode)
        {
            lpNode->mpPrev = 0;
            lpNode->mpNext = 0;
        }
        return lpNode;
    }

    // 0x82815018 - unlink and return the tail node.
    BaseLinkedListNode* BaseLinkedList::InternalRemoveTail()
    {
        CGS_ASSERT_AT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised",
            KAC_LINKED_LIST_FILE, 223);

        BaseLinkedListNode* lpNode = mpLast;
        if (mpLast)
        {
            mpLast = mpLast->mpPrev;
        }
        if (mpLast)
        {
            mpLast->mpNext = 0;
        }
        if (--miCount == 0)
        {
            mpFirst = 0;
        }
        if (lpNode)
        {
            lpNode->mpPrev = 0;
            lpNode->mpNext = 0;
        }
        return lpNode;
    }

    // 0x82815708 - unlink an arbitrary node, fixing up the head/tail or its
    // neighbours. Forwards to RemoveHead/RemoveTail at the ends.
    void BaseLinkedList::InternalRemoveNode(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED,
            CGS_ASSERT_MESSAGE_AT(
                "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 258 /* ARTIST 0x82815740 */));
        CGS_ASSERT(InternalGetNodeIndex(lpNode) >= 0,
            CGS_ASSERT_MESSAGE_AT(
                "InternalGetNodeIndex( lpNode ) >= 0", KAC_LINKED_LIST_FILE, 259 /* ARTIST 0x82815770 */));

        if (mpFirst == lpNode)
        {
            InternalRemoveHead();
        }
        else if (mpLast == lpNode)
        {
            InternalRemoveTail();
        }
        else
        {
            lpNode->mpPrev->mpNext = lpNode->mpNext;
            lpNode->mpNext->mpPrev = lpNode->mpPrev;
            lpNode->mpPrev = 0;
            lpNode->mpNext = 0;
            --miCount;
        }
    }

    // 0x828150C8 - 0-based position of lpNode walking from the head, or -1 if absent.
    s32 BaseLinkedList::InternalGetNodeIndex(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED, CGS_ASSERT_MESSAGE_AT(
            "BaseLinkedList accessed when uninitialised", KAC_LINKED_LIST_FILE, 282));

        s32 liPosition = 0;
        for (BaseLinkedListNode* lpCurrNode = mpFirst; lpCurrNode != 0; lpCurrNode = lpCurrNode->mpNext)
        {
            if (lpCurrNode == lpNode)
                return liPosition;
            ++liPosition;
        }
        return -1;
    }
}
