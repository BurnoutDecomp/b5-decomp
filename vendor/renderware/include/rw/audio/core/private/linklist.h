#pragma once

namespace rw
{
namespace audio
{
namespace core
{

struct ListNode
{
    ListNode *pnext;
};

struct ListDNode
{
    ListDNode *pnext;
    ListDNode *pprev;
};

// The private linklist DWARF names these operations. ARTIST inlines Push in
// Collection::RemoveNode at 0x82B64BEC and Pop in AddItem at 0x82B6E200.
struct ListDStack
{
    void Reset()
    {
        phead = 0;
    }

    ListDNode *GetHead() const
    {
        return phead;
    }

    bool IsEmpty()
    {
        return phead == 0;
    }

    void Push(ListDNode *node)
    {
        // The inlined push reads the old head before touching either link (0x82B6E228).
        ListDNode *const previousHead = phead;
        node->pprev = 0;
        node->pnext = previousHead;
        if (phead)
            phead->pprev = node;
        phead = node;
    }

    ListDNode *Pop()
    {
        ListDNode *node = phead;
        if (node)
        {
            phead = node->pnext;
            if (phead)
                phead->pprev = 0;
        }
        return node;
    }

    void Remove(ListDNode *node)
    {
        if (node == phead)
            phead = phead->pnext;
        if (node->pprev)
            node->pprev->pnext = node->pnext;
        if (node->pnext)
            node->pnext->pprev = node->pprev;
    }

    ListDNode *phead;
};

struct ListQueue
{
    void Reset()
    {
        phead = 0;
        ptail = 0;
        entries = 0;
    }

    ListNode *GetHead() const
    {
        return phead;
    }

    void PushTail(ListNode *node)
    {
        node->pnext = 0;
        if (phead)
            ptail->pnext = node;
        else
            phead = node;
        ptail = node;
        ++entries;
    }

    ListNode *Pop()
    {
        ListNode *node = phead;
        if (node)
        {
            phead = node->pnext;
            if (!phead)
                ptail = 0;
            --entries;
        }
        return node;
    }

    ListNode *phead;
    ListNode *ptail;
    int entries;
};

}
}
}
