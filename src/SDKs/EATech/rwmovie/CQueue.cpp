// ===========================================================================
// SDKs/EATech/rwmovie/CQueue.cpp
//
// CQueue -- intrusive singly-linked queue with a pre-allocated node pool (RTCMV/WMV
// video encoder path). Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// This batch bodies:
//   CQueue::AddElement    0x82A03CC0
//   CQueue::RemoveElement 0x82A03DA0
//   CQueue::GetElement    0x82A03E80
//   CQueue::DestroyQueue  0x82A03C58
//
// CQueue::CQueue (0x82A03FA8) is NOT bodied here -- the proposed ctor was refuted
// (in-loop store zeroed the wrong node field; fabricated scratch member) -- so it is
// declared-only in the header and homed by a dedicated verify pass.
//
// The node's +0x04 slot is the caller's element pointer (AddElement stores it there;
// GetElement/RemoveElement read it back), modelled as void* (Node::mpElement).
// ===========================================================================

#include "SDKs/EATech/rwmovie/CQueue.h"

// ---------------------------------------------------------------------------
// CQueue::AddElement  @ 0x82A03CC0
// ---------------------------------------------------------------------------
s32 CQueue::AddElement(void* lpElement, s32 liIndex)
{
    if (lpElement != nullptr && liIndex <= miCount)
    {
        // Pop a node off the free list.
        Node* lpNode = mpFreeHead;
        bool lbFreeListEmptied = (lpNode->mpNext == nullptr);
        mpFreeHead = lpNode->mpNext;
        if (lbFreeListEmptied)
        {
            mpFreeTail = nullptr;
        }
        lpNode->mpElement = lpElement;

        if (liIndex != 0)
        {
            if (liIndex == -1)
            {
                // Append to the tail of the active list.
                lpNode->mpNext = nullptr;
                Node* lpOldHead = mpHead;
                mpTail = lpNode;
                if (lpOldHead == nullptr)
                {
                    mpHead = lpNode;
                    ++miCount;
                    return 1;
                }
                ++miCount;
                return 1;
            }

            // Splice after (liIndex - 1) nodes.
            s32 liSteps = liIndex - 1;
            Node* lpCursor = mpHead;
            if (liSteps > 0)
            {
                do
                {
                    --liSteps;
                    lpCursor = lpCursor->mpNext;
                }
                while (liSteps != 0);
            }
            lpNode->mpNext = lpCursor->mpNext;
            lpCursor->mpNext = lpNode;
            if (lpNode->mpNext == nullptr)
            {
                mpTail = lpNode;
            }
            ++miCount;
            return 1;
        }
        else
        {
            // Insert at the head of the active list.
            lpNode->mpNext = mpHead;
            Node* lpOldTail = mpTail;
            mpHead = lpNode;
            if (lpOldTail == nullptr)
            {
                mpTail = lpNode;
            }
            ++miCount;
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CQueue::RemoveElement  @ 0x82A03DA0
// Unlink the element at index liIndex from the active list, store its element pointer
// through lppElement, and recycle the node onto the free list. liIndex == -1 removes the
// tail. Returns 0 (and leaves *lppElement == 0) when the index is out of range; 1 on
// success. (The +0x04 node slot -- read here as mpElement -- is Header Y's "miValue".)
// ---------------------------------------------------------------------------
s32 CQueue::RemoveElement(void** lppElement, s32 liIndex)
{
    *lppElement = nullptr;

    const s32 liCount = miCount;
    if (liIndex >= liCount)
        return 0;

    Node* lpRemoved;
    if (liIndex == 0)
    {
        // Pop the head of the active list.
        lpRemoved = mpHead;
        Node* lpNext = lpRemoved->mpNext;
        mpHead = lpNext;
        if (!lpNext)
            mpTail = nullptr;
    }
    else if (liIndex == -1 && liCount == 1)
    {
        // Removing the only element via the tail path.
        lpRemoved = mpTail;
        mpHead = nullptr;
    }
    else
    {
        // Walk to the predecessor of the target, then splice the target out.
        s32 liHops = liIndex;
        if (liIndex == -1)
            liHops = liCount - 1;

        s32 liWalk = liHops - 1;
        Node* lpPrev = mpHead;
        if (liWalk > 0)
        {
            do
            {
                --liWalk;
                lpPrev = lpPrev->mpNext;
            } while (liWalk);
        }

        lpRemoved = lpPrev->mpNext;
        lpPrev->mpNext = lpRemoved->mpNext;
        if (liIndex == -1)
            mpTail = lpPrev;
    }

    *lppElement = lpRemoved->mpElement;

    // Recycle the node onto the free list.
    lpRemoved->mpNext = mpFreeHead;
    Node* lpFreeTail = mpFreeTail;
    mpFreeHead = lpRemoved;
    if (!lpFreeTail)
        mpFreeTail = lpRemoved;

    miCount = miCount - 1;
    return 1;
}

// ---------------------------------------------------------------------------
// CQueue::GetElement  @ 0x82A03E80
// ---------------------------------------------------------------------------
s32 CQueue::GetElement(void** lppElement, s32 liIndex)
{
    s32 liSteps = liIndex;
    if (lppElement == nullptr)
    {
        return 0;
    }
    s32 liCount = miCount;
    if (liIndex >= liCount)
    {
        return 0;
    }
    if (liIndex == -1)
    {
        liSteps = liCount - 1;
    }
    Node* lpNode = mpHead;
    if (liSteps > 0)
    {
        do
        {
            --liSteps;
            lpNode = lpNode->mpNext;
        }
        while (liSteps != 0);
    }
    *lppElement = lpNode->mpElement;
    return 1;
}

// ---------------------------------------------------------------------------
// CQueue::DestroyQueue  @ 0x82A03C58
// ---------------------------------------------------------------------------
void CQueue::DestroyQueue()
{
    // Free every node on the free list.
    Node* lpNode = mpFreeHead;
    if (lpNode != nullptr)
    {
        do
        {
            Node* lpNext = lpNode->mpNext;
            XMemFree(lpNode, KU_QUEUE_XMEM_ATTRIBUTES);
            lpNode = lpNext;
        }
        while (lpNode != nullptr);
    }

    // Free every node still on the active list.
    Node* lpActive = mpHead;
    if (lpActive != nullptr)
    {
        do
        {
            Node* lpNext = lpActive->mpNext;
            XMemFree(lpActive, KU_QUEUE_XMEM_ATTRIBUTES);
            lpActive = lpNext;
        }
        while (lpActive != nullptr);
    }
}
