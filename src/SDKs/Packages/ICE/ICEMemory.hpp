#ifndef SDKS_PACKAGES_ICE_ICEMEMORY_HPP
#define SDKS_PACKAGES_ICE_ICEMEMORY_HPP

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc (base + embedded edit heap)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (bTList<T>::iterator::operator++)

// ============================================================================
// SDKs/Packages/ICE/ICEMemory.hpp
//
// The real ICE memory manager (was a keystone minimal stub -- a fake
// ICEMemoryManager with a lone mEditHeap -- seeded during the ICEData keystone;
// this replaces it with the attested layout).
//
// ICE::ICEMemory IS-A CgsMemory::HeapMalloc (DWARF: struct ICEMemory : public
// HeapMalloc) AND embeds a SECOND HeapMalloc at +0x520, proven by the X360 asm:
//   * Construct(buffer, size): HeapMalloc::Construct(this, buffer, size) builds
//     the BASE heap (offset 0) over the caller block, then allocates a 1.75 MB
//     "ICE Debug Memory" block and HeapMalloc::Construct(this+0x520, block, 1.75M)
//     builds the embedded edit heap (mEditHeap).
//   * GetMemory(size): HeapMalloc::Malloc(this+0x520, size, 4) -- allocates from
//     mEditHeap, the +0x520 heap (NOT the base heap at offset 0).
//   * ICETake::FreeEditBuffer frees the edit buffer with
//     HeapMalloc::Free(this+0x520, block) -- the SAME mEditHeap.
// So the global dword_82FB62C0 is an ICE::ICEMemory* (named ICE::spICEMemory in
// the DWARF), and +0x520 is mEditHeap -- the embedded edit heap that GetMemory
// allocates from and FreeEditBuffer frees to. sizeof(HeapMalloc) == 0x520 places
// mEditHeap exactly at +0x520.
// ============================================================================

namespace ICE
{

// ============================================================================
// The ICE intrusive doubly-linked list family (DWARF: SDKs/Packages/ICE/
// ICEMemory.hpp:105-402). This is the canonical mirrored home for bNode / bList /
// bTNode<T> / bTList<T> and the bTList<T>::iterator that walks the take list.
//
// Reconstructed minimally to support the bTList<ICETakeData>::iterator TU (the
// post-increment operator++ at X360 0x82531338, the iterator over the ICE take
// list). Only the layout pieces and the one comparison target (EndOfList) that the
// iterator needs are fleshed out; the rest of the list API is declaration-only
// (the per-TU `cl /c` gate does not link, and these bodies live in the ICEMemory
// body phase / are inlined at their use sites).
// ============================================================================

// bNode (DWARF ICEMemory.hpp:105). The intrusive node base: Next @+0, Prev @+4.
// ICETakeData chains through this 8-byte base (its Next is at offset 0, which the
// iterator advance reads as `node->Next`).
struct bNode
{
    bNode* Next;   // @+0
    bNode* Prev;   // @+4

    // --- DECLARE-ONLY (bodies in the ICEMemory body phase) ---
    void   Construct();
    void   Destruct();
    bNode* GetNext();
    bNode* GetPrev();
    bNode* Remove();
    bNode* AddAfter(bNode* lpNode);
    bNode* AddBefore(bNode* lpNode);
};

// bList (DWARF ICEMemory.hpp:191). The intrusive list head. HeadNode is the first
// member (offset 0), so EndOfList() -- the past-the-end sentinel -- is &HeadNode,
// i.e. the list pointer reinterpreted as a node. The iterator only COMPARES its
// _Ptr against EndOfList(); EndOfList is declaration-only (the gate does not link).
struct bList
{
private:
    bNode HeadNode;                          // @+0  sentinel node (head/tail ring)
    s32   TraversebList(bNode* lpNode);

public:
    // --- DECLARE-ONLY (bodies in the ICEMemory body phase) ---
    void   Construct();
    void   InitList();
    bool   IsEmpty();
    bNode* EndOfList();
    bNode* GetHead();
    bNode* GetTail();
    bNode* GetByPos(s32 liPos);
    bNode* GetNextCircular(bNode* lpNode);
    bNode* GetPrevCircular(bNode* lpNode);
    bNode* AddHead(bNode* lpNode);
    bNode* AddTail(bNode* lpNode);
    bNode* AddBefore(bNode* lpAt, bNode* lpNode);
    bNode* AddAfter(bNode* lpAt, bNode* lpNode);
    bNode* Remove(bNode* lpNode);
    bNode* RemoveHead();
    bNode* RemoveTail();
    void   AddTail(bList* lpList);
    void   AddHead(bList* lpList);
    bNode* GetNode(s32 liIndex);
    s32    GetNodeNumber(bNode* lpNode);
    s32    IsInList(bNode* lpNode);
    s32    CountElements();
    bNode* AddSorted(s32 (*lpfnCompare)(bNode*, bNode*), bNode* lpNode);
    void   Sort(s32 (*lpfnCompare)(bNode*, bNode*));

private:
    void   MergeSort(s32 (*lpfnCompare)(bNode*, bNode*));
};

// bTNode<T> (DWARF ICEMemory.hpp:166). Typed intrusive node: IS-A bNode, exposes the
// node API with T* return types. Adds no data (same 8-byte Next/Prev layout). A type
// T that lives in a bTList<T> derives from bTNode<T>.
template< class Type >
struct bTNode : public bNode
{
    // --- DECLARE-ONLY (bodies in the ICEMemory body phase) ---
    void  Construct();
    void  Destruct();
    Type* GetNext();
    Type* GetPrev();
    Type* Remove();
    Type* AddAfter(Type* lpNode);
    Type* AddBefore(Type* lpNode);
};

// bTList<T> (DWARF ICEMemory.hpp:298). Typed intrusive list: IS-A bList, exposes the
// list API with T* and the begin/end iterators. The nested iterator is the cursor the
// take-list walk uses.
template< class Type >
struct bTList : public bList
{
    // ------------------------------------------------------------------------
    // bTList<T>::iterator. A forward cursor over the intrusive list: { _Ptr, _Lst }.
    //   _Ptr @+0  current node
    //   _Lst @+4  owning list (used to find EndOfList() / the past-the-end sentinel)
    // ------------------------------------------------------------------------
    struct iterator
    {
        bNode*       _Ptr;   // @+0  current node
        bTList<Type>* _Lst;  // @+4  owning list

