#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsContainers::IndexedLinkedList<T, Index> - an intrusive doubly-linked list that
// stores its links as INDICES into a caller-supplied, relocatable node array (mpBase)
// instead of pointers, so the whole node block can be relocated/relinked in place (the
// resource Heap relies on this during defrag). Indices are 0-based offsets from mpBase;
// KI_LIST_INVALID (0xFFFF for a u16 index) is the null sentinel; an empty list has
// miFirst == miLast == KI_LIST_INVALID and miCount == 0.
//
// DECOMPILED from the X360 build (the template is inlined everywhere; no out-of-line
// copy exists): the layout and the exact index math were recovered from the inlined code
// in CgsResource::Heap::Reprepare (Init / InternalAddFirstNode / InternalAddTail) and
// ::GetLargestFreeBlock (the GetHead/GetNext walk). The baked asserts confirm two call
// contracts -- Init requires a base pointer (CgsIndexedLinkList.h:423) and InternalAddTail
// must not run on an empty list (CgsIndexedLinkList.h:1109). The remaining list surgery
// (AddHead/AddBefore/Remove*) follows the same confirmed 0-based / KI_LIST_INVALID rules.

namespace CgsContainers
{
    template <class T, class Index>
    class IndexedLinkedListNode
    {
    public:
        T*       GetData()                { return &mData; }
        const T* GetData() const          { return &mData; }
        void     SetData(T* lpData)       { mData = *lpData; }

        Index GetNextIndex() const        { return miNextIndex; }
        Index GetPrevIndex() const        { return miPrevIndex; }
        void  SetNextIndex(Index liIndex) { miNextIndex = liIndex; }
        void  SetPrevIndex(Index liIndex) { miPrevIndex = liIndex; }

        T*       operator->()             { return &mData; }
        const T* operator->() const       { return &mData; }
        T&       operator*()              { return mData; }
        const T& operator*() const        { return mData; }

    private:
        T     mData;
        Index miNextIndex;
        Index miPrevIndex;
    };

    template <class T, class Index>
    class IndexedLinkedList
    {
    public:
        typedef IndexedLinkedListNode<T, Index> Node;

        static const Index KI_LIST_ZERO    = 0;
        static const Index KI_LIST_INVALID = static_cast<Index>(-1);   // 0xFFFF for a u16 index

        // :412 - bind to the node array and, if liCount > 0, chain every node 0 -> 1 ->
        // ... -> liCount-1 (head prev / tail next = INVALID); otherwise start empty.
        void Init(Node* lpBase, Index liCount)
        {
            mpBase  = lpBase;
            miCount = liCount;
            if (liCount > 0)
            {
                CGS_ASSERT(lpBase != 0, "Indexed list must be initialized with a base pointer\n");  // :423
                miFirst = 0;
                miLast  = static_cast<Index>(liCount - 1);
                mpBase[0].SetPrevIndex(KI_LIST_INVALID);
                mpBase[miLast].SetNextIndex(KI_LIST_INVALID);
                for (s32 i = 1; i < static_cast<s32>(liCount); ++i)
                {
                    mpBase[i - 1].SetNextIndex(static_cast<Index>(i));
                    mpBase[i].SetPrevIndex(static_cast<Index>(i - 1));
                }
            }
            else
            {
                miFirst = KI_LIST_INVALID;
                miLast  = KI_LIST_INVALID;
            }
        }

        Node*       GetHead()              { return miFirst == KI_LIST_INVALID ? 0 : &mpBase[miFirst]; }
        const Node* GetHead() const        { return miFirst == KI_LIST_INVALID ? 0 : &mpBase[miFirst]; }
        Node*       GetTail()              { return miLast  == KI_LIST_INVALID ? 0 : &mpBase[miLast]; }
        const Node* GetTail() const        { return miLast  == KI_LIST_INVALID ? 0 : &mpBase[miLast]; }
        Index       GetHeadIndex() const   { return miFirst; }

        Node* GetNext(const Node* lpNode)
        {
            Index liNext = lpNode->GetNextIndex();
            return liNext == KI_LIST_INVALID ? 0 : &mpBase[liNext];
        }
        const Node* GetNext(const Node* lpNode) const
        {
            Index liNext = lpNode->GetNextIndex();
            return liNext == KI_LIST_INVALID ? 0 : &mpBase[liNext];
        }
        Node* GetPrev(const Node* lpNode)
        {
            Index liPrev = lpNode->GetPrevIndex();
            return liPrev == KI_LIST_INVALID ? 0 : &mpBase[liPrev];
        }
        const Node* GetPrev(const Node* lpNode) const
        {
            Index liPrev = lpNode->GetPrevIndex();
            return liPrev == KI_LIST_INVALID ? 0 : &mpBase[liPrev];
        }

        void AddHead(Node* lpNode) { if (miCount == 0) InternalAddFirstNode(lpNode); else InternalAddHead(lpNode); }
        void AddTail(Node* lpNode) { if (miCount == 0) InternalAddFirstNode(lpNode); else InternalAddTail(lpNode); }

