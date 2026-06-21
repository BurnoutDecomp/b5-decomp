#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"

// CgsMemory::HeapMalloc - the engine's general-purpose heap. A thin wrapper over an embedded
// EA::Allocator::GeneralAllocator (vendor/PPMalloc): Construct adopts the caller's buffer as the
// allocator's core, and Malloc/Free/ReAlloc/CAlloc delegate to it. The GeneralAllocator's empty
// state is set up by its ctor (which runs when HeapMalloc is constructed); Construct then AddCore's
// the buffer. (The allocation engine is the user-approved PC-leaf boundary-tag allocator over the
// adopted buffer - see EAGeneralAllocator.cpp - preserving the fixed-memory-budget semantics.)
namespace CgsMemory
{
    // @ 0x82866680 - adopt [lpBuffer, lpBuffer+lnBufferSize) as this heap's memory.
    void HeapMalloc::Construct(void* lpBuffer, s32 lnBufferSize)
    {
        mpBuffer     = lpBuffer;
        mnBufferSize = lnBufferSize;
        mAllocator.AddCore(lpBuffer, static_cast<size_t>(lnBufferSize));
    }

    // X360: GeneralAllocator::Shutdown(this heap's allocator). The buffer is owned by the caller.
    void HeapMalloc::Destruct()
    {
        mAllocator.Shutdown();
        mpBuffer     = 0;
        mnBufferSize = 0;
    }

    bool HeapMalloc::Prepare()
    {
        return true;
    }

    // @ 0x82866780 - allocate lnSize bytes, lnAlignment-aligned, from this heap (null if full).
    void* HeapMalloc::Malloc(s32 lnSize, s32 lnAlignment)
    {
        return mAllocator.MallocAligned(static_cast<size_t>(lnSize), static_cast<size_t>(lnAlignment));
    }

    void HeapMalloc::Free(void* lpBlock)
    {
        mAllocator.Free(lpBlock);
    }

    void* HeapMalloc::ReAlloc(void* lpBlock, s32 lnSize)
    {
        return mAllocator.Realloc(lpBlock, static_cast<size_t>(lnSize));
    }

    void* HeapMalloc::CAlloc(s32 lnCount, s32 lnSize)
    {
        return mAllocator.Calloc(static_cast<size_t>(lnCount), static_cast<size_t>(lnSize));
    }

    // @ 0x82866848 - diagnostics; the X360 walks the live allocation list. [STUB] (debug-only;
    // the GeneralAllocator's report path is not part of the PC-leaf engine).
    void HeapMalloc::PrintAllocations()
    {
    }

    // X360 walks the free list for the largest contiguous gap. [STUB] returns the buffer size as a
    // conservative upper bound (used only on the out-of-memory diagnostic path).
    u32 HeapMalloc::GetLargestFreeBlock(bool /*lbContiguous*/)
    {
        return static_cast<u32>(mnBufferSize);
    }
}
