#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (DebugLinkedList guards)

// CgsDev::Internal debug collection templates - the three container shapes the whole debug-UI
// runtime is built from. Recovered from the DecFIGS DWARF
// (Development/DebugSystem/Core/CgsDebugCollections.h), which lists them per-instantiation
// (DebugStaticPool<Menu>, DebugStaticPool<Variable>, ...); the implementation is a single
// template per shape.
//
//   DebugStaticArray<T> : a fixed-capacity array of T* (built once from a resource allocator).
//   DebugStaticPool<T>  : a free-list pool of T, layered over two DebugStaticArray<T> (active +
//                         free) plus the backing T[] block (mpItemsArray).
//   DebugLinkedList<T>  : an intrusive singly-linked list threaded through T::mpDebugLinkedListNext.
//
// Every element type (Variable / Function / Menu / MenuItem / ...) is held BY POINTER (T** / T*),
// so this header needs only a forward declaration of T.
//
// Body status (X360 ARTIST):
//   * DebugStaticPool<T>::Allocate is GROUNDED in the X360 (DebugStaticPool<Variable>::Allocate
//     0x82827908): pop the last free slot (free is a LIFO stack), push it onto active, return it.
//     Layout verified there: DebugStaticArray = {i16 miMaxSize@0, i16 miCount@2, T** mppItems@4};
//     DebugStaticPool = {mActive@0, mFree@8, T* mpItemsArray@16}.
//   * The Array append/accessors + the Pool free/queries are reconstructed over that verified
//     layout + the LIFO mechanics Allocate attests (the Pool is built purely on the Array public
//     API, so no cross-type friendship is needed).
//   * DebugLinkedList<T> is now defined inline: an intrusive singly-linked list using
//     T::mpDebugLinkedListNext (so each element type friends DebugLinkedList<itself>). Add (append
//     to tail) + AddAfter are GROUNDED in the X360 (DebugLinkedList<MenuItem>::Add 0x828211B8,
//     ::AddAfter 0x828282B0 - link at the element's mpDebugLinkedListNext, e.g. MenuItem+12); the
//     remaining ops (Remove/GetPrevious/Replace/wrap/queries) follow that same attested link layout.
//   * Construct/Clear (they take rw::IResourceAllocator and build/zero the backing block) are the
//     next grounding step - declared here, defined alongside the allocator wiring.

namespace rw { class IResourceAllocator; }

namespace CgsDev
{
    namespace Internal
    {
        // Fixed-capacity array of T* (DWARF CgsDebugCollections.h:87).
        template <class T>
        struct DebugStaticArray
        {
            s16 miMaxSize;
            s16 miCount;
            T** mppItems;

            void Construct(int liMaxSize, rw::IResourceAllocator* lpAllocator);
            void Clear();

            void Add(T* lpItem)
            {
                mppItems[miCount] = lpItem;
                ++miCount;
            }

            // Find the slot and swap-remove with the last entry (order is not significant for the
            // pool's active/free arrays; the LIFO free stack relies on this being the last item).
            void Remove(T* lpItem)
            {
                for (s32 liIndex = 0; liIndex < miCount; ++liIndex)
                {
                    if (mppItems[liIndex] == lpItem)
                    {
                        --miCount;
                        mppItems[liIndex] = mppItems[miCount];
                        return;
                    }
                }
            }

            T*   GetFree();
            T*   GetFirst() const  { return (miCount > 0) ? mppItems[0] : nullptr; }
            bool IsEmpty() const   { return miCount == 0; }
            s32  GetMaxSize() const{ return miMaxSize; }
            s32  GetCount() const  { return miCount; }
            T*   GetAt(int liIndex) const       { return mppItems[liIndex]; }
            T*   operator[](int liIndex) const  { return mppItems[liIndex]; }
        };

        // Free-list pool of T (DWARF CgsDebugCollections.h:147), built on two DebugStaticArray<T>.
        template <class T>
        struct DebugStaticPool
        {
            DebugStaticArray<T> mActive;
            DebugStaticArray<T> mFree;
            T*                  mpItemsArray;

            void Construct(int liMaxSize, rw::IResourceAllocator* lpAllocator);
            void Clear();

            s32 GetMaxSize() const { return mActive.GetMaxSize(); }

            // X360 0x82827908: pop the last free item (free is a LIFO stack) onto the active list.
            T* Allocate()
            {
                const s32 liFreeCount = mFree.GetCount();
                if (liFreeCount <= 0)
                    return nullptr;

                T* lpItem = mFree.GetAt(liFreeCount - 1);
                if (!lpItem)
                    return nullptr;

                mFree.Remove(lpItem);
                mActive.Add(lpItem);
                return lpItem;
            }

            void Free(T* lpItem)
            {
                mActive.Remove(lpItem);
                mFree.Add(lpItem);
            }

