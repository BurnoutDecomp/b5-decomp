#include "GameShared/GameClasses/Containers/CgsLinkedList.h"

// CgsContainers::BaseLinkedList - out-of-line surgery for the intrusive doubly-
// linked list base. DECOMPILED from the X360 build (BURNOUT_X360_ARTIST.XEX):
//   CountElements        @ 0x821F0E48
//   InternalInit         @ 0x82814CE8
//   InternalGetHead      @ 0x82814D60
//   InternalGetTail      @ 0x82814DC0
//   InternalAddHead      @ 0x82814E20
//   InternalAddTail      @ 0x82814EC8
//   InternalRemoveHead   @ 0x82814F70
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
        if (lpNodes)
        {
            miCount = liNodeCount;
            mpFirst = lpNodes;
            lpNodes->mpPrev = 0;

            if (liNodeCount > 1)
            {
                BaseLinkedListNode* lpCurrNode = lpNodes;
                BaseLinkedListNode* lpNextPrevSlot = StrideNode(lpNodes, liStride);
                s32 liRemaining = liNodeCount - 1;
                do
                {
                    lpNextPrevSlot->mpPrev = lpCurrNode;
                    --liRemaining;
                    lpNextPrevSlot = StrideNode(lpNextPrevSlot, liStride);
                    lpCurrNode->mpNext = StrideNode(lpCurrNode, liStride);
                    lpCurrNode = StrideNode(lpCurrNode, liStride);
                }
                while (liRemaining);
            }

            BaseLinkedListNode* lpLast = StrideNode(lpNodes, (liNodeCount - 1) * liStride);
            mpLast = lpLast;
            lpLast->mpNext = 0;
        }
        else
        {
            miCount = 0;
            mpLast  = 0;
            mpFirst = 0;
        }
    }

    // 0x82814D60 - head of the list.
    BaseLinkedListNode* BaseLinkedList::InternalGetHead() const
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");
        return mpFirst;
    }

    // 0x82814DC0 - tail of the list.
    BaseLinkedListNode* BaseLinkedList::InternalGetTail() const
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");
        return mpLast;
    }

    // 0x82814E20 - prepend a node.
    void BaseLinkedList::InternalAddHead(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");

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
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");

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
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");
        CGS_ASSERT(InternalGetNodeIndex(lpNodeInList) >= 0, "InternalGetNodeIndex( lpNodeInList ) >= 0");

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
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");

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

    // 0x82815708 - unlink an arbitrary node, fixing up the head/tail or its
    // neighbours. Forwards to RemoveHead/RemoveTail at the ends.
    void BaseLinkedList::InternalRemoveNode(BaseLinkedListNode* lpNode)
    {
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");
        CGS_ASSERT(InternalGetNodeIndex(lpNode) >= 0, "InternalGetNodeIndex( lpNode ) >= 0");

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
        CGS_ASSERT(miCount != KI_UNINITIALISED, "BaseLinkedList accessed when uninitialised");

        BaseLinkedListNode* lpCurrNode = mpFirst;
        s32 liPosition = 0;
        if (!mpFirst)
        {
            return -1;
        }
        while (lpCurrNode != lpNode)
        {
            lpCurrNode = lpCurrNode->mpNext;
            ++liPosition;
            if (!lpCurrNode)
            {
                return -1;
            }
        }
        return liPosition;
    }
}
