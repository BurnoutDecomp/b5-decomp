#ifndef GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
#define GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// GameShared/GameClasses/Containers/CgsIndexedPool.h
//
// CgsContainers::IndexedPool<T, N> -- a fixed-capacity object pool that hands out
// stable integer handles. Live objects live in maElements[]; the free slots are
// tracked by a list of free indices (mpFreeIndices[0 .. muNumFree)). operator[]
// resolves a handle back to its element, asserting (non-fatally) that the handle is
// in range AND that the slot is currently allocated (not on the free list).
//
// SLICE -- operator[] (the indexed accessor) plus Clear / GetObjectIndex / PushIndex,
// all reconstructed from the X360 out-of-line instantiation
//   CgsContainers::IndexedPool<BrnResource::GameDataEventSlot, short>::*
//     operator[](s16)      @ 0x82663010  (asserts CgsIndexedPool.h:241 / :250)
//     Clear()              @ 0x826638D0
//     GetObjectIndex(T*)   @ 0x82662F50  (assert CgsIndexedPool.h:222 "Object not in pool")
//     PushIndex(s16)       @ 0x82663180  (asserts :301 "Object not in pool", :310 "Object is already free")
// (IDA-truncated symbol "BrnResource::GameDataEventSlot,short>"). The element base /
// free-list / count fields are modeled at the exact byte offsets the asm reads. The
// pool's construction, Allocate/Free proper, iteration and the rest of the class are
// DEFERRED to their own reconstruction pass -- GROW this header additively when they
// land; do NOT fork the pool elsewhere.
//
// FLAG (additive correction, X360-authoritative): the prior commit modeled +0x08 as an
// honest unnamed `muPad08` because operator[] never touches it. Clear @0x826638D0 and
// PushIndex @0x82663180 now PROVE +0x08 is a real s16 live/allocated count -- Clear sets
// it to 0, PushIndex decrements it when an index is returned to the free list. It is
// renamed msNumAllocated here. No prior TU referenced muPad08 by name (it was padding),
// so the rename is safe; the byte offset (+0x08) and the size (s16) are unchanged.
//
// FLAG: this is a layout-faithful model of the generic, not the full IndexedPool surface.
// The byte layout (+0 element base, +4 free-index array, +0x08 num-allocated, +0x0A
// num-free, +0x0C capacity) is proven by the X360 asm of these four methods; the +0x06
// short is not touched by any reconstructed method and stays honest unnamed padding.
// ============================================================================

namespace CgsContainers
{

// IndexType is the free-list / count field type spelled by the demangler's second template
// tail (the "<...,short>" / "<...,char>" the IDA symbols show). It controls BOTH the width of
// the free-index list entries AND the width of the three live/free/capacity count fields, which
// is why the byte-indexed pools (IndexType=s8) pack their counts at +0x08/+0x09/+0x0A while the
// short-indexed pools (IndexType=s16) land them at +0x08/+0x0A/+0x0C. Defaulted to s16 so the
// pre-existing <T,N> instantiations keep their recovered layout unchanged. N stays the runtime
// capacity placeholder (msCapacity is the live field; N is not read by any reconstructed method).
template <typename T, s32 N, typename IndexType = s16>
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
    void Construct(T* lpElements, IndexType* lpFreeIndices, IndexType lsCapacity)
    {
        maElements     = lpElements;
        mpFreeIndices  = lpFreeIndices;
        msCapacity     = lsCapacity;
        Clear();
    }

    // Clear @ 0x826638D0: reset the free list to the identity permutation
    // (mpFreeIndices[i] = i for i in [0, msCapacity)), then mark every slot free --
    // msNumAllocated = 0, msNumFree = msCapacity. X360 loops over the capacity (`lhz
    // 0xC`) filling the s16 free-index array, then stores 0 at +0x08 and the capacity
    // at +0x0A. The fastcall "return result" is a register artifact; Clear is void.
    void Clear()
    {
        const IndexType lsCapacity = msCapacity;
        for (IndexType lsIndex = 0; lsIndex < lsCapacity; ++lsIndex)
        {
            mpFreeIndices[lsIndex] = lsIndex;
        }
        msNumAllocated = 0;
        msNumFree      = msCapacity;
    }

