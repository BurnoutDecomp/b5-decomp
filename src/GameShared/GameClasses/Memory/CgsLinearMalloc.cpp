#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsMemory::LinearMalloc - bump allocator over a caller-supplied region. Recovered
// Construct resets the region and owned slot without touching the last allocation.
// Create borrows [buffer, buffer+size); it does not claim ownership of that buffer.

namespace CgsMemory
{
    // ARTIST 0x8305F500 and 0x8305F50C: canonical zero-initialized class storage.
    RwMemoryFunctions LinearMalloc::mCachedMemoryFuncs = {};
    LinearMalloc* LinearMalloc::mpActiveRWAllocator = 0;

    // ARTIST 0x828690E0. mpLastAlloc is not part of this reset.
    void LinearMalloc::Construct()
    {
        mpAlloc = 0;
        mbCreated = false;
        mnStartAddress = 0;
        mnEndAddress = 0;
        mnNextAddress = 0;
        mnAlignment = KI_DEFAULT_ALIGNMENT;
    }

    // ARTIST 0x82869108. The assertions continue into borrowed-region stores.
    void LinearMalloc::Create(void* lpBuffer, size_t lnSize)
    {
        if (mpAlloc != 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpAlloc==NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsLinearMalloc.cpp",
                164);
            CgsDev::Assert::EndAssert();
        }
        if (mbCreated)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mbCreated==false",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsLinearMalloc.cpp",
                165);
            CgsDev::Assert::EndAssert();
        }
        if (lpBuffer == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsLinearMalloc.cpp",
                166);
            CgsDev::Assert::EndAssert();
        }

        mnStartAddress = reinterpret_cast<size_t>(lpBuffer);
        mnNextAddress = mnStartAddress;
        mnEndAddress = mnStartAddress + lnSize;
        mnAlignment = KI_DEFAULT_ALIGNMENT;
        mbCreated = true;
    }

    // 0x82866DC0 - round the bump pointer up to mnAlignment, reserve lnSize, and return the
    // aligned address. Advances mnNextAddress even on overflow, returning null in that case.
    void* LinearMalloc::Malloc(size_t lnSize)
    {
        CGS_ASSERT(mbCreated, "mbCreated");   // CgsLinearMalloc.cpp:336

        size_t lnAligned = (mnNextAddress + mnAlignment - 1) & ~(mnAlignment - 1);
        mnNextAddress = lnAligned + lnSize;
        if (mnNextAddress <= mnEndAddress)
        {
            mpLastAlloc = reinterpret_cast<void*>(lnAligned);
            return mpLastAlloc;
        }
        mpLastAlloc = 0;
        return 0;
    }

    // 0x82866E60 - reset the bump pointer to the region start (X360: next = start), freeing
    // every allocation made since Create() without releasing the backing region.
    void LinearMalloc::FreeAll()
    {
        mnNextAddress = mnStartAddress;
    }

    // ARTIST 0x828691C8. Stop restores the table before owned backing is released.
    void LinearMalloc::Destruct()
    {
        if (mpActiveRWAllocator == this)
            StopRWAllocation();

        if (mpAlloc != 0)
        {
            ::rwMemoryFunctions.rwfree(mpAlloc);
            mpAlloc = 0;
        }

        mnStartAddress = 0;
        mnEndAddress = 0;
        mnNextAddress = 0;
        mbCreated = false;
    }

    void LinearMalloc::SetAlignment(size_t lnAlignment)
    {
        mnAlignment = lnAlignment;
    }

    size_t LinearMalloc::GetFreeMemory() const
    {
        return mnEndAddress - mnNextAddress;
    }

    // 0x82866D40 - `return *(this+12) - *(this+4)`: the bump pointer's distance from the
    // region start == bytes consumed so far.
    size_t LinearMalloc::GetUsage() const
    {
        return mnNextAddress - mnStartAddress;
    }

    void* LinearMalloc::GetStartAddress() const
    {
        return reinterpret_cast<void*>(mnStartAddress);
    }

    void LinearMalloc::StartRWAllocation()
    {
    }

    // ARTIST 0x82866C38. Assertion continuation still restores the cached table.
    void LinearMalloc::StopRWAllocation()
    {
        if (mpActiveRWAllocator != this)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "IsRWAllocator()",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\CgsLinearMalloc.cpp",
                245);
            CgsDev::Assert::EndAssert();
        }

        mpActiveRWAllocator = 0;
        ::rwMemoryFunctions = mCachedMemoryFuncs;
    }
}
