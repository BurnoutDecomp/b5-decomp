#pragma once

// ===========================================================================
// EATech Apt -- DOGMA pool allocator.
//
// Reconstructed from the Feb-2007 partial source (SDKs/Packages/Apt/2.00.00/include/Apt/
// DogmaAllocator.h) for SHAPE, and from the X360 ARTIST.XEX pseudocode for the
// method BODIES that the leak only declared:
//     DOGMA_PoolManager::DOGMA_PoolManager           @ 0x82ADB850
//     DOGMA_PoolManager::~DOGMA_PoolManager          @ 0x82ADB950
//     DOGMA_PoolManager::`scalar deleting destructor'@ 0x82ADD1D8
//     DOGMA_PoolManager::Allocate                    @ 0x82AE1058
//     DOGMA_PoolManager::Deallocate                  @ 0x82AE1308
//
// This is vendor/SDK code reconstructed in its canonical home (the leak path is
// SDKs/Packages/Apt/2.00.00; the already-landed Apt sibling ObjectIndex sits at
// SDKs/EATech/Apt/, so for consistency this lands beside it). Per the naming
// convention, the EA SDK identifiers (DOGMA_PoolManager, _DOGMA_MemPool,
// Allocate, Deallocate, mnPoolFree, ...) are preserved verbatim -- they are an
// external/middleware API, not project-owned code.
//
// A DOGMA_PoolManager is a fixed-size-bucket free-list allocator backed by one
// or more contiguous "pools". Each requested size is rounded up to a 4-byte
// multiple (and clamped to mnMinimumAllocationSize) and indexes a per-size
// free-list head stored in mpaFirstFreeBySize. Requests larger than
// mnMaxSizeAllocation fall back either to a tracked "outside allocation" wrapper
// (when mbTrackOutsideAllocations) or to a bare DOGMA_Malloc.
// ===========================================================================

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// DOGMA global allocator hooks.
//
// The X360 binary reaches the underlying heap through three indirect calls
// (function pointers in .data): dword_8324E818 / dword_8324E81C / dword_8324E820.
// They are the classic DOGMA malloc / free / sized-free install hooks. Declared
// here and defined by the platform's DOGMA heap layer (another TU); a forward
// declaration is all this allocator needs to compile and link.
//   dword_8324E818(size)        -> allocate `size` bytes              (DOGMA_Malloc)
//   dword_8324E81C(ptr)         -> free a block (no size)             (DOGMA_Free)
//   dword_8324E820(ptr, size)   -> free a block of known size         (DOGMA_FreeSized)
// ---------------------------------------------------------------------------
void* DOGMA_Malloc(size_t nSize);
void  DOGMA_Free(void* pBlock);
void  DOGMA_FreeSized(void* pBlock, size_t nSize);

// ---------------------------------------------------------------------------
// DOGMA interrupt-masking spin lock.
//
// The X360 free-list mutations are bracketed by an interrupt-disabling atomic
// test-and-set spinlock (the mfmsr/mtmsree/lwarx/stwcx. idiom on the globals
// unk_8324E720 / unk_8324E758 / unk_8324E8EC -- one lock per free-list family:
// outside allocations, the by-size free lists, and the overflow pool list).
// Modelled here as a portable test-and-set lock; the three global instances are
// defined in DogmaAllocator.cpp.
// ---------------------------------------------------------------------------
class DOGMA_SpinLock
{
public:
    void Lock();
    void Unlock();

private:
    volatile long mlHeld;
};

typedef struct _DOGMA_MemPool
{
    void SetupPool(struct _DOGMA_MemPool* pNextPool, size_t nSize)
    {
        mnPoolFree = mnPoolSize = (nSize - sizeof(struct _DOGMA_MemPool) + 1);
        mpNextPool = pNextPool;
    }

    struct _DOGMA_MemPool* GetNextPool() const
    {
        return mpNextPool;
    }

    bool CanFitBytes(size_t nBytes) const
    {
        return (mnPoolFree >= nBytes);
    }

    void* ConsumeBytes(size_t nBytes)
    {
        size_t mOldFree = mnPoolFree;
        mnPoolFree -= nBytes;
        return &(&mPoolStart)[mnPoolSize - mOldFree];
    }

    void* GetFirstItem()
    {
        return &mPoolStart;
    }

    bool PtrIsInThisPool(const void* ptr) const
    {
        if (ptr >= &mPoolStart && ptr < &((&mPoolStart)[mnPoolSize - mnPoolFree]))
            return true;
        else
            return false;
    }

    size_t GetBytesUsed()
    {
        return mnPoolSize - mnPoolFree;
    }

protected:

    struct _DOGMA_MemPool* mpNextPool;
    size_t mnPoolSize;
    size_t mnPoolFree;
    uint8_t mPoolStart;
} DOGMA_MemPool;


