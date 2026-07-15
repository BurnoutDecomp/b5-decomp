// ===========================================================================
// SDKs/EATech/rwmovie/CQueue.cpp
//
// CQueue -- intrusive singly-linked queue with a pre-allocated node pool (RTCMV/WMV
// video encoder path). Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// This TU bodies:
//   CQueue::CQueue        0x82A03FA8
//   CQueue::AddElement    0x82A03CC0
//   CQueue::RemoveElement 0x82A03DA0
//   CQueue::GetElement    0x82A03E80
//   CQueue::DestroyQueue  0x82A03C58
//
// The node's +0x04 slot is the caller's element pointer (AddElement stores it there;
// GetElement/RemoveElement read it back), modelled as void* (Node::mpElement).
// ===========================================================================

#include "SDKs/EATech/rwmovie/CQueue.h"

// ---------------------------------------------------------------------------
// CQueue::CQueue  @ 0x82A03FA8
// Zeroes every field, records the requested capacity, then pre-allocates `liCapacity`
// 8-byte pool nodes (raw XMemAlloc, attributes 0x248C8000) and links them onto the free
// list via a running link cursor that starts at the mpFreeHead slot. On the first
// allocation failure it tears the queue down (DestroyQueue) and reports -100 through
// *lpiResult; otherwise it reports 0. After the pool is built, the shared tail terminates
// the free list (last node's mpNext = 0), records mpFreeTail, and re-clears mpHead/mpTail.
//
// De-optimization note: the X360 allocates `8` bytes per node -- that literal is the
// X360 sizeof(Node) (two 32-bit pointers), written here as sizeof(Node) so a real Node is
// allocated on any target (mirrors the sibling CAltTablesEncoder sizeof(...) idiom).
//
// Degenerate path (liCapacity <= 0): the asm never enters the loop, so its "last node"
// register still points at a throwaway stack word. It stores that stack address into
// mpFreeTail and writes a zero word through it. That dangling store is reproduced faithfully
// here via lScratchNode; it is dead in practice (all callers pass a positive capacity), and
// mpFreeHead stays null so the free list is still empty.
// ---------------------------------------------------------------------------
CQueue::CQueue(s32* lpiResult, s32 liCapacity)
{
    // r27/v6: link cursor. Starts at the free-list head slot; after each node it advances
    // to that node's mpNext (offset 0), threading the singly-linked free list.
    Node** lppLink = &mpFreeHead;
    // r29/v8: count of nodes successfully allocated so far.
    s32 liAllocated = 0;
    // r11/v7: "last node" cursor. In the liCapacity <= 0 path the asm leaves it pointing at
    // this throwaway stack word (never a real node) -- see header note.
    Node  lScratchNode;
    Node* lpLast = &lScratchNode;

    mpHead     = nullptr;   // stw r30, 0(r31)
    mpTail     = nullptr;   // stw r30, 4(r31)
    mpFreeHead = nullptr;   // stw r30, 8(r31)
    mpFreeTail = nullptr;   // stw r30, 0xC(r31)
    *lpiResult = 0;         // stw r30, 0(r26)
    miCapacity = liCapacity; // stw r28, 0x14(r31)
    miCount    = 0;         // stw r30, 0x10(r31)

    if (liCapacity > 0)
    {
        for (;;)
        {
            Node* lpNode = static_cast<Node*>(XMemAlloc(sizeof(Node), KU_QUEUE_XMEM_ATTRIBUTES));
            lpLast = lpNode;
            if (lpNode == nullptr)
            {
                // Allocation failed: free whatever we linked and report -100.
                DestroyQueue();
                *lpiResult = -100;
                return;
            }
            ++liAllocated;
            lpNode->mpElement = nullptr; // stw r30, 4(r11)
            *lppLink = lpNode;           // stw r11, 0(r27)
            lppLink  = &lpNode->mpNext;  // r27 = r11 (next link target is node->mpNext @ +0)
            if (liAllocated >= liCapacity)
                break;
        }
    }

    // Shared tail (also reached when liCapacity <= 0, with lpLast == &lScratchNode).
    mpFreeTail     = lpLast;    // stw r11, 0xC(r31)
    lpLast->mpNext = nullptr;   // stw r30, 0(r11)
    mpTail         = nullptr;   // stw r30, 4(r31)
    mpHead         = nullptr;   // stw r30, 0(r31)
}

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
