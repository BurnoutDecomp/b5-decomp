#pragma once

namespace rw
{
namespace audio
{
namespace core
{

struct ListNode
{
    ListNode *GetNext() { return pnext; }
    ListNode *pnext;
};

struct ListDNode
{
    void SetNext(ListDNode *node) { pnext = node; }
    void SetPrev(ListDNode *node) { pprev = node; }
    ListDNode *GetNext() { return pnext; }
    ListDNode *GetPrev() { return pprev; }

    ListDNode *pnext;
    ListDNode *pprev;
};

// The private linklist DWARF names these operations. ARTIST inlines Push in
// Collection::RemoveNode at 0x82B64BEC and Pop in AddItem at 0x82B6E200.
struct ListDStack
{
    ListDStack() : phead(0) {}

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
        node->SetNext(phead);
        node->SetPrev(0);
        if (phead)
            phead->SetPrev(node);
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
        if (node->GetPrev())
        {
            ListDNode *previous = node->GetPrev();
            previous->SetNext(node->GetNext());
        }
        if (node->GetNext())
        {
            ListDNode *next = node->GetNext();
            next->SetPrev(node->GetPrev());
        }
    }

    ListDNode *phead;
};

struct ListQueue
{
    ListQueue() : phead(0), ptail(0), entries(0) {}

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

    void Push(ListNode *node)
    {
        node->pnext = phead;
        if (!ptail)
            ptail = node;
        phead = node;
        ++entries;
    }

    void PushTail(ListNode *node)
    {
        node->pnext = 0;
        if (!phead)
            phead = node;
        else
            ptail->pnext = node;
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