        // Post-increment: advance to the next node and return the PRE-advance copy.
        // X360 0x82531338 (bTList<ICETakeData>::iterator::operator++(int)).
        // The node's Next is at offset 0 of the bNode base, so advancing is
        // `_Ptr = _Ptr->Next`. The assert guards a live, non-end cursor; the baked
        // X360 path/line (ICEMemory.hpp:368) is dropped (CGS_ASSERT supplies them).
        //
        // EndOfList() is the past-the-end sentinel node. The typed bTList<Type>::
        // EndOfList() returns Type* (the sentinel reinterpreted as the element type);
        // since _Ptr is a bNode* and the sentinel IS the bList head node, the
        // comparison is done against the base bList::EndOfList() (returns bNode*) --
        // pointer-identity, exactly as the X360 compares _Ptr against the head sentinel
        // without needing Type to be a complete/derived type here.
        iterator operator++(int)
        {
            CGS_ASSERT(_Ptr && _Lst && _Ptr != _Lst->bList::EndOfList(),
                "_Ptr && _Lst && _Ptr!=_Lst->EndOfList()");

            iterator lOld;
            lOld._Lst = _Lst;
            lOld._Ptr = _Ptr;
            _Ptr = _Ptr->Next;
            return lOld;
        }
    };

    struct reverse_iterator;

    // EndOfList() -- the past-the-end sentinel the iterator compares against (the
    // typed override returns T*; declaration-only, the gate does not link).
    Type* EndOfList();

    // --- DECLARE-ONLY (the rest of the typed list API; bodies in the body phase) ---
    void  Destruct();
    void  DeleteAllElements();
    iterator begin();
    iterator end();
};

// The ICE memory manager. The base CgsMemory::HeapMalloc (offset 0) is the ICE
// general heap; mEditHeap (offset +0x520) is the dedicated edit-buffer heap that
// ICETake's edit path allocates from / frees to.
struct ICEMemory : public CgsMemory::HeapMalloc
{
    // X360 +0x520: the edit-buffer heap (ICETakeData home). GetMemory allocates
    // from it; ICETake::FreeEditBuffer frees to it.
    CgsMemory::HeapMalloc mEditHeap;

    // Build the base heap over [lpBuffer, lpBuffer+lnBufferSize), then build the
    // embedded edit heap over a freshly allocated debug-memory block. X360 0x82533468.
    void  Construct(void* lpBuffer, s32 lnBufferSize);
    // Allocate luSize bytes from the embedded edit heap (mEditHeap). X360 0x8252CC80.
    void* GetMemory(u32 luSize);
    // Free a block previously returned by GetMemory back to the edit heap.
    void  FreeMemory(void* lpMemory);
};

// The ICE memory-manager singleton (X360 dword_82FB62C0). DWARF: ICE::spICEMemory.
// Defined out-of-line by the ICE memory TU; the per-TU cl /c gate does not link.
extern ICEMemory* spICEMemory;

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEMEMORY_HPP