typedef struct _AllocationDataPerSize
{
    uint32_t nAllocationCount;
    size_t nCurrentlyAllocated;
    size_t nMaxAllocatedAtAnyOneTime;
} AllocationDataPerSize;

typedef struct _TrackedAllocation
{
    void* pAllocated;
    uint32_t nLineNumber;
    const char* szFileName;
    uint32_t nAllocSize;
} TrackedAllocation;


class DOGMA_PoolManager
{
public:
    static void* operator new(size_t size);
    static void operator delete(void* p, size_t size);
    static void* operator new[](size_t size);
    static void operator delete[](void* p);

    DOGMA_PoolManager(size_t mainPoolSizeBytes,
        size_t overflowPoolSizeBytes,
        size_t minSizeAllocation,
        size_t maxSizeAllocation,
        uint8_t nOffsetToStoreNextInFreeItem,
        bool bStoreFreeBlockSize, uint8_t nOffsetToStoreSizeInFreeItem,
        bool bTrackOusideAllocations);

    ~DOGMA_PoolManager();

    void* Allocate(size_t nAllocatedSize);
    bool Deallocate(void* pNowFree, size_t nAllocatedSize);

    size_t GetTotalBytesUsed();

    void* GetFirstOutsideAllocation();
    void* GetNextOutsideAllocation(const void* pCurrent);

protected:

    void* ConsumeFreeBlockBySize(size_t nSize);
    void AddFreeBlockBySize(void* pNowFree, size_t nSize);
    size_t ToNextValidSize(size_t nSize);

    // ADDITIVE GROW (FLAGGED): protected pool/outside-list accessors used by
    // the AptValueGC_PoolManager subclass to walk live items (GetFirst/
    // GetNextAptValue @ 0x82AE0DF8 / 0x82AE0BE0). The X360 GC walk reads
    // mpFirstPool / mpFirstOutSideAllocation / mbTrackOutsideAllocations
    // directly; exposed here by name (rather than re-deriving offsets) so the
    // subclass need not friend or offset-cast. No layout change.
    DOGMA_MemPool* GetFirstPool() const { return mpFirstPool; }
    void*  GetFirstOutsideAllocationRaw() const { return mpFirstOutSideAllocation; }
    bool   GetTracksOutsideAllocations() const { return mbTrackOutsideAllocations != 0; }

    uintptr_t** mpaFirstFreeBySize;

    DOGMA_MemPool* mpFirstPool;

    const size_t mnOverflowPoolSize;


    const size_t mnMaxSizeAllocation;


    uint32_t mnOffsetToStoreNext;
    uint32_t mnOffsetToStoreSize;
    uint32_t mnMinimumAllocationSize;
    uint32_t mbStoreFreeBlockSize;
    uint32_t mbTrackOutsideAllocations;


    typedef struct _OutsideAllocationT
    {
    public:
        struct _OutsideAllocationT* pNext;
        struct _OutsideAllocationT* pPrev;

        void* GetReturnedPointer()
        {
            return &returnedMemory[0];
        }

        static size_t GetStructOverHead()
        {
            return 2 * sizeof(struct _OutsideAllocationT*);
        }

        static struct _OutsideAllocationT* GetStructPointerFromReturnedPointer(const void* pReturned)
        {
            return (struct _OutsideAllocationT*)(((uint8_t*)pReturned) - GetStructOverHead());
        }

    private:

        uint8_t returnedMemory[1];

    } OutsideAllocationT;

    OutsideAllocationT* mpFirstOutSideAllocation;

public:

    uint32_t mnItemsAllocated;
    uint32_t mnItemsFreed;

private:

    DOGMA_PoolManager& operator=(const DOGMA_PoolManager& r);
};
