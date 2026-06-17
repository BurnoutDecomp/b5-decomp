#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsIndexedLinkList.h"   // IndexedLinkedList / IndexedLinkedListNode

// CgsResource::Heap - the resource pool's linear sub-allocator. It owns a contiguous
// block of memory (mpcAddress..+muTotalSize) carved into a doubly-linked run of
// HeapEntry nodes (free + allocated, address-ordered). Allocation splits a free node;
// freeing coalesces with free neighbours. The node structs live in a fixed array
// (mpNodes) and are recycled through two index-lists: mUnusedNodes (the free pool of
// node structs) and mUsedNodes (the live address-ordered entry list). Batch alloc /
// relocate drive defrag. A Pool owns three Heaps (one per memory pool).
//
// DECOMPILED from the X360 build: every member offset here was verified against the
// inlined field accesses in Heap::Reprepare (0x828FD6A8) and the entry-size packing in
// Heap::GetLargestFreeBlock (0x828ECAE8) -- the HeapEntry size word packs the status in
// bits 28-30 and the size in the remaining bits (mask 0x8FFFFFFF == KU_MAX_SIZE). Field
// widths follow the x64 PC target (pointers widen); the field order/types are faithful.
// Method bodies are reconstructed in CgsResourceHeap.cpp.

namespace CgsMemory { class LinearMalloc; }   // overhead allocator (pointer param only)

namespace CgsResource
{
    // CgsResourceHeap.h:146
    enum EBatchAllocResult
    {
        E_BATCHALLOCRESULT_SUCCESS          = 0,
        E_BATCHALLOCRESULT_FAIL_NEED_DEFRAG = 1,
        E_BATCHALLOCRESULT_FAIL_NO_ROOM     = 2,
    };

    // CgsResourceHeap.h:54 - one request in a batch allocation (size + alignment, and
    // whether to allocate from the top or bottom of the free space).
    struct AllocRequest
    {
        u32   muSize;
        u32   muAlignment;
        bool  mbAllocateTop;
        void* mpOwner;
    };

    // CgsResourceHeap.h:71 - an allocation request pinned to a fixed offset.
    struct AllocRequestAddressed
    {
        u32   muSize;
        u32   muOffset;
        void* mpOwner;
    };

    // CgsResourceHeap.h:87 - the result of one batch allocation (the memory + its node).
    struct AllocResult
    {
        void* mpData;
        u16   muIndex;
    };

    // CgsResourceHeap.h:102 - move node muNode to byte offset muDestOffset (defrag).
    struct RelocateRequest
    {
        u16 muNode;
        u32 muDestOffset;
    };

    // CgsResourceHeap.h:117 - the source span of a relocation.
    struct RelocateSource
    {
        u32   muSourceOffset;
        u32   muSize;
        void* mpOwner;
    };

    // CgsResourceHeap.h:133 - a flattened (offset,size,status) view of the heap used to
    // hand the layout to the defrag planner.
    struct LinearHeapNode
    {
        static const u32 KU_STATUS_FREE = 0;
        static const u32 KU_STATUS_USED = 1;

        u32   muOffset;
        u32   muSize;
        u16   muStatus;
        u16   muNode;
        void* mpOwner;
    };

    // CgsResourceHeap.h:169 - one heap block: its address, its size+status word, and its
    // owner. The size word packs the allocation status in bits 28-30 and the size in the
    // rest (KU_MAX_SIZE is the size mask); the accessors below pack/unpack it.
    struct HeapEntry
    {
        static const u32 KU_MAX_SIZE = 0x8FFFFFFFu;   // 2415919103 - size mask of the packed word

        bool  Contains(char* lpcAddress) const;
        bool  IsFree() const;
        bool  IsAllocated() const;
        char* GetAddress() const;                                  // the raw block base
        char* GetAddress(bool lbAllocatedFromTop, u32 luSize) const;
        void* GetOwner() const;
        u32   GetSize() const;
        void  SetAllocated(bool lbAllocated);
        void  SetAddress(char* lpcAddress);
        void  SetOwner(void* lpOwner);
        void  SetSize(u32 luSize);

    private:
        char* mpAddress;   // +0
        u32   muSize;      // +4  (status<<28 | size)
        void* mpOwner;     // +8
    };

    // CgsResourceHeap.h:238
    class Heap
    {
    public:
        typedef CgsContainers::IndexedLinkedListNode<HeapEntry, u16> HeapEntryNode;
        typedef CgsContainers::IndexedLinkedList<HeapEntry, u16>     HeapEntryList;

