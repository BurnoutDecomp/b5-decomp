#include "GameShared/GameClasses/System/Resource/CgsResourceHeap.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // overhead allocator
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

#include <cstdint>   // uintptr_t (heap-memory alignment check)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the node-exhaustion boot gate

// CgsResource::Heap method bodies, decompiled from the X360 ARTIST IDA (addresses noted).
// Built up incrementally: Prepare (setup) first; the live allocator (Reprepare/GetNewNode/
// Malloc/Free + FindFreeNode* + the HeapEntry size/status packing) follows. Methods not yet
// bodied are declared-only in the header (the per-TU gate is cl /c).

namespace CgsResource
{
    // 0x82901AB0 - one-time setup: validate alignment + node count, allocate the node-struct
    // array from the overhead allocator, record the managed region, then Reprepare() (which
    // builds the free-node chain and the initial whole-heap free block).
    bool Heap::Prepare(u32 luMaxNodes, CgsMemory::LinearMalloc* lpOverheadAllocator, void* lpHeapMemory,
                       u32 luHeapMemorySize, u32 luHeapAlignment, const char* lpcDebugName)
    {
        if (mbPrepared)
            return true;

        mbPrepared = true;
        CGS_ASSERT((luHeapMemorySize % luHeapAlignment) == 0, "(luHeapMemorySize % luHeapAlignment) == 0");                         // :94
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpHeapMemory) % luHeapAlignment) == 0, "(reinterpret_cast<int>(lpHeapMemory) % luHeapAlignment) == 0");   // :95
        CGS_ASSERT(luMaxNodes < 0xFFFF, "luMaxNodes < 0xFFFF");                                                                       // :96

        // X360 hardcodes 16 bytes/node; the x64 HeapEntryNode is wider, so size by sizeof.
        mpNodes            = static_cast<HeapEntryNode*>(lpOverheadAllocator->Malloc(sizeof(HeapEntryNode) * luMaxNodes));
        miNumNodes         = static_cast<s32>(luMaxNodes);
        mpcDebugName       = lpcDebugName;
        muTotalSize        = luHeapMemorySize;
        mbUpdated          = true;
        miAllocationNumber = 0;
        mbTracingEnabled   = false;
        muHeapAlignment    = luHeapAlignment;
        mpcAddress         = static_cast<char*>(lpHeapMemory);

        Reprepare();
        return true;
    }

    // 0x828FD6A8 - (re)build the lists: mUnusedNodes becomes the whole node array as a free
    // chain; mUsedNodes starts holding a single free block that spans the entire heap.
    bool Heap::Reprepare()
    {
        if (!mbPrepared)
            return true;

        mUnusedNodes.Init(mpNodes, static_cast<u16>(miNumNodes));
        mUsedNodes.Init(mpNodes, 0);

        muAmountFree              = muTotalSize;
        muLargestFree             = muTotalSize;
        mbNeedToRecalcLargestFree = false;

        HeapEntryNode* lpNode = GetNewNode(mpcAddress, static_cast<s32>(muTotalSize), false, 0, 0);
        mUsedNodes.AddTail(lpNode);
        return true;
    }

    // 0x828F4568 - recycle a node struct from the free-node pool (mUnusedNodes) and fill its
    // HeapEntry. The X360 inlines the list pull + a debug-name-length assert; here the pull is
    // the decompiled IndexedLinkedList::RemoveHead and the (debug-only) name assert is omitted.
    Heap::HeapEntryNode* Heap::GetNewNode(void* lpAddress, s32 liSize, bool lbAllocated, const char* lpcDebugName, void* lpOwner)
    {
        CGS_ASSERT(!mUnusedNodes.IsEmpty(), "!mUnusedNodes.IsEmpty()");   // :211
        (void)lpcDebugName;

        HeapEntryNode* lpNode  = mUnusedNodes.RemoveHead();
        HeapEntry*     lpEntry = lpNode->GetData();
        lpEntry->SetAddress(static_cast<char*>(lpAddress));
        lpEntry->SetSize(static_cast<u32>(liSize));
        lpEntry->SetAllocated(lbAllocated);
        lpEntry->SetOwner(lpOwner);
        return lpNode;
    }

    // ---- HeapEntry: muSize packs the allocation status in bits 28-30 (mask 0x70000000) and
    // the block size in the rest (mask 0x8FFFFFFF == KU_MAX_SIZE). ---------------------------

    bool HeapEntry::Contains(char* lpcAddress) const
    {
        return lpcAddress >= mpAddress && lpcAddress < mpAddress + GetSize();
    }

    bool HeapEntry::IsFree() const      { return (muSize & 0x70000000u) == 0; }
    bool HeapEntry::IsAllocated() const { return (muSize & 0x70000000u) != 0; }

    // Top-allocated blocks start at mpAddress; bottom-allocated blocks place their luSize-byte
    // payload at the end of the block (matching Heap::Malloc's address computation).
    char* HeapEntry::GetAddress(bool lbAllocatedFromTop, u32 luSize) const
    {
        return lbAllocatedFromTop ? mpAddress : (mpAddress + GetSize() - luSize);
    }

    void* HeapEntry::GetOwner() const { return mpOwner; }
    u32   HeapEntry::GetSize() const  { return muSize & KU_MAX_SIZE; }

    void HeapEntry::SetAllocated(bool lbAllocated)
    {
        if (lbAllocated) muSize |= 0x70000000u;
        else             muSize &= KU_MAX_SIZE;   // clear the status bits, keep the size
    }

    void HeapEntry::SetAddress(char* lpcAddress) { mpAddress = lpcAddress; }
    void HeapEntry::SetOwner(void* lpOwner)      { mpOwner = lpOwner; }

    void HeapEntry::SetSize(u32 luSize)
    {
        CGS_ASSERT(luSize <= KU_MAX_SIZE, "Size can not be larger than 0x8FFFFFFF - last bit is used as flag!\n");   // CgsResourceHeap.h:621
        muSize = (muSize & 0x70000000u) | luSize;   // preserve status, replace size
    }

    char* HeapEntry::GetAddress() const { return mpAddress; }

    // ---- node recycling & the free-list search ------------------------------------

    // 0x828F46D8 - return a node struct to the free-node pool (the caller has already
    // unlinked it from mUsedNodes). Despite the name it ADDS to mUnusedNodes.
    void Heap::RemoveNode(HeapEntryNode* lpNode)
    {
        mUnusedNodes.AddTail(lpNode);
    }

    // 0x828EB548 - first free block (>= luSize) walking forward (ascending address) from
    // lpStartNode, wrapping once through the head.
    Heap::HeapEntryNode* Heap::FindFreeNodeFromTop(HeapEntryNode* lpStartNode, u32 luSize)
    {
        for (HeapEntryNode* lpNode = lpStartNode; lpNode != 0; lpNode = mUsedNodes.GetNext(lpNode))
            if (lpNode->GetData()->IsFree() && lpNode->GetData()->GetSize() >= luSize)
                return lpNode;
        for (HeapEntryNode* lpNode = mUsedNodes.GetHead(); lpNode != 0 && lpNode != lpStartNode; lpNode = mUsedNodes.GetNext(lpNode))
            if (lpNode->GetData()->IsFree() && lpNode->GetData()->GetSize() >= luSize)
                return lpNode;
        return 0;
    }

    // 0x828EB618 - first free block (>= luSize) walking backward (descending address) from
    // lpStartNode, wrapping once through the tail.
    Heap::HeapEntryNode* Heap::FindFreeNodeFromBottom(HeapEntryNode* lpStartNode, u32 luSize)
    {
        for (HeapEntryNode* lpNode = lpStartNode; lpNode != 0; lpNode = mUsedNodes.GetPrev(lpNode))
            if (lpNode->GetData()->IsFree() && lpNode->GetData()->GetSize() >= luSize)
                return lpNode;
        for (HeapEntryNode* lpNode = mUsedNodes.GetTail(); lpNode != 0 && lpNode != lpStartNode; lpNode = mUsedNodes.GetPrev(lpNode))
            if (lpNode->GetData()->IsFree() && lpNode->GetData()->GetSize() >= luSize)
                return lpNode;
        return 0;
    }

    // 0x828EB430 - the free block (>= luSize) with the least leftover, scanning the whole
    // list circularly from lpStartNode.
    Heap::HeapEntryNode* Heap::FindFreeNodeBestFit(HeapEntryNode* lpStartNode, u32 luSize)
    {
        HeapEntryNode* lpBest = 0;
        for (HeapEntryNode* lpNode = lpStartNode; lpNode != 0; lpNode = mUsedNodes.GetNext(lpNode))
        {
            HeapEntry* lpEntry = lpNode->GetData();
            if (lpEntry->IsFree() && lpEntry->GetSize() >= luSize &&
                (lpBest == 0 || (lpBest->GetData()->GetSize() - luSize) > (lpEntry->GetSize() - luSize)))
                lpBest = lpNode;
        }
        for (HeapEntryNode* lpNode = mUsedNodes.GetHead(); lpNode != 0 && lpNode != lpStartNode; lpNode = mUsedNodes.GetNext(lpNode))
        {
            HeapEntry* lpEntry = lpNode->GetData();
            if (lpEntry->IsFree() && lpEntry->GetSize() >= luSize &&
                (lpBest == 0 || (lpBest->GetData()->GetSize() - luSize) > (lpEntry->GetSize() - luSize)))
                lpBest = lpNode;
        }
        return lpBest;
    }

    // Addressed allocation (Malloc with a fixed lpAddress) is not on the resource load path;
    // reconstructed when the batch-addressed allocator is brought up.
    Heap::HeapEntryNode* Heap::FindFreeNodeContainingAddress(HeapEntryNode*, u32, void*)
    {
        CGS_ASSERT(false, "Heap::FindFreeNodeContainingAddress not yet reconstructed");
        return 0;
    }

    // ---- allocate / free ----------------------------------------------------------

    // 0x828F4808 - carve luSize from a free block. Pick the block via the find-node policy
    // (fixed address / best-fit / top / bottom), place the allocation at the top or bottom of
    // it, then fix the address-ordered list: insert the new allocated node, shrink (or drop)
    // the chosen free block, and add a free node for any remainder.
    void* Heap::Malloc(u32 luSize, const char* lpcDebugName, void* lpOwner, u16 luStartIndex,
                       u16* lpuFoundIndex, bool lbBestFit, bool lbAllocateFromTop, void* lpAddress)
    {
        CGS_ASSERT(reinterpret_cast<uintptr_t>(lpOwner) != 0xFFFFFFFFu, "reinterpret_cast<uintptr_t>(lpOwner) != 0xFFFFFFFF");   // :274

        if (luSize == 0)
            luSize = 1;
        const u32 luAlignedSize = (muHeapAlignment + luSize - 1) & ~(muHeapAlignment - 1);

        // FromBottom searches backward from the tail; the rest search forward from the head.
        const bool lbFromBottom = (lpAddress == 0 && !lbBestFit && !lbAllocateFromTop);
        HeapEntryNode* lpStartNode;
        if (luStartIndex == 0xFFFF)
            lpStartNode = lbFromBottom ? mUsedNodes.GetTail() : mUsedNodes.GetHead();
        else
            lpStartNode = &mpNodes[luStartIndex];

        HeapEntryNode* lpFreeNode;
        if (lpAddress)               lpFreeNode = FindFreeNodeContainingAddress(lpStartNode, luAlignedSize, lpAddress);
        else if (lbBestFit)          lpFreeNode = FindFreeNodeBestFit(lpStartNode, luAlignedSize);
        else if (!lbAllocateFromTop) lpFreeNode = FindFreeNodeFromBottom(lpStartNode, luAlignedSize);
        else                         lpFreeNode = FindFreeNodeFromTop(lpStartNode, luAlignedSize);

        if (lpFreeNode == 0)
            return 0;

        // [FLAG PC boot gate] Bail out BEFORE mutating the free block when the node pool is
        // exhausted -- the X360 asserts in GetNewNode and carries on, which here corrupts the
        // address-ordered list. Returning null makes the caller
        // (CgsResource::Pool::AllocateMemoryForResource) report OUT-OF-MEMORY for that one
        // resource, which the bundle loader already handles. 2026-08-24: the original trigger
        // (inert unload leg) is fixed, but a long DRIVING run can still fragment a pool's heap
        // because the console's relocating defragmenter is not reconstructed yet; the game-data
        // pools now get 2*maxResources+1 nodes (BrnGameDataModule::CreatePools), which makes
        // exhaustion structurally impossible there, so this gate is a belt-and-braces guard for
        // the remaining hand-sized pools. DELETE when the defragmenter lands.
        if (mUnusedNodes.IsEmpty())
        {
            static bool sbLoggedNodeExhaustion = false;
            if (!sbLoggedNodeExhaustion && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLoggedNodeExhaustion = true;
                *CgsDev::Log::gpDebugPrint
                    << "CgsResource::Heap: out of heap NODES -- refusing further allocations"
                       " in this heap [FLAG PC boot gate]\n";
            }
            return 0;
        }

        HeapEntry*  lpFreeEntry = lpFreeNode->GetData();
        char* const lpcFreeAddr = lpFreeEntry->GetAddress();
        const u32   luFreeSize  = lpFreeEntry->GetSize();

        char* lpcAllocAddr;
        if (lpAddress)            lpcAllocAddr = static_cast<char*>(lpAddress);
        else if (lbAllocateFromTop) lpcAllocAddr = lpcFreeAddr;
        else                        lpcAllocAddr = lpcFreeAddr + luFreeSize - luAlignedSize;

        muAmountFree -= luAlignedSize;
        if (luFreeSize == muLargestFree)
            mbNeedToRecalcLargestFree = true;

        // The allocated node, inserted into the address-ordered list right after the block.
        HeapEntryNode* lpAllocNode = GetNewNode(lpcAllocAddr, static_cast<s32>(luAlignedSize), true, lpcDebugName, lpOwner);
        if (lpuFoundIndex)
            *lpuFoundIndex = mUsedNodes.GetNodeOffset(lpAllocNode);
        mUsedNodes.AddAfter(lpFreeNode, lpAllocNode);

        // Shrink the chosen block to the space before the allocation; drop it if nothing left.
        const u32 luBeforeSize = static_cast<u32>(lpcAllocAddr - lpcFreeAddr);
        const u32 luAfterSize  = luFreeSize - luBeforeSize - luAlignedSize;
        lpFreeEntry->SetSize(luBeforeSize);
        if (luBeforeSize == 0)
        {
            mUsedNodes.RemoveNode(lpFreeNode);
            RemoveNode(lpFreeNode);
        }

        // A free node for the space after the allocation, if any.
        if (luAfterSize != 0)
        {
            HeapEntryNode* lpAfterNode = GetNewNode(lpcAllocAddr + luAlignedSize, static_cast<s32>(luAfterSize), false, 0, 0);
            mUsedNodes.AddAfter(lpAllocNode, lpAfterNode);
        }

        ++miAllocationNumber;
        mbUpdated = true;
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpcAllocAddr) % muHeapAlignment) == 0, "((int)lpcAddress % muHeapAlignment) == 0");   // :471
        return lpcAllocAddr;
    }

    // 0x828F4F80 - free the block at node luNodeIndex, coalescing with a free lower-address
    // neighbour (extend backward) and/or a free higher-address neighbour (extend forward).
    void Heap::Free(u16 luNodeIndex)
    {
        HeapEntryNode* lpNode  = &mpNodes[luNodeIndex];
        HeapEntry*     lpEntry = lpNode->GetData();
        CGS_ASSERT(lpEntry->IsAllocated(), "lpNode->GetData()->IsAllocated()");                                       // :531
        CGS_ASSERT(reinterpret_cast<uintptr_t>(lpEntry->GetOwner()) != 0xFFFFFFFFu, "reinterpret_cast<uintptr_t>(lpNode->GetData()->GetOwner()) != 0xFFFFFFFF");   // :532

        const u32 luFreedSize = lpEntry->GetSize();
        lpEntry->SetAllocated(false);

        HeapEntryNode* lpPrev = mUsedNodes.GetPrev(lpNode);
        if (lpPrev != 0 && lpPrev->GetData()->IsFree())
        {
            lpEntry->SetAddress(lpEntry->GetAddress() - lpPrev->GetData()->GetSize());
            lpEntry->SetSize(lpPrev->GetData()->GetSize() + lpEntry->GetSize());
            mUsedNodes.RemoveNode(lpPrev);
            RemoveNode(lpPrev);
        }

        HeapEntryNode* lpNext = mUsedNodes.GetNext(lpNode);
        if (lpNext != 0 && lpNext->GetData()->IsFree())
        {
            lpEntry->SetSize(lpNext->GetData()->GetSize() + lpEntry->GetSize());
            mUsedNodes.RemoveNode(lpNext);
            RemoveNode(lpNext);
        }

        muAmountFree += luFreedSize;
        if (lpEntry->GetSize() > muLargestFree)
            muLargestFree = lpEntry->GetSize();
    }

    // Free the allocated block at an address. Only reached on the pool's alloc-failure cleanup path
    // (AllocateMemoryForResource backs out the pools it already carved when a later pool runs out of
    // room) -- which never happens for a correctly-sized pool. The address->node resolution
    // (FindFreeNodeContainingAddress / the allocated-node walk) is deferred, so this is a no-op for
    // now; an undersized pool already asserts at the call site. Use Free(u16) for normal frees.
    void Heap::Free(void* /*lpPtr*/)
    {
    }

    // Reset to an unprepared, empty state (the X360 Construct Init's both index-lists).
    void Heap::Construct()
    {
        mUnusedNodes.Init(0, 0);
        mUsedNodes.Init(0, 0);
        mbPrepared = false;
    }
}