            T*   GetFirstFree() const          { return mFree.GetFirst(); }
            T*   GetFreeAt(int liIndex) const  { return mFree.GetAt(liIndex); }
            s32  GetFreeCount() const          { return mFree.GetCount(); }
            T*   GetFirstActive() const        { return mActive.GetFirst(); }
            T*   GetActiveAt(int liIndex) const{ return mActive.GetAt(liIndex); }
            s32  GetActiveCount() const        { return mActive.GetCount(); }

            bool IsFromPool(T* lpItem) const
            {
                return lpItem >= mpItemsArray && lpItem < (mpItemsArray + mActive.GetMaxSize());
            }
        };

        // Intrusive singly-linked list (DWARF CgsDebugCollections.h:116): a head pointer; the next
        // link lives in T (T::mpDebugLinkedListNext). Each element type friends DebugLinkedList<itself>
        // so these bodies can thread that private link. Methods instantiate only where T is complete.
        template <class T>
        struct DebugLinkedList
        {
            T* mpList;

            void Clear() { mpList = nullptr; }

            bool IsEmpty() const  { return mpList == nullptr; }
            T*   GetFirst() const { return mpList; }
            T*   GetNext(const T* lpItem) const { return lpItem->mpDebugLinkedListNext; }

            T* GetLast() const
            {
                T* lpNode = mpList;
                if (lpNode)
                    while (lpNode->mpDebugLinkedListNext)
                        lpNode = lpNode->mpDebugLinkedListNext;
                return lpNode;
            }

            s32 GetCount() const
            {
                s32 liCount = 0;
                for (const T* lpNode = mpList; lpNode; lpNode = lpNode->mpDebugLinkedListNext)
                    ++liCount;
                return liCount;
            }

            bool IsAdded(const T* lpItem) const
            {
                for (const T* lpNode = mpList; lpNode; lpNode = lpNode->mpDebugLinkedListNext)
                    if (lpNode == lpItem)
                        return true;
                return false;
            }

            // X360 0x828211B8: append to the tail; the new item terminates the list.
            void Add(T* lpItem)
            {
                CGS_ASSERT(lpItem, "lpItem");
                CGS_ASSERT(!IsAdded(lpItem), "!IsAdded( lpItem )");

                if (mpList)
                {
                    T* lpNode = mpList;
                    while (lpNode->mpDebugLinkedListNext)
                        lpNode = lpNode->mpDebugLinkedListNext;
                    lpNode->mpDebugLinkedListNext = lpItem;
                }
                else
                {
                    mpList = lpItem;
                }
                lpItem->mpDebugLinkedListNext = nullptr;
            }

            // X360 0x828282B0: splice lpItem in directly after lpItemReference.
            void AddAfter(T* lpItemReference, T* lpItem)
            {
                CGS_ASSERT(lpItem, "lpItem");
                CGS_ASSERT(lpItemReference, "lpItemReference");
                CGS_ASSERT(mpList, "mpList");
                CGS_ASSERT(IsAdded(lpItemReference), "IsAdded(lpItemReference)");

                if (IsAdded(lpItem))
                    Remove(lpItem);

                lpItem->mpDebugLinkedListNext = lpItemReference->mpDebugLinkedListNext;
                lpItemReference->mpDebugLinkedListNext = lpItem;
            }

            // Unlink lpItem (reconstructed over the same attested link layout).
            void Remove(T* lpItem)
            {
                if (mpList == lpItem)
                {
                    mpList = lpItem->mpDebugLinkedListNext;
                    lpItem->mpDebugLinkedListNext = nullptr;
                    return;
                }
                for (T* lpNode = mpList; lpNode; lpNode = lpNode->mpDebugLinkedListNext)
                {
                    if (lpNode->mpDebugLinkedListNext == lpItem)
                    {
                        lpNode->mpDebugLinkedListNext = lpItem->mpDebugLinkedListNext;
                        lpItem->mpDebugLinkedListNext = nullptr;
                        return;
                    }
                }
            }

            void Replace(T* lpOld, T* lpNew)
            {
                if (mpList == lpOld)
                {
                    lpNew->mpDebugLinkedListNext = lpOld->mpDebugLinkedListNext;
                    mpList = lpNew;
                }
                else
                {
                    T* lpPrevious = GetPrevious(lpOld);
                    if (lpPrevious)
                    {
                        lpNew->mpDebugLinkedListNext = lpOld->mpDebugLinkedListNext;
                        lpPrevious->mpDebugLinkedListNext = lpNew;
                    }
                }
                lpOld->mpDebugLinkedListNext = nullptr;
            }

            T* GetPrevious(const T* lpItem) const
            {
                for (T* lpNode = mpList; lpNode; lpNode = lpNode->mpDebugLinkedListNext)
                    if (lpNode->mpDebugLinkedListNext == lpItem)
                        return lpNode;
                return nullptr;
            }

            T* GetNextWrap(const T* lpItem) const
            {
                T* lpNext = lpItem->mpDebugLinkedListNext;
                return lpNext ? lpNext : mpList;
            }

            T* GetPreviousWrap(const T* lpItem) const
            {
                T* lpPrevious = GetPrevious(lpItem);
                return lpPrevious ? lpPrevious : GetLast();
            }
        };
    }
}