        void AddAfter(Node* lpExisting, Node* lpNode)
        {
            Index liExisting = GetNodeOffset(lpExisting);
            Index liNode     = GetNodeOffset(lpNode);
            Index liNext     = lpExisting->GetNextIndex();
            lpNode->SetPrevIndex(liExisting);
            lpNode->SetNextIndex(liNext);
            lpExisting->SetNextIndex(liNode);
            if (liNext == KI_LIST_INVALID) miLast = liNode;
            else                           mpBase[liNext].SetPrevIndex(liNode);
            ++miCount;
        }

        void AddBefore(Node* lpExisting, Node* lpNode)
        {
            Index liExisting = GetNodeOffset(lpExisting);
            Index liNode     = GetNodeOffset(lpNode);
            Index liPrev     = lpExisting->GetPrevIndex();
            lpNode->SetNextIndex(liExisting);
            lpNode->SetPrevIndex(liPrev);
            lpExisting->SetPrevIndex(liNode);
            if (liPrev == KI_LIST_INVALID) miFirst = liNode;
            else                           mpBase[liPrev].SetNextIndex(liNode);
            ++miCount;
        }

        Node* RemoveHead()
        {
            if (miCount == 0) return 0;
            if (miCount == 1) return InternalRemoveLastNode();
            return InternalRemoveHead();
        }
        Node* RemoveTail()
        {
            if (miCount == 0) return 0;
            if (miCount == 1) return InternalRemoveLastNode();
            return InternalRemoveTail();
        }

        // Unlink an arbitrary node, fixing up its neighbours (or the head/tail).
        void RemoveNode(Node* lpNode)
        {
            Index liPrev = lpNode->GetPrevIndex();
            Index liNext = lpNode->GetNextIndex();
            if (liPrev == KI_LIST_INVALID) miFirst = liNext;
            else                           mpBase[liPrev].SetNextIndex(liNext);
            if (liNext == KI_LIST_INVALID) miLast = liPrev;
            else                           mpBase[liNext].SetPrevIndex(liPrev);
            --miCount;
        }

        s32  GetNodeIndex(Node* lpNode) const { return static_cast<s32>(lpNode - mpBase); }
        bool IsEmpty() const                  { return miCount == 0; }

        // CountElements walks the chain (vs the cached miCount) -- the generator/validators
        // use it to verify the live length.
        s32 CountElements() const
        {
            s32 liCount = 0;
            for (const Node* lpNode = GetHead(); lpNode != 0; lpNode = GetNext(lpNode))
                ++liCount;
            return liCount;
        }

        Index GetNodeOffset(Node* lpNode) const { return static_cast<Index>(lpNode - mpBase); }

    private:
        void InternalAddFirstNode(Node* lpNode)
        {
            Index liNode = GetNodeOffset(lpNode);
            lpNode->SetNextIndex(KI_LIST_INVALID);
            lpNode->SetPrevIndex(KI_LIST_INVALID);
            miFirst = liNode;
            miLast  = liNode;
            ++miCount;
        }
        void InternalAddHead(Node* lpNode)
        {
            Index liNode = GetNodeOffset(lpNode);
            lpNode->SetPrevIndex(KI_LIST_INVALID);
            lpNode->SetNextIndex(miFirst);
            mpBase[miFirst].SetPrevIndex(liNode);
            miFirst = liNode;
            ++miCount;
        }
        void InternalAddTail(Node* lpNode)
        {
            CGS_ASSERT(miCount > 0, "To add node to a zero length list call InternalAddFirstNode\n");  // :1109
            Index liNode = GetNodeOffset(lpNode);
            lpNode->SetNextIndex(KI_LIST_INVALID);
            lpNode->SetPrevIndex(miLast);
            mpBase[miLast].SetNextIndex(liNode);
            miLast = liNode;
            ++miCount;
        }
        Node* InternalRemoveLastNode()
        {
            Node* lpNode = &mpBase[miFirst];
            miFirst = KI_LIST_INVALID;
            miLast  = KI_LIST_INVALID;
            --miCount;
            return lpNode;
        }
        Node* InternalRemoveHead()
        {
            Node* lpNode = &mpBase[miFirst];
            Index liNext = lpNode->GetNextIndex();
            mpBase[liNext].SetPrevIndex(KI_LIST_INVALID);
            miFirst = liNext;
            --miCount;
            return lpNode;
        }
        Node* InternalRemoveTail()
        {
            Node* lpNode = &mpBase[miLast];
            Index liPrev = lpNode->GetPrevIndex();
            mpBase[liPrev].SetNextIndex(KI_LIST_INVALID);
            miLast = liPrev;
            --miCount;
            return lpNode;
        }

        s32   miCount;   // cached live element count
        Node* mpBase;    // caller-owned node array (links are indices into this)
        Index miFirst;   // head index, or KI_LIST_INVALID
        Index miLast;    // tail index, or KI_LIST_INVALID
    };
}