    // Get @ 0x828DFD38 (CgsResource::ResourceModule::ProcessPendingFileSystemResponses, char-pool):
    // gather pointers to every CURRENTLY-ALLOCATED element into the caller's buffer and return how
    // many were written. X360: assert the supplied buffer holds at least msCapacity entries
    // ("You must supply a buffer to contain a minimum of all the objects in the pool", :400 -- the
    // asm asserts when lsBufferCapacity < msCapacity). Then (1) fill lppOutBuffer[i] = &maElements[i]
    // for every slot i in [0, msCapacity); (2) null out the entries whose index is on the free list
    // (lppOutBuffer[mpFreeIndices[f]] = 0 for f in [0, msNumFree)); (3) compact the surviving
    // non-null pointers to the front, counting them. Returns the live count. Additive: the original
    // header did not model Get; recovered from the char-pool asm of this instantiation.
    s32 Get(T** lppOutBuffer, s32 liBufferCapacity) const
    {
        CGS_ASSERT(liBufferCapacity >= msCapacity,
                   "You must supply a buffer to contain a minimum of all the objects in the pool\n");

        const s32 liCapacity = static_cast<s32>(msCapacity);
        for (s32 li = 0; li < liCapacity; ++li)
        {
            lppOutBuffer[li] = const_cast<T*>(&maElements[li]);
        }

        const s32 liNumFree = static_cast<s32>(msNumFree);
        for (s32 lf = 0; lf < liNumFree; ++lf)
        {
            lppOutBuffer[static_cast<s32>(mpFreeIndices[lf])] = 0;
        }

        s32 liLive = 0;
        for (s32 li = 0; li < liCapacity; ++li)
        {
            if (lppOutBuffer[li])
            {
                lppOutBuffer[liLive] = lppOutBuffer[li];
                ++liLive;
            }
        }
        return liLive;
    }

    // Allocate (front half of an alloc; recovered from the X360 CgsFileSystem::PhysicalX360Device
    // Open @0x828F9728 / OpenDirectory @0x828F9E40, which inline this pool's claim-a-slot): pop a
    // free index. X360: read msNumFree; when it is 0 the pool is full and -1 is returned ("Out of
    // handles"). Otherwise take the FIRST free index (mpFreeIndices[0]) as the result, move the
    // LAST free entry into slot 0 (the unordered-swap pop the asm does: `*free = free[numFree-1]`),
    // bump msNumAllocated and drop msNumFree. Returns the allocated handle, or -1 when full.
    IndexType Allocate()
    {
        const IndexType lsNumFree = msNumFree;
        if (lsNumFree == 0)
        {
            return static_cast<IndexType>(-1);
        }

        const IndexType lsResult = mpFreeIndices[0];
        mpFreeIndices[0] = mpFreeIndices[lsNumFree - 1];
        ++msNumAllocated;
        msNumFree = static_cast<IndexType>(lsNumFree - 1);
        return lsResult;
    }

    // Pop @ 0x828F2890 (CgsResource::ResourceModule::AddOpen/CloseFileRequest /
    // AddOpen/CloseReadStreamRequest): claim a free slot and return the OBJECT pointer rather
    // than the handle. X360: index = Allocate(); when the pool is full (index == -1) return 0,
    // else return &maElements[index] (the asm's `12*index + maElements` for the 12-byte
    // ResourcePoolRecord instantiation -- the stride is sizeof(T)). DWARF CgsIndexedPool.h:330.
    T* Pop()
    {
        const IndexType lsIndex = Allocate();
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
    IndexType GetObjectIndex(const T* lpObject) const
    {
        const IndexType lsIndex = static_cast<IndexType>(lpObject - maElements);

        CGS_ASSERT(lsIndex >= 0 && lsIndex < msCapacity, "Object not in pool\n");

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
        CGS_ASSERT(lsIndex < msCapacity, "Object not in pool\n");

        IndexType       lsScanned = 0;
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
        CGS_ASSERT(lsScanned == lsNumFree, "Object is already free\n");

        mpFreeIndices[msNumFree] = lsIndex;
        --msNumAllocated;
        ++msNumFree;
    }

    // Push @ 0x828EB138 (CgsResource::ResourceModule::ProcessPendingFileSystemResponses, char-pool):
    // the X360 emitted a Push(IndexType) symbol with a body BYTE-IDENTICAL to PushIndex (same
    // range-guard :301, same "Object is already free" free-list scan :310, same append +
    // --msNumAllocated / ++msNumFree). It is the public free-an-index entry point that PushIndex
    // backs; modeled as a thin forward so the two attested symbols share one body.
    void Push(IndexType lsIndex)
    {
        PushIndex(lsIndex);
    }

private:
    // ---- layout (X360 byte offsets; widths follow IndexType) ----
    // IndexType=s16 (default): mpFreeIndices@+4, counts @ +0x08/+0x0A/+0x0C (the short-pool
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

} // namespace CgsContainers

#endif // GAMESHARED_GAMECLASSES_CONTAINERS_CGSINDEXEDPOOL_H
