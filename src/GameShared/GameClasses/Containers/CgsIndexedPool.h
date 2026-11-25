#ifndef GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
#define GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#define CGS_INDEXED_POOL_ARTIST_HEADER "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsIndexedPool.h"

// DecFIGS CgsIndexedPool.h:33-81. The runtime pool stores object and free-index
// pointers followed by the used, free and capacity counts. The index type sets
// the count widths. FixedIndexedPool owns the compile-time backing extents.
namespace CgsContainers
{

template <typename T, typename IndexType>
class IndexedPool
{
public:
    // Resolve a handle to its element. X360 operator[](s16): range-guard the index
    // against the capacity ("Object not in pool"), then verify the slot is NOT on the
    // free list ("Object is free"), then return &maElements[luIndex]. Both guards are
    // non-fatal (the binary returns the element pointer regardless). The element stride
    // is sizeof(T) (== 0x30 for the GameDataEventSlot instantiation).
    T& operator[](IndexType lsIndex)
    {
        CGS_ASSERT(lsIndex >= 0 && lsIndex < msCapacity, "Object not in pool\n");

        // Walk the free-index list; lsScanned counts the entries inspected before a
        // match (it stays one short of msNumFree iff lsIndex was found on the list).
        IndexType lsScanned = 0;
        const IndexType lsNumFree = msNumFree;
        if (lsNumFree > 0)
        {
            for (IndexType lsFree = 0; lsFree < lsNumFree; ++lsFree)
            {
                if (mpFreeIndices[lsFree] == lsIndex)
                {
                    break;
                }
                lsScanned = static_cast<IndexType>(lsFree + 1);
            }
        }
        CGS_ASSERT(lsScanned == lsNumFree, "Object is free\n");

        return maElements[lsIndex];
    }

    const T& operator[](IndexType lsIndex) const
    {
        return const_cast<IndexedPool*>(this)->operator[](lsIndex);
    }

    // ADDITIVE (attested by @0x82671B90): bind the pool to its backing element / free-index
    // arrays and reset it. BrnResource::GameDataModule::Construct wires its event-slot pool
    // inline with exactly these stores -- capacity (96) at +0x0C, the element array base
    // (a1+439328) at +0, the free-index array (a1+443936) at +4, then Clear() -- so the
    // construction step is modeled as this named member on the generic. (The X360 also zeroes
    // two neighbouring module fields in the same run; those belong to the module, not the pool.)
    void Construct(T* lpElements, IndexType* lpFreeIndices, s32 liCapacity)
    {
        maElements     = lpElements;
        mpFreeIndices  = lpFreeIndices;
        msCapacity     = static_cast<IndexType>(liCapacity);
        Clear();
    }

    // Clear @ 0x826638D0: reset the free list to the identity permutation
    // (mpFreeIndices[i] = i for i in [0, msCapacity)), then mark every slot free --
    // msNumAllocated = 0, msNumFree = msCapacity. X360 loops over the capacity (`lhz
    // 0xC`) filling the s16 free-index array, then stores 0 at +0x08 and the capacity
    // at +0x0A. The fastcall "return result" is a register artifact; Clear is void.
    __declspec(noinline) void Clear()
    {
        for (IndexType lsIndex = 0; lsIndex < msCapacity; ++lsIndex)
        {
            mpFreeIndices[lsIndex] = lsIndex;
        }
        msNumAllocated = 0;
        msNumFree      = msCapacity;
    }

    // ADDITIVE (crash parity FX-OCTREE, 2026-09-25): the three count accessors, DWARF CgsIndexedPool.h:171 /
    // :187 / :203 (`uint16_t GetPoolSize()` / `GetNumFree()` / `GetNumUsed()` on
    // IndexedPool<CgsSceneManager::LooseOctree::LooseOctreeNodeAllocation, std::uint16_t>; not const in the
    // DWARF). Inlined at every console reader of the loose octree's mFreeNodeGroupPool (X360 +0x446A8):
    //   GetPoolSize  StartFrustumTestJobs @0x828B24BC (`lhzx +0x446B4` -> the job's node count)
    //   GetNumFree   AdaptiveDepthUpdateAddNodesRecursive @0x828CA394 and SplitAndPropogateRecursive
    //                @0x828BC304 (`lhz +0x446B2`; assert text "mFreeNodeGroupPool.GetNumFree() > 0", :815)
    //   GetNumUsed   AdaptiveDepthUpdateRemoveNodesRecursive @0x828CA520 and Construct's miNumStaticNodes
    //                latch (`lhzx +0x446B0` @0x828C9E90 -> `stwx +0x44684` @0x828C9EF8)
    IndexType GetPoolSize() { return msCapacity; }
    IndexType GetNumFree()  { return msNumFree; }
    IndexType GetNumUsed()  { return msNumAllocated; }

