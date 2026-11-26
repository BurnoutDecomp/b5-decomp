#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#define CGS_ARTIST_HEAP_MALLOC_FILE \
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsHeapMalloc.cpp"

// CgsMemory::HeapMalloc - the engine's general-purpose heap. A thin wrapper over an embedded
// EA::Allocator::GeneralAllocator (vendor/PPMalloc): Construct initialises the allocator over the
// caller's buffer (adopting it as the allocator's core, with system-alloc growth disabled), and
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
        CGS_ASSERT_AT(lpBuffer,
            "lpBuffer", CGS_ARTIST_HEAP_MALLOC_FILE, 52 /* ARTIST 0x828666B0 */);           // CgsHeapMalloc.cpp:52
        CGS_ASSERT_AT(lnBufferSize > 0,
            "lnBufferSize>0", CGS_ARTIST_HEAP_MALLOC_FILE, 53 /* ARTIST 0x828666D4 */); // CgsHeapMalloc.cpp:53

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

    // ARTIST 0x828667B8, called by the heap allocation trace.
    static void TraceFunc(const char* lpcRecord, void*)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << lpcRecord << "\n";
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

    // @ 0x827EF3F0 -- out-of-line virtual destructor; the host compiler synthesises the
    // scalar/vector deleting-destructor thunk from it. The X360 vector-deleting-destructor
    // runs the implicit member-destruction chain -- ~HeapMalloc(mAllocator) -> its embedded
    // ~GeneralAllocator (the asm's `~GeneralAllocator(a1+0xC)` call) -- restores the
    // ICoreAllocator/HeapMallocCoreAllocator vtable (off_8200F5B4) at +0x0, then conditionally
    // `operator delete(this)` when the low bit of the flag arg is set. Member destruction is
    // implicit and the host `delete` stands in for the deleting tail, so the body is empty.
    HeapMallocCoreAllocator::~HeapMallocCoreAllocator()
    {
    }

    // CgsHeapMalloc.cpp:247.
    void* HeapMallocCoreAllocator::Alloc(size_t lnSize, const char*, unsigned int)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Alloc: " << lnSize << "bytes\n";

        void* lpMemory = mAllocator.GetAllocator()->MallocAligned(
            lnSize, HeapMalloc::KI_DEFAULT_ALIGNMENT, 0, 0);
        CGS_ASSERT_STREAM_AT(lpMemory, "Allocation of: " << lnSize << "bytes failed\n",
            CGS_ARTIST_HEAP_MALLOC_FILE, 251);
        return lpMemory;
    }

    // CgsHeapMalloc.cpp:263.
    void* HeapMallocCoreAllocator::Alloc(size_t lnSize, const char*, unsigned int,
                                         unsigned int luAlignment, unsigned int luAlignmentOffset)
    {
        CGS_ASSERT_STREAM_AT(luAlignmentOffset == 0,
            "Don't currently support offseted alignments in heap malloc interface\n",
            CGS_ARTIST_HEAP_MALLOC_FILE, 265);

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Alloc aligned: " << lnSize << "bytes at " << luAlignment << " alignment\n";

        void* lpMemory = mAllocator.GetAllocator()->MallocAligned(lnSize, luAlignment, 0, 0);
        CGS_ASSERT_STREAM_AT(lpMemory, "Allocation aligned of " << lnSize << "bytes at "
            << luAlignment << " alignment failed\n", CGS_ARTIST_HEAP_MALLOC_FILE, 268);
        return lpMemory;
    }

    // CgsHeapMalloc.cpp:280.
    void HeapMallocCoreAllocator::Free(void* lpBlock, size_t lnSize)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Free " << lnSize << "bytes\n";
        mAllocator.GetAllocator()->Free(lpBlock);
    }
}

#undef CGS_ARTIST_HEAP_MALLOC_FILE