        u32  GetOverheadMemoryRequired(u32 luMaxNodes);
        void Construct();
        bool Prepare(u32 luMaxNodes, CgsMemory::LinearMalloc* lpOverheadAllocator, void* lpHeapMemory,
                     u32 luHeapMemorySize, u32 luHeapAlignment, const char* lpcDebugName);
        bool Reprepare();
        bool Release();
        void Destruct();

        void* Malloc(u32 luSize, const char* lpcDebugName, void* lpOwner, u16 luStartIndex,
                     u16* lpuFoundIndex, bool lbBestFit, bool lbAllocateFromTop, void* lpAddress);
        void  Free(void* lpPtr);
        void  Free(u16 luNodeIndex);

        int  GetAmountFree() const;
        int  GetLargestFreeBlock();
        u32  GetPoolChecksum();
        bool IsUpdated();
        void EnableTracing(bool lbEnable);
        void DebugPrint();
        void UnitTest();

        u32   GetHeapAlignment() const;
        char* GetBaseAddress() const;
        s32   GetTotalSize() const;
        bool  Contains(const void* lpPtr) const;
        s32   GetMaxNodes() const;

        void  GetNodeUsageStatistics(s32* lpiOutNumUsedNodes, s32* lpiOutNumUnusedNodes,
                                     s32* lpiOutNumAllocatedNodes, s32* lpiOutNumFreeNodes,
                                     s32* lpiOutTotalAllocatedSize, s32* lpiOutTotalFreeSize,
                                     s32* lpiOutLargestFreeBlock);
        float GetFragmentationLevel();

        HeapEntryNode* GetFirstNode(bool lbStartFromTop);
        HeapEntryNode* GetFirstFreeNode(bool lbStartFromTop);
        HeapEntryNode* GetFirstAllocatedNode(bool lbStartFromTop);
        HeapEntryNode* GetNextNode(bool lbStartFromTop, HeapEntryNode* lpNode);
        HeapEntryNode* GetNextFreeNode(bool lbStartFromTop, HeapEntryNode* lpNode);
        HeapEntryNode* GetNextAllocatedNode(bool lbStartFromTop, HeapEntryNode* lpNode);

        EBatchAllocResult ExecuteBatchAllocation(AllocRequest* lpRequests, AllocResult* lpResults,
                                                 u32 luNumRequests, bool lbFreeOnFailure);
        EBatchAllocResult ExecuteBatchAddressedAllocation(AllocRequestAddressed* lpRequests, AllocResult* lpResults,
                                                          u32 luNumRequests, bool lbFreeOnFailure);
        void ExecuteBatchRelocation(RelocateRequest* lpRelocateRequests, u32 luNumRequests);
        u16  GenerateLinearHeap(LinearHeapNode* lpInputLinearHeap, u16 luMaxLength);

        u32 CalculateRequiredSizeForAllocList(const AllocRequest* lpRequests, u32 luNumRequests) const;
        u32 CalculateRequiredSizeForAllocList(const AllocRequestAddressed* lpRequests, u32 luNumRequests) const;

    private:
        HeapEntryNode* GetNewNode(void* lpAddress, s32 liSize, bool lbAllocated, const char* lpcDebugName, void* lpOwner);
        void           RemoveNode(HeapEntryNode* lpNode);
        void           UnitTestIteration(void** lpAllocArray, s32 liMaxAllocs);
        HeapEntryNode* FindFreeNodeContainingAddress(HeapEntryNode* lpStartNode, u32 luSize, void* lpAddress);
        HeapEntryNode* FindFreeNodeBestFit(HeapEntryNode* lpStartNode, u32 luSize);
        HeapEntryNode* FindFreeNodeFromTop(HeapEntryNode* lpStartNode, u32 luSize);
        HeapEntryNode* FindFreeNodeFromBottom(HeapEntryNode* lpStartNode, u32 luSize);
        static s32     SortAllocsBySizeQSortCallback(const void* lpData0, const void* lpData1);

        // ---- Layout (offsets verified against Heap::Reprepare, x64 widths) -------------
        s32           miNumNodes;          // count of node structs in mpNodes
        HeapEntryNode* mpNodes;            // the node-struct array (links are indices into it)
        HeapEntryList  mUnusedNodes;       // free pool of node structs
        HeapEntryList  mUsedNodes;         // live address-ordered entry list (free + allocated)
        const char*    mpcDebugName;
        u32            muTotalSize;
        char*          mpcAddress;         // base of the managed memory block
        u32            muHeapAlignment;
        s32            miAllocationNumber;
        u32            muAmountFree;
        u32            muLargestFree;
        bool           mbNeedToRecalcLargestFree;
        bool           mbPrepared;
        bool           mbTracingEnabled;
        bool           mbUpdated;
    };
}