    // GetUsedList @ 0x828DFD38 (CgsResource::ResourceModule::ProcessPendingFileSystemResponses, char-pool):
    // gather pointers to every CURRENTLY-ALLOCATED element into the caller's buffer and return how
    // many were written. X360: assert the supplied buffer holds at least msCapacity entries
    // ("You must supply a buffer to contain a minimum of all the objects in the pool", :400 -- the
    // asm asserts when lsBufferCapacity < msCapacity). Then (1) fill lppOutBuffer[i] = &maElements[i]
    // for every slot i in [0, msCapacity); (2) null out the entries whose index is on the free list
    // (lppOutBuffer[mpFreeIndices[f]] = 0 for f in [0, msNumFree)); (3) compact the surviving
    // non-null pointers to the front, counting them. Returns the live count. Additive: the original
    // header did not model Get; recovered from the char-pool asm of this instantiation.
    s32 GetUsedList(T** lpUsedList, s32 liMaxListLength)
    {
        CGS_ASSERT_STREAM_AT(liMaxListLength >= msCapacity,
            "You must supply a buffer to contain a minimum of all the objects in the pool\n",
            CGS_INDEXED_POOL_ARTIST_HEADER, 400);

        s32 liIndex;
        s32 liNewIndex;
        for (liIndex = 0; liIndex < msCapacity; ++liIndex)
            lpUsedList[liIndex] = &maElements[liIndex];
        for (liIndex = 0; liIndex < msNumFree; ++liIndex)
            lpUsedList[mpFreeIndices[liIndex]] = 0;
        for (liIndex = 0, liNewIndex = 0; liIndex < msCapacity; ++liIndex)
        {
            if (lpUsedList[liIndex])
                lpUsedList[liNewIndex++] = lpUsedList[liIndex];
        }
        return liNewIndex;
    }

    // PopIndex (DecFIGS CgsIndexedPool.h:267; recovered from the X360 CgsFileSystem::PhysicalX360Device
    // Open @0x828F9728 / OpenDirectory @0x828F9E40, which inline this pool's claim-a-slot): pop a
    // free index. X360: read msNumFree; when it is 0 the pool is full and -1 is returned ("Out of
    // handles"). Otherwise take the FIRST free index (mpFreeIndices[0]) as the result, move the
    // LAST free entry into slot 0 (the unordered-swap pop the asm does: `*free = free[numFree-1]`),
    // bump msNumAllocated and drop msNumFree. Returns the allocated handle, or -1 when full.
    IndexType PopIndex()
    {
        if (msNumFree == 0)
        {
            return static_cast<IndexType>(-1);
        }

        const IndexType lsResult = mpFreeIndices[0];
        mpFreeIndices[0] = mpFreeIndices[msNumFree - 1];
        ++msNumAllocated;
        --msNumFree;
        return lsResult;
    }

    // Pop @ 0x828F2890 (CgsResource::ResourceModule::AddOpen/CloseFileRequest /
    // AddOpen/CloseReadStreamRequest): claim a free slot and return the OBJECT pointer rather
    // than the handle. X360: index = PopIndex(); when the pool is full (index == -1) return 0,
    // else return &maElements[index] (the asm's `12*index + maElements` for the 12-byte
    // PendingFileResponse instantiation -- the stride is sizeof(T)). DWARF CgsIndexedPool.h:330.
    T* Pop()
    {
        const IndexType lsIndex = PopIndex();
        if (lsIndex == static_cast<IndexType>(-1))
        {
            return 0;
        }
        return &maElements[lsIndex];
    }

