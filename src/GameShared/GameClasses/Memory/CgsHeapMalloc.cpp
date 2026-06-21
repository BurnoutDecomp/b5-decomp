#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsMemory::HeapMalloc - the engine's general-purpose heap. A thin wrapper over an embedded
// EA::Allocator::GeneralAllocator (vendor/PPMalloc): Construct initialises the allocator over the
// caller's buffer (adopting it as the allocator's core, with system-alloc growth disabled), and
// Malloc/Free/ReAlloc/CAlloc delegate to it. (The allocation engine is the user-approved PC-leaf
// boundary-tag allocator over the adopted buffer - see EAGeneralAllocator - preserving the
// fixed-memory-budget semantics.)
//
// HeapMallocCoreAllocator is the EA::Allocator::ICoreAllocator front-end onto a HeapMalloc.
namespace CgsMemory
{
    // --- HeapMalloc ----------------------------------------------------------------------------

    // @ 0x82866680 - initialise this heap over [lpBuffer, lpBuffer+lnBufferSize).
    // Stores the buffer/size, then GeneralAllocator::Init(buffer, size, false, false, 0, 0)
    // (the 6-arg core-adopting overload) followed by SetOption(kOptionEnableSystemAlloc, 0)
    // so the heap is fixed to the supplied buffer and never grows via the OS.
    void HeapMalloc::Construct(void* lpBuffer, s32 lnBufferSize)
    {
        CGS_ASSERT(lpBuffer, "lpBuffer");           // CgsHeapMalloc.cpp:52
        CGS_ASSERT(lnBufferSize > 0, "lnBufferSize>0"); // CgsHeapMalloc.cpp:53

        mpBuffer     = lpBuffer;
        mnBufferSize = lnBufferSize;
        mAllocator.Init(lpBuffer, static_cast<size_t>(lnBufferSize),
                        /*bShouldFreeInitialCore*/ false, /*bShouldTrace*/ false,
                        /*pCoreAddFunction*/ 0, /*pCoreAddFunctionContext*/ 0);
        mAllocator.SetOption(EA::Allocator::GeneralAllocator::kOptionEnableSystemAlloc, 0);
    }

    // @ 0x82866730 - GeneralAllocator::Shutdown(this heap's allocator). The buffer is owned by the
    // caller, so it is not released here.
    void HeapMalloc::Destruct()
    {
        mAllocator.Shutdown();
    }

    // @ 0x82866738 - tear the allocator down and re-Construct it over the same buffer (a full reset).
    bool HeapMalloc::Prepare()
    {
        mAllocator.Shutdown();
        Construct(mpBuffer, mnBufferSize);
        return true;
    }

    // @ 0x82866780 - allocate lnSize bytes, lnAlignment-aligned, from this heap (null if full).
    // X360: MallocAligned(size, alignment, /*offset*/ 0, /*flags*/ 0).
    void* HeapMalloc::Malloc(s32 lnSize, s32 lnAlignment)
    {
        return mAllocator.MallocAligned(static_cast<size_t>(lnSize),
                                        static_cast<size_t>(lnAlignment), 0, 0);
    }

    // @ 0x82866790 - GeneralAllocator::Free(block).
    void HeapMalloc::Free(void* lpBlock)
    {
        mAllocator.Free(lpBlock);
    }

    // @ 0x828667A8 - GeneralAllocator::Realloc(block, newSize, /*flags*/ 0).
    void* HeapMalloc::ReAlloc(void* lpBlock, s32 lnSize)
    {
        return mAllocator.Realloc(lpBlock, static_cast<size_t>(lnSize), 0);
    }

    // @ 0x82866798 - GeneralAllocator::Calloc(count, size, /*flags*/ 0).
    void* HeapMalloc::CAlloc(s32 lnCount, s32 lnSize)
    {
        return mAllocator.Calloc(static_cast<size_t>(lnCount), static_cast<size_t>(lnSize), 0);
    }

    // The per-allocation trace sink the X360 PrintAllocations hands to TraceAllocatedMemory.
    // X360 passed sub_828667B8 (UNIDENTIFIED in the export - its body is not recovered).
    // [FLAG] best-effort stand-in: a no-op sink so the trace walk has a valid callback. The
    // original almost certainly forwarded the formatted record to a CgsDev::Message debug stream
    // (CgsHeapMalloc.cpp:176 TraceFunc, using namespace CgsDev::Message); not fabricated here.
    static void TraceFunc(const char* /*lpcRecord*/, void* /*lpContext*/)
    {
    }

    // @ 0x82866848 - GeneralAllocator::TraceAllocatedMemory(callback, 0, 0, 0): walk the live
    // allocation list and emit each record through the trace sink (out-of-memory diagnostics).
    void HeapMalloc::PrintAllocations()
    {
        mAllocator.TraceAllocatedMemory(&TraceFunc, 0, 0, 0);
    }

    // GeneralAllocator::GetLargestFreeBlock(bClearCache): the largest single allocatable block.
    u32 HeapMalloc::GetLargestFreeBlock(bool lbContiguous)
    {
        return static_cast<u32>(mAllocator.GetLargestFreeBlock(lbContiguous));
    }

    // --- HeapMallocCoreAllocator ---------------------------------------------------------------

    // @ 0x82866870 - HeapMalloc::Construct(&mAllocator, lpBuffer, lnBufferSize).
    void HeapMallocCoreAllocator::Construct(void* lpBuffer, s32 lnBufferSize)
    {
        mAllocator.Construct(lpBuffer, lnBufferSize);
    }

    // CgsHeapMalloc.cpp:234 - tear the embedded HeapMalloc down.
    void HeapMallocCoreAllocator::Destruct()
    {
        mAllocator.Destruct();
    }

    // CgsHeapMalloc.cpp:247 - ICoreAllocator::Alloc -> HeapMalloc::Malloc, default alignment.
    // The debug name / flags are ignored by the underlying engine.
    void* HeapMallocCoreAllocator::Alloc(size_t lnSize, const char* /*lpcName*/, unsigned int /*luFlags*/)
    {
        return mAllocator.Malloc(static_cast<s32>(lnSize), HeapMalloc::KI_DEFAULT_ALIGNMENT);
    }

    // CgsHeapMalloc.cpp:263 - ICoreAllocator::Alloc (aligned) -> HeapMalloc::Malloc.
    void* HeapMallocCoreAllocator::Alloc(size_t lnSize, const char* /*lpcName*/, unsigned int /*luFlags*/,
                                         unsigned int luAlignment, unsigned int /*luAlignmentOffset*/)
    {
        return mAllocator.Malloc(static_cast<s32>(lnSize), static_cast<s32>(luAlignment));
    }

    // CgsHeapMalloc.cpp:280 - ICoreAllocator::Free -> HeapMalloc::Free (size is unused by the engine).
    void HeapMallocCoreAllocator::Free(void* lpBlock, size_t /*lnSize*/)
    {
        mAllocator.Free(lpBlock);
    }
}