    // GetObjectIndex @ 0x82662F50: recover a live object's handle from its address.
    // X360: index = (s16)((lpObject - maElements) / sizeof(T)); then range-guard
    // [0, msCapacity) ("Object not in pool", CgsIndexedPool.h:222, non-fatal) and
    // return the index. Pointer subtraction already scales by sizeof(T) (== the 48-byte
    // stride the asm divides by), so this is byte-faithful. The returned index is the
    // sign-extended 16-bit difference (X360 `extsh`).
    IndexType GetObjectIndex(const T* lpObject)
    {
        const IndexType lsIndex = static_cast<IndexType>(lpObject - maElements);

        CGS_ASSERT_STREAM_AT(lsIndex >= 0 && lsIndex < msCapacity, "Object not in pool\n",
            CGS_INDEXED_POOL_ARTIST_HEADER, 222);

        return lsIndex;
    }

    // PushIndex @ 0x82663180: return a handle to the free list (the back half of
    // Free). X360: range-guard lsIndex < msCapacity ("Object not in pool",
    // CgsIndexedPool.h:301); scan the free list over msNumFree entries asserting the
    // index is NOT already present ("Object is already free", :310 -- lsScanned stays
    // one short of msNumFree iff a match was hit); append lsIndex at
    // mpFreeIndices[msNumFree]; decrement msNumAllocated; increment msNumFree. Both
    // asserts are non-fatal (the binary mutates the list regardless). The fastcall
    // "return result" is the receiver pointer (register artifact); modeled void.
    void PushIndex(IndexType lsIndex)
    {
        CGS_ASSERT_STREAM_AT(lsIndex < msCapacity, "Object not in pool\n",
            CGS_INDEXED_POOL_ARTIST_HEADER, 301);

        CGS_ASSERT_STREAM_AT(!DebugIsIndexFree(lsIndex), "Object is already free\n",
            CGS_INDEXED_POOL_ARTIST_HEADER, 310);

        mpFreeIndices[msNumFree] = lsIndex;
        --msNumAllocated;
        ++msNumFree;
    }

    // DecFIGS CgsIndexedPool.h:374. Scan the live part of the free-index array.
    __forceinline bool DebugIsIndexFree(IndexType lsIndex)
    {
        const IndexType lsNumFree = msNumFree;
        IndexType lsFree = 0;
        while (lsFree < msNumFree && mpFreeIndices[lsFree] != lsIndex)
            ++lsFree;

        return lsFree != lsNumFree;
    }

    // DecFIGS CgsIndexedPool.h:357. The resource response consumer
    // releases the typed record through its signed pool index.
    void Push(T* lpObject)
    {
        PushIndex(GetObjectIndex(lpObject));
    }

private:
    // ---- layout (X360 byte offsets; widths follow IndexType) ----
    // IndexType=s16: mpFreeIndices@+4, counts @ +0x08/+0x0A/+0x0C (the short-pool
    //   layout the committed <GameDataEventSlot,N> instantiation recovered: `lhz/sth` halfwords).
    // IndexType=s8: counts pack at +0x08/+0x09/+0x0A and the free list is a byte array (the
    //   char-pool layout the CgsResource/CgsFileSystem instantiations recover: `lbz/stb` bytes,
    //   `extsb` sign-extends, divide-by-sizeof(T) for GetObjectIndex).
    T*         maElements;      // +0x00  live element array (the X360 `*a1` base)
    IndexType* mpFreeIndices;   // +0x04  free-slot index list (`*(a1+4)`, IndexType entries)
    IndexType  msNumAllocated;  // +0x08  live/allocated count (Clear -> 0, PushIndex --)
    IndexType  msNumFree;       // +N(s16:+0x0A / s8:+0x09)  number of free slots
    IndexType  msCapacity;      // +N(s16:+0x0C / s8:+0x0A)  pool capacity
};

// ARTIST 0x829056FC binds the embedded records at +0x0C and the free list at +0xCC.
template<typename T, s32 N, typename IndexType>
struct FixedIndexedPool : public IndexedPool<T, IndexType>
{
    void Construct()
    {
        IndexedPool<T, IndexType>::Construct(maObjects, maiFreeIndices, N);
    }

private:
    T maObjects[N];
    IndexType maiFreeIndices[N];
};

template<typename T, s32 N>
struct IndexedPool8 : public FixedIndexedPool<T, N, s8> {};

} // namespace CgsContainers

#undef CGS_INDEXED_POOL_ARTIST_HEADER

#endif // GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
